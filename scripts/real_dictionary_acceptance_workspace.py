#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Create and validate isolated paired Qt 5/Qt 6 acceptance workspaces."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from typing import Iterable

import real_dictionary_manifest


PAIR_SCHEMA = "goldendict-real-dictionary-acceptance-pair-v1"
RUN_SCHEMA = "goldendict-real-dictionary-acceptance-run-v1"
CONDITIONS_SCHEMA = "goldendict-real-dictionary-conditions-v1"
ACKNOWLEDGEMENT_SCHEMA = "goldendict-real-dictionary-condition-ack-v1"
MAX_CONTROL_METADATA_BYTES = 1024 * 1024
MAX_MANIFEST_BYTES = 64 * 1024 * 1024
VERSIONS = ("qt5", "qt6")
DIRECTORY_NAMES = (
    "home",
    "config",
    "data",
    "cache",
    "indexes",
    "logs",
    "evidence",
    "temp",
    "windows-config",
)
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")
HASH_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class WorkspaceError(RuntimeError):
    """Raised when an isolated acceptance workspace is unsafe or inconsistent."""


def _canonical_json(value: object) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("utf-8")


def _sha256_bytes(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def _read_json(
    path: Path, maximum_bytes: int = MAX_CONTROL_METADATA_BYTES
) -> tuple[dict[str, object], bytes]:
    try:
        content = path.read_bytes()
    except OSError as error:
        raise WorkspaceError(f"Cannot read metadata: {path}") from error
    if len(content) > maximum_bytes:
        raise WorkspaceError(f"Metadata exceeds {maximum_bytes} bytes: {path}")
    try:
        value = json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise WorkspaceError(f"Invalid UTF-8 JSON metadata: {path}") from error
    if not isinstance(value, dict):
        raise WorkspaceError(f"Metadata root must be an object: {path}")
    return value, content


def _resolve_metadata_path(path: Path, label: str) -> Path:
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise WorkspaceError(f"{label} does not exist: {path}") from error
    if not resolved.is_file():
        raise WorkspaceError(f"{label} is not a regular file: {resolved}")
    return resolved


def _write_json(path: Path, value: object) -> None:
    path.write_bytes(
        (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode(
            "utf-8"
        )
    )


def _validate_revision(name: str, revision: str) -> None:
    if not REVISION_PATTERN.fullmatch(revision):
        raise WorkspaceError(f"{name} revision must be a lowercase 40-digit commit ID")


def _validate_conditions(conditions: dict[str, object]) -> None:
    if conditions.get("schema") != CONDITIONS_SCHEMA:
        raise WorkspaceError(f"Conditions schema must be {CONDITIONS_SCHEMA}")
    if not isinstance(conditions.get("locale"), str) or not conditions["locale"]:
        raise WorkspaceError("Conditions locale must be a non-empty string")
    for key in ("group", "preferences", "platform"):
        if not isinstance(conditions.get(key), dict):
            raise WorkspaceError(f"Conditions {key} must be an object")
    if not isinstance(conditions.get("queries"), list):
        raise WorkspaceError("Conditions queries must be an array")


def _validate_manifest(manifest: dict[str, object]) -> None:
    if manifest.get("schema") != real_dictionary_manifest.SCHEMA:
        raise WorkspaceError(
            f"Corpus manifest schema must be {real_dictionary_manifest.SCHEMA}"
        )
    if manifest.get("hash_algorithm") != real_dictionary_manifest.HASH_ALGORITHM:
        raise WorkspaceError("Corpus manifest must use SHA-256")
    if not isinstance(manifest.get("file_count"), int) or manifest["file_count"] < 0:
        raise WorkspaceError("Corpus manifest file_count must be non-negative")
    if not isinstance(manifest.get("total_bytes"), int) or manifest["total_bytes"] < 0:
        raise WorkspaceError("Corpus manifest total_bytes must be non-negative")
    if not isinstance(manifest.get("files"), list):
        raise WorkspaceError("Corpus manifest files must be an array")
    if len(manifest["files"]) != manifest["file_count"]:
        raise WorkspaceError("Corpus manifest file_count does not match files")


def _validate_corpus_matches_manifest(
    corpus_root: Path, manifest: dict[str, object]
) -> None:
    try:
        current_manifest = real_dictionary_manifest.build_manifest(corpus_root)
    except real_dictionary_manifest.ManifestError as error:
        raise WorkspaceError(f"Cannot verify corpus against manifest: {error}") from error
    if current_manifest != manifest:
        raise WorkspaceError("Corpus content does not match the supplied manifest")


def _resolved_new_workspace(workspace: Path, corpus_root: Path) -> Path:
    if workspace.exists() or workspace.is_symlink():
        raise WorkspaceError(f"Workspace already exists: {workspace}")
    try:
        parent = workspace.parent.resolve(strict=True)
    except OSError as error:
        raise WorkspaceError(f"Workspace parent does not exist: {workspace.parent}") from error
    resolved = parent / workspace.name
    overlaps_corpus = real_dictionary_manifest.is_within(
        resolved, corpus_root
    ) or real_dictionary_manifest.is_within(corpus_root, resolved)
    if overlaps_corpus:
        raise WorkspaceError("Workspace and corpus directories must be disjoint")
    return resolved


def _expected_environment(version_root: Path) -> dict[str, str]:
    directories = {name: str((version_root / name).resolve()) for name in DIRECTORY_NAMES}
    return {
        "APPDATA": directories["config"],
        "GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT": directories["evidence"],
        "GOLDENDICT_ACCEPTANCE_INDEX_ROOT": directories["indexes"],
        "GOLDENDICT_ACCEPTANCE_LOG_ROOT": directories["logs"],
        "GOLDENDICT_TEST_CONFIG_ROOT": directories["windows-config"],
        "HOME": directories["home"],
        "LOCALAPPDATA": directories["data"],
        "TEMP": directories["temp"],
        "TMP": directories["temp"],
        "TMPDIR": directories["temp"],
        "XDG_CACHE_HOME": directories["cache"],
        "XDG_CONFIG_HOME": directories["config"],
        "XDG_DATA_HOME": directories["data"],
    }


def _pair_identity(
    manifest_hash: str,
    conditions_hash: str,
    qt5_revision: str,
    qt6_revision: str,
) -> str:
    identity = {
        "conditions_sha256": conditions_hash,
        "corpus_manifest_sha256": manifest_hash,
        "qt5_revision": qt5_revision,
        "qt6_revision": qt6_revision,
    }
    return _sha256_bytes(_canonical_json(identity))


def _create_tree(
    root: Path,
    final_workspace: Path,
    pair: dict[str, object],
    revisions: dict[str, str],
) -> None:
    for version in VERSIONS:
        version_root = root / version
        for directory_name in DIRECTORY_NAMES:
            (version_root / directory_name).mkdir(parents=True, exist_ok=True)
        final_version_root = final_workspace / version
        environment = _expected_environment(final_version_root)
        corpus_metadata = pair["corpus"]
        if not isinstance(corpus_metadata, dict):
            raise WorkspaceError("Pair corpus metadata must be an object")
        _write_json(version_root / "environment.json", environment)
        _write_json(version_root / "conditions.json", pair["conditions"])
        _write_json(
            version_root / "run.json",
            {
                "conditions_sha256": pair["conditions_sha256"],
                "conditions_file": f"{version}/conditions.json",
                "corpus_manifest_sha256": corpus_metadata["manifest_sha256"],
                "directories": {name: name for name in DIRECTORY_NAMES},
                "environment_file": f"{version}/environment.json",
                "pair_id": pair["pair_id"],
                "revision": revisions[version],
                "schema": RUN_SCHEMA,
                "version": version,
            },
        )
    _write_json(root / "pair.json", pair)


def create_workspace(
    workspace: Path,
    corpus: Path,
    manifest_path: Path,
    conditions_path: Path,
    qt5_revision: str,
    qt6_revision: str,
) -> str:
    _validate_revision("Qt 5", qt5_revision)
    _validate_revision("Qt 6", qt6_revision)
    try:
        corpus_root = real_dictionary_manifest.resolve_corpus(corpus)
    except real_dictionary_manifest.ManifestError as error:
        raise WorkspaceError(str(error)) from error
    final_workspace = _resolved_new_workspace(workspace, corpus_root)

    resolved_manifest = _resolve_metadata_path(manifest_path, "Corpus manifest")
    resolved_conditions = _resolve_metadata_path(
        conditions_path, "Acceptance conditions"
    )
    manifest, manifest_bytes = _read_json(resolved_manifest, MAX_MANIFEST_BYTES)
    conditions, _ = _read_json(resolved_conditions)
    _validate_manifest(manifest)
    _validate_conditions(conditions)
    if real_dictionary_manifest.is_within(resolved_manifest, corpus_root):
        raise WorkspaceError("Corpus manifest must be outside the corpus")
    if real_dictionary_manifest.is_within(resolved_conditions, corpus_root):
        raise WorkspaceError("Acceptance conditions must be outside the corpus")
    _validate_corpus_matches_manifest(corpus_root, manifest)

    manifest_hash = _sha256_bytes(manifest_bytes)
    conditions_hash = _sha256_bytes(_canonical_json(conditions))
    pair_id = _pair_identity(
        manifest_hash, conditions_hash, qt5_revision, qt6_revision
    )
    pair = {
        "conditions": conditions,
        "conditions_sha256": conditions_hash,
        "corpus": {
            "file_count": manifest["file_count"],
            "manifest_sha256": manifest_hash,
            "total_bytes": manifest["total_bytes"],
        },
        "pair_id": pair_id,
        "runs": {
            "qt5": {"metadata": "qt5/run.json", "revision": qt5_revision},
            "qt6": {"metadata": "qt6/run.json", "revision": qt6_revision},
        },
        "schema": PAIR_SCHEMA,
    }

    temporary_root = Path(
        tempfile.mkdtemp(prefix=f".{final_workspace.name}.", dir=final_workspace.parent)
    )
    try:
        _create_tree(
            temporary_root,
            final_workspace,
            pair,
            {"qt5": qt5_revision, "qt6": qt6_revision},
        )
        os.replace(temporary_root, final_workspace)
    except OSError as error:
        raise WorkspaceError(f"Cannot publish workspace: {final_workspace}") from error
    finally:
        if temporary_root.exists():
            shutil.rmtree(temporary_root)
    return pair_id


def _require_string(mapping: dict[str, object], key: str, context: str) -> str:
    value = mapping.get(key)
    if not isinstance(value, str):
        raise WorkspaceError(f"{context} {key} must be a string")
    return value


def validate_workspace(
    workspace: Path,
    corpus: Path,
    manifest_path: Path,
    conditions_path: Path,
    expected_qt5_revision: str,
    expected_qt6_revision: str,
) -> str:
    _validate_revision("Expected Qt 5", expected_qt5_revision)
    _validate_revision("Expected Qt 6", expected_qt6_revision)
    try:
        corpus_root = real_dictionary_manifest.resolve_corpus(corpus)
        workspace_root = workspace.resolve(strict=True)
    except (OSError, real_dictionary_manifest.ManifestError) as error:
        raise WorkspaceError(f"Cannot resolve workspace or corpus: {error}") from error
    if not workspace_root.is_dir():
        raise WorkspaceError("Workspace path is not a directory")
    overlaps_corpus = real_dictionary_manifest.is_within(
        workspace_root, corpus_root
    ) or real_dictionary_manifest.is_within(corpus_root, workspace_root)
    if overlaps_corpus:
        raise WorkspaceError("Workspace and corpus directories must be disjoint")

    pair, _ = _read_json(workspace_root / "pair.json")
    resolved_manifest = _resolve_metadata_path(manifest_path, "Corpus manifest")
    resolved_conditions = _resolve_metadata_path(
        conditions_path, "Acceptance conditions"
    )
    if real_dictionary_manifest.is_within(resolved_manifest, corpus_root):
        raise WorkspaceError("Corpus manifest must be outside the corpus")
    if real_dictionary_manifest.is_within(resolved_conditions, corpus_root):
        raise WorkspaceError("Acceptance conditions must be outside the corpus")
    manifest, manifest_bytes = _read_json(resolved_manifest, MAX_MANIFEST_BYTES)
    conditions, _ = _read_json(resolved_conditions)
    _validate_manifest(manifest)
    _validate_conditions(conditions)
    _validate_corpus_matches_manifest(corpus_root, manifest)
    if pair.get("schema") != PAIR_SCHEMA:
        raise WorkspaceError(f"Pair schema must be {PAIR_SCHEMA}")

    manifest_hash = _sha256_bytes(manifest_bytes)
    conditions_hash = _sha256_bytes(_canonical_json(conditions))
    if pair.get("conditions") != conditions:
        raise WorkspaceError("Pair conditions do not match the supplied conditions")
    if pair.get("conditions_sha256") != conditions_hash:
        raise WorkspaceError("Pair conditions hash does not match")
    corpus_metadata = pair.get("corpus")
    if not isinstance(corpus_metadata, dict):
        raise WorkspaceError("Pair corpus metadata must be an object")
    expected_corpus = {
        "file_count": manifest["file_count"],
        "manifest_sha256": manifest_hash,
        "total_bytes": manifest["total_bytes"],
    }
    if corpus_metadata != expected_corpus:
        raise WorkspaceError("Pair corpus metadata does not match the supplied manifest")

    runs = pair.get("runs")
    if not isinstance(runs, dict):
        raise WorkspaceError("Pair runs must be an object")
    revisions: dict[str, str] = {}
    expected_revisions = {
        "qt5": expected_qt5_revision,
        "qt6": expected_qt6_revision,
    }
    for version in VERSIONS:
        run_reference = runs.get(version)
        if not isinstance(run_reference, dict):
            raise WorkspaceError(f"Pair {version} run reference must be an object")
        revision = _require_string(run_reference, "revision", f"Pair {version}")
        _validate_revision(version, revision)
        if revision != expected_revisions[version]:
            raise WorkspaceError(f"Pair {version} revision does not match the expected commit")
        if run_reference != {
            "metadata": f"{version}/run.json",
            "revision": revision,
        }:
            raise WorkspaceError(f"Pair {version} run reference is not canonical")
        revisions[version] = revision

    expected_pair_id = _pair_identity(
        manifest_hash,
        conditions_hash,
        revisions["qt5"],
        revisions["qt6"],
    )
    if pair.get("pair_id") != expected_pair_id:
        raise WorkspaceError("Pair identity does not match its inputs")

    directory_sets: dict[str, set[Path]] = {}
    for version in VERSIONS:
        version_root = (workspace_root / version).resolve(strict=True)
        if not real_dictionary_manifest.is_within(version_root, workspace_root):
            raise WorkspaceError(f"{version} run directory escapes the workspace")
        run, _ = _read_json(version_root / "run.json")
        expected_run = {
            "conditions_sha256": conditions_hash,
            "conditions_file": f"{version}/conditions.json",
            "corpus_manifest_sha256": manifest_hash,
            "directories": {name: name for name in DIRECTORY_NAMES},
            "environment_file": f"{version}/environment.json",
            "pair_id": expected_pair_id,
            "revision": revisions[version],
            "schema": RUN_SCHEMA,
            "version": version,
        }
        if run != expected_run:
            raise WorkspaceError(f"{version} run metadata does not match the pair")

        projected_conditions, _ = _read_json(version_root / "conditions.json")
        if projected_conditions != conditions:
            raise WorkspaceError(f"{version} projected conditions do not match the pair")

        environment, _ = _read_json(version_root / "environment.json")
        expected_environment = _expected_environment(version_root)
        if environment != expected_environment:
            raise WorkspaceError(f"{version} environment is not isolated as declared")
        for directory_name in DIRECTORY_NAMES:
            directory = version_root / directory_name
            if not directory.is_dir():
                raise WorkspaceError(
                    f"{version} required run directory is missing: {directory_name}"
                )
            resolved_directory = directory.resolve(strict=True)
            if not real_dictionary_manifest.is_within(
                resolved_directory, version_root
            ):
                raise WorkspaceError(
                    f"{version} required run directory escapes: {directory_name}"
                )
        directories = {Path(value).resolve(strict=True) for value in environment.values()}
        if any(
            not real_dictionary_manifest.is_within(path, version_root)
            for path in directories
        ):
            raise WorkspaceError(f"{version} environment escapes its run directory")
        directory_sets[version] = directories

    if directory_sets["qt5"] & directory_sets["qt6"]:
        raise WorkspaceError("Qt 5 and Qt 6 run directories overlap")
    return expected_pair_id


def _validated_dictionary_root_argument(command: list[str], corpus_root: Path) -> None:
    supplied_values: list[str] = []
    for index, value in enumerate(command):
        if value == "--dictionary-root":
            if index + 1 >= len(command):
                raise WorkspaceError(
                    "Acceptance command must contain exactly one --dictionary-root value"
                )
            supplied_values.append(command[index + 1])
        elif value.startswith("--dictionary-root="):
            supplied_values.append(value.partition("=")[2])
    if len(supplied_values) != 1:
        raise WorkspaceError(
            "Acceptance command must contain exactly one --dictionary-root value"
        )
    try:
        supplied = Path(supplied_values[0]).resolve(strict=True)
    except OSError as error:
        raise WorkspaceError("Acceptance command dictionary root does not exist") from error
    if supplied != corpus_root:
        raise WorkspaceError("Acceptance command dictionary root does not match --corpus")


def run_in_workspace(
    workspace: Path,
    corpus: Path,
    manifest_path: Path,
    conditions_path: Path,
    expected_qt5_revision: str,
    expected_qt6_revision: str,
    version: str,
    command: list[str],
) -> int:
    pair_id = validate_workspace(
        workspace,
        corpus,
        manifest_path,
        conditions_path,
        expected_qt5_revision,
        expected_qt6_revision,
    )
    if version not in VERSIONS:
        raise WorkspaceError(f"Version must be one of: {', '.join(VERSIONS)}")
    if not command:
        raise WorkspaceError("Run command must not be empty")

    workspace_root = workspace.resolve(strict=True)
    try:
        corpus_root = real_dictionary_manifest.resolve_corpus(corpus)
    except real_dictionary_manifest.ManifestError as error:
        raise WorkspaceError(str(error)) from error
    _validated_dictionary_root_argument(command, corpus_root)
    version_root = workspace_root / version
    projected_environment, _ = _read_json(version_root / "environment.json")
    if not all(
        isinstance(key, str) and isinstance(value, str)
        for key, value in projected_environment.items()
    ):
        raise WorkspaceError(f"{version} environment must contain string values")
    environment = dict(os.environ)
    environment.update(
        {
            key: value
            for key, value in projected_environment.items()
            if isinstance(key, str) and isinstance(value, str)
        }
    )
    pair, _ = _read_json(workspace_root / "pair.json")
    conditions_hash = _require_string(
        pair, "conditions_sha256", "Pair"
    )
    corpus_metadata = pair.get("corpus")
    if not isinstance(corpus_metadata, dict):
        raise WorkspaceError("Pair corpus metadata must be an object")
    manifest_hash = _require_string(
        corpus_metadata, "manifest_sha256", "Pair corpus"
    )
    revision = (
        expected_qt5_revision if version == "qt5" else expected_qt6_revision
    )
    acknowledgement_path = version_root / "evidence" / "condition-ack.json"
    try:
        acknowledgement_path.unlink(missing_ok=True)
    except OSError as error:
        raise WorkspaceError("Cannot clear the prior condition acknowledgement") from error
    environment.update(
        {
            "GOLDENDICT_ACCEPTANCE_ACK_PATH": str(acknowledgement_path),
            "GOLDENDICT_ACCEPTANCE_CONDITIONS_FILE": str(
                version_root / "conditions.json"
            ),
            "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256": conditions_hash,
            "GOLDENDICT_ACCEPTANCE_CORPUS_MANIFEST_SHA256": manifest_hash,
            "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT": str(corpus_root),
            "GOLDENDICT_ACCEPTANCE_PAIR_ID": pair_id,
            "GOLDENDICT_ACCEPTANCE_REVISION": revision,
            "GOLDENDICT_ACCEPTANCE_VERSION": version,
        }
    )
    try:
        completed = subprocess.run(
            command,
            cwd=version_root,
            env=environment,
            check=False,
        )
    except OSError as error:
        raise WorkspaceError(f"Cannot start acceptance command: {command[0]}") from error
    post_run_pair_id = validate_workspace(
        workspace,
        corpus,
        manifest_path,
        conditions_path,
        expected_qt5_revision,
        expected_qt6_revision,
    )
    if post_run_pair_id != pair_id:
        raise WorkspaceError("Acceptance pair identity changed while the command ran")
    if completed.returncode == 0:
        if not acknowledgement_path.is_file():
            raise WorkspaceError(
                "Acceptance command did not acknowledge the exact paired conditions"
            )
        acknowledgement, _ = _read_json(acknowledgement_path)
        expected_acknowledgement = {
            "conditions_sha256": conditions_hash,
            "corpus_manifest_sha256": manifest_hash,
            "pair_id": pair_id,
            "revision": revision,
            "schema": ACKNOWLEDGEMENT_SCHEMA,
            "version": version,
        }
        if acknowledgement != expected_acknowledgement:
            raise WorkspaceError(
                "Acceptance command did not acknowledge the exact paired conditions"
            )
    return completed.returncode


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("create", "validate", "run"):
        subparser = subparsers.add_parser(command)
        subparser.add_argument("--workspace", type=Path, required=True)
        subparser.add_argument("--corpus", type=Path, required=True)
        subparser.add_argument("--manifest", type=Path, required=True)
        subparser.add_argument("--conditions", type=Path, required=True)
        subparser.add_argument("--qt5-revision", required=True)
        subparser.add_argument("--qt6-revision", required=True)
        if command == "run":
            subparser.add_argument("--version", choices=VERSIONS, required=True)
            subparser.add_argument("child_command", nargs=argparse.REMAINDER)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        if options.command == "create":
            pair_id = create_workspace(
                options.workspace,
                options.corpus,
                options.manifest,
                options.conditions,
                options.qt5_revision,
                options.qt6_revision,
            )
        elif options.command == "validate":
            pair_id = validate_workspace(
                options.workspace,
                options.corpus,
                options.manifest,
                options.conditions,
                options.qt5_revision,
                options.qt6_revision,
            )
        else:
            child_command = options.child_command
            if child_command and child_command[0] == "--":
                child_command = child_command[1:]
            return run_in_workspace(
                options.workspace,
                options.corpus,
                options.manifest,
                options.conditions,
                options.qt5_revision,
                options.qt6_revision,
                options.version,
                child_command,
            )
    except WorkspaceError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    print(pair_id)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
