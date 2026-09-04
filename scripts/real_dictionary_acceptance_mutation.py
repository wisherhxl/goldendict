#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Apply bounded disposable-corpus lifecycle mutations around an observer."""

from __future__ import annotations

import argparse
import os
import stat
import subprocess
import tempfile
import time
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

DISPOSABLE_MARKER = ".goldendict-disposable-acceptance-v1"
MARKER_CONTENT = "Generated disposable GoldenDict acceptance corpus.\n"
MAX_MUTABLE_COMPONENT_BYTES = 64 * 1024 * 1024
SCENARIOS = (
    "clean-discovery",
    "changed-source",
    "unavailable-companion",
    "companion-recovery",
)


class MutationError(RuntimeError):
    """Raised when a requested acceptance mutation is unsafe or incomplete."""


def _is_reparse_point(path: Path) -> bool:
    try:
        attributes = getattr(path.stat(follow_symlinks=False), "st_file_attributes", 0)
    except OSError as error:
        raise MutationError(f"Cannot inspect disposable corpus path: {path}") from error
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return bool(attributes & reparse_flag)


def _relative_component(value: str, label: str) -> PurePosixPath:
    if not value or "\\" in value or ":" in value:
        raise MutationError(f"{label} must be a normalized relative path")
    path = PurePosixPath(value)
    if (
        path.is_absolute()
        or value.startswith("//")
        or path.as_posix() != value
        or any(part in ("", ".", "..") for part in path.parts)
    ):
        raise MutationError(f"{label} must be normalized and confined")
    return path


def _regular_component(root: Path, value: str, label: str) -> Path:
    relative = _relative_component(value, label)
    current = root
    for part in relative.parts:
        current = current / part
        if current.is_symlink() or _is_reparse_point(current):
            raise MutationError(f"{label} must not traverse a link or reparse point")
    try:
        metadata = current.stat(follow_symlinks=False)
        resolved = current.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError) as error:
        raise MutationError(f"{label} is not a confined corpus file") from error
    if not stat.S_ISREG(metadata.st_mode):
        raise MutationError(f"{label} must be a regular file")
    if metadata.st_size > MAX_MUTABLE_COMPONENT_BYTES:
        raise MutationError(f"{label} exceeds the disposable mutation size bound")
    return resolved


def _validated_root(dictionary_root: Path) -> Path:
    try:
        root = dictionary_root.resolve(strict=True)
    except OSError as error:
        raise MutationError("Disposable corpus root does not exist") from error
    if not root.is_dir() or root.is_symlink() or _is_reparse_point(root):
        raise MutationError("Disposable corpus root must be a real directory")
    expected = os.environ.get("GOLDENDICT_ACCEPTANCE_CORPUS_ROOT")
    if not expected:
        raise MutationError("Paired acceptance corpus binding is missing")
    try:
        if Path(expected).resolve(strict=True) != root:
            raise MutationError("Disposable corpus root does not match the paired run")
    except OSError as error:
        raise MutationError("Paired acceptance corpus root does not exist") from error
    marker = root / DISPOSABLE_MARKER
    if not marker.exists():
        raise MutationError("Disposable corpus marker is missing")
    if marker.is_symlink() or _is_reparse_point(marker) or not marker.is_file():
        raise MutationError("Disposable corpus marker must be a regular file")
    try:
        if marker.read_text(encoding="utf-8") != MARKER_CONTENT:
            raise MutationError("Disposable corpus marker is invalid")
    except (OSError, UnicodeError) as error:
        raise MutationError("Cannot read disposable corpus marker") from error
    return root


def _touch_source(path: Path) -> None:
    try:
        before = path.stat(follow_symlinks=False)
        modified = max(before.st_mtime_ns, time.time_ns()) + 2_000_000_000
        os.utime(path, ns=(before.st_atime_ns, modified))
        after = path.stat(follow_symlinks=False)
    except OSError as error:
        raise MutationError("Cannot update disposable source timestamp") from error
    if after.st_mtime_ns == before.st_mtime_ns or after.st_size != before.st_size:
        raise MutationError("Disposable source timestamp did not change safely")


def _validate_child_command(command: list[str]) -> None:
    if not command:
        raise MutationError("Observer command is empty")
    if any(item == "--scenario" or item.startswith("--scenario=") for item in command):
        raise MutationError("Observer command must not override the mutation scenario")


def _run_child(command: list[str], scenario: str) -> int:
    try:
        return subprocess.run(
            [*command, "--scenario", scenario], check=False
        ).returncode
    except OSError as error:
        raise MutationError("Observer command could not be launched") from error


def run_mutation(
    dictionary_root: Path,
    source_component: str,
    companion_component: str,
    scenario: str,
    command: list[str],
) -> int:
    if scenario not in SCENARIOS:
        raise MutationError("Mutation adapter received an unsupported scenario")
    _validate_child_command(command)
    root = _validated_root(dictionary_root)
    source = _regular_component(root, source_component, "Mutable source component")
    companion = _regular_component(
        root, companion_component, "Unavailable companion component"
    )
    if source == companion:
        raise MutationError("Source and companion components must be distinct")

    if scenario == "changed-source":
        _touch_source(source)
        return _run_child(command, scenario)
    if scenario != "unavailable-companion":
        return _run_child(command, scenario)

    evidence_value = os.environ.get("GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT")
    if not evidence_value:
        raise MutationError("Paired acceptance evidence binding is missing")
    try:
        evidence_root = Path(evidence_value).resolve(strict=True)
    except OSError as error:
        raise MutationError("Paired acceptance evidence root does not exist") from error
    if (
        root == evidence_root
        or root in evidence_root.parents
        or evidence_root in root.parents
    ):
        raise MutationError("Mutation quarantine must be outside the disposable corpus")

    quarantine_root = Path(
        tempfile.mkdtemp(prefix=".unavailable-companion-", dir=evidence_root)
    )
    quarantine = quarantine_root / companion.name
    moved = False
    try:
        os.replace(companion, quarantine)
        moved = True
        return _run_child(command, scenario)
    except OSError as error:
        raise MutationError("Cannot quarantine unavailable companion") from error
    finally:
        try:
            if moved:
                os.replace(quarantine, companion)
            quarantine_root.rmdir()
        except OSError as error:
            raise MutationError("Cannot restore unavailable companion") from error


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disposable-corpus", type=Path, required=True)
    parser.add_argument("--source-component", required=True)
    parser.add_argument("--companion-component", required=True)
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    parser.add_argument("child_command", nargs=argparse.REMAINDER)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    command = options.child_command
    if command and command[0] == "--":
        command = command[1:]
    try:
        return run_mutation(
            options.disposable_corpus,
            options.source_component,
            options.companion_component,
            options.scenario,
            command,
        )
    except MutationError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
