#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Run one command with this checkout's generated Conan environment."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import Dict, List, Mapping, Optional, Sequence


class LauncherError(RuntimeError):
    """Describes a launcher configuration or environment activation failure."""


def repository_root() -> Path:
    return Path(__file__).resolve().parent.parent


def resolve_generators_directory(
    root: Path, build_type: str, explicit_directory: Optional[str]
) -> Path:
    root = root.resolve()
    if explicit_directory:
        explicit_path = Path(explicit_directory)
        if not explicit_path.is_absolute():
            explicit_path = root / explicit_path
        candidates = [explicit_path]
    else:
        candidates = [
            root / "build" / build_type / "generators",
            root / "build" / "generators",
        ]

    for candidate in candidates:
        resolved = candidate.resolve()
        try:
            resolved.relative_to(root)
        except ValueError as error:
            raise LauncherError(
                f"Conan generators directory must be inside this checkout: {resolved}"
            ) from error
        if resolved.is_dir():
            return resolved

    locations = ", ".join(str(candidate.resolve()) for candidate in candidates)
    raise LauncherError(
        "Conan generators were not found. Run 'conan install' for this checkout "
        f"and build type first. Checked: {locations}"
    )


def activation_scripts(
    generators_directory: Path, include_build_environment: bool
) -> List[Path]:
    suffix = ".bat" if os.name == "nt" else ".sh"
    names = [f"conanbuild{suffix}"] if include_build_environment else []
    names.append(f"conanrun{suffix}")
    scripts = [generators_directory / name for name in names]
    missing = [script for script in scripts if not script.is_file()]
    if missing:
        raise LauncherError(
            "Required Conan environment script is missing: "
            + ", ".join(str(path) for path in missing)
        )
    return scripts


def _activation_environment(scripts: Sequence[Path]) -> Dict[str, str]:
    environment = dict(os.environ)
    variable_names = []
    for index, script in enumerate(scripts):
        name = f"GOLDENDICT_CONAN_ENV_SCRIPT_{index}"
        environment[name] = str(script)
        variable_names.append(name)

    if os.name == "nt":
        commands = ["@echo off"]
        for name in variable_names:
            commands.extend(
                [
                    f'call "%{name}%" >nul',
                    "if errorlevel 1 exit /b %errorlevel%",
                ]
            )
        commands.append("set")
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".bat", encoding="ascii", delete=False
        ) as wrapper:
            wrapper.write("\r\n".join(commands) + "\r\n")
            wrapper_path = Path(wrapper.name)
        try:
            completed = subprocess.run(
                [
                    environment.get("COMSPEC", "cmd.exe"),
                    "/d",
                    "/u",
                    "/c",
                    str(wrapper_path),
                ],
                check=False,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        finally:
            wrapper_path.unlink(missing_ok=True)
        output = completed.stdout.decode("utf-16-le", errors="replace")
        errors = completed.stderr.decode("utf-16-le", errors="replace").strip()
        entries = output.splitlines()
    else:
        commands = [f'. "${name}" >/dev/null' for name in variable_names]
        commands.append("env -0")
        completed = subprocess.run(
            ["/bin/sh", "-c", " && ".join(commands)],
            check=False,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        encoding = sys.getfilesystemencoding()
        entries = completed.stdout.decode(
            encoding, errors="surrogateescape"
        ).split("\0")
        errors = completed.stderr.decode(
            encoding, errors="surrogateescape"
        ).strip()

    if completed.returncode != 0:
        detail = f": {errors}" if errors else ""
        raise LauncherError(
            f"Conan environment activation failed with exit code "
            f"{completed.returncode}{detail}"
        )

    activated: Dict[str, str] = {}
    for entry in entries:
        if not entry or entry.startswith("=") or "=" not in entry:
            continue
        name, value = entry.split("=", 1)
        activated[name] = value
    for name in variable_names:
        activated.pop(name, None)
    if not activated:
        raise LauncherError("Conan environment activation produced no environment.")
    return activated


def run_command(
    command: Sequence[str], root: Path, environment: Mapping[str, str]
) -> int:
    resolved_command = list(command)
    executable = Path(resolved_command[0])
    if not executable.is_absolute() and executable.parent == Path("."):
        search_path = next(
            (
                value
                for name, value in environment.items()
                if name.casefold() == "path"
            ),
            None,
        )
        resolved = shutil.which(resolved_command[0], path=search_path)
        if resolved:
            resolved_command[0] = resolved
    try:
        completed = subprocess.run(
            resolved_command, check=False, cwd=root, env=dict(environment)
        )
    except OSError as error:
        raise LauncherError(f"Unable to start '{command[0]}': {error}") from error
    return completed.returncode


def parse_arguments(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run a command from the repository root after loading this "
            "checkout's generated Conan runtime environment."
        )
    )
    parser.add_argument(
        "--build-type", choices=("Debug", "Release"), default="Release"
    )
    parser.add_argument(
        "--generators-dir",
        help=(
            "Generators directory inside this checkout. By default the launcher "
            "checks build/<build-type>/generators and build/generators."
        ),
    )
    parser.add_argument(
        "--with-build-environment",
        action="store_true",
        help="Load conanbuild before conanrun for configure or build commands.",
    )
    parser.add_argument("command", nargs=argparse.REMAINDER)
    parsed = parser.parse_args(arguments)
    if parsed.command and parsed.command[0] == "--":
        parsed.command = parsed.command[1:]
    if not parsed.command:
        parser.error("a command is required after '--'")
    return parsed


def main(arguments: Optional[Sequence[str]] = None) -> int:
    parsed = parse_arguments(sys.argv[1:] if arguments is None else arguments)
    root = repository_root()
    try:
        generators = resolve_generators_directory(
            root, parsed.build_type, parsed.generators_dir
        )
        scripts = activation_scripts(generators, parsed.with_build_environment)
        environment = _activation_environment(scripts)
        print(f"[conan-env] {generators}", flush=True)
        return run_command(parsed.command, root, environment)
    except LauncherError as error:
        print(f"run_with_conan: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
