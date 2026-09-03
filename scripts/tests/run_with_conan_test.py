#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import run_with_conan


class RunWithConanTest(unittest.TestCase):
    def _write_environment_script(
        self, directory: Path, name: str, variable: str, value: str
    ) -> None:
        suffix = ".bat" if os.name == "nt" else ".sh"
        if os.name == "nt":
            content = f'@echo off\r\nset "{variable}={value}"\r\n'
        else:
            content = f"export {variable}='{value}'\n"
        (directory / f"{name}{suffix}").write_text(content, encoding="utf-8")

    def test_resolves_build_type_specific_directory_first(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            typed = root / "build" / "Release" / "generators"
            fallback = root / "build" / "generators"
            typed.mkdir(parents=True)
            fallback.mkdir(parents=True)

            resolved = run_with_conan.resolve_generators_directory(
                root, "Release", None
            )

            self.assertEqual(typed.resolve(), resolved)

    def test_rejects_explicit_directory_outside_checkout(self) -> None:
        with tempfile.TemporaryDirectory() as root_directory:
            with tempfile.TemporaryDirectory() as outside_directory:
                with self.assertRaises(run_with_conan.LauncherError):
                    run_with_conan.resolve_generators_directory(
                        Path(root_directory), "Release", outside_directory
                    )

    def test_resolves_relative_explicit_directory_from_checkout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            generators = root / "custom" / "generators"
            generators.mkdir(parents=True)

            resolved = run_with_conan.resolve_generators_directory(
                root, "Release", "custom/generators"
            )

            self.assertEqual(generators.resolve(), resolved)

    def test_loads_runtime_and_optional_build_environment(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            generators = Path(temporary_directory)
            self._write_environment_script(
                generators, "conanbuild", "GD_LAUNCHER_BUILD", "active"
            )
            self._write_environment_script(
                generators, "conanrun", "GD_LAUNCHER_RUNTIME", "active"
            )

            runtime_only = run_with_conan._activation_environment(
                run_with_conan.activation_scripts(generators, False)
            )
            build_and_runtime = run_with_conan._activation_environment(
                run_with_conan.activation_scripts(generators, True)
            )

            self.assertEqual("active", runtime_only["GD_LAUNCHER_RUNTIME"])
            self.assertNotIn("GD_LAUNCHER_BUILD", runtime_only)
            self.assertEqual("active", build_and_runtime["GD_LAUNCHER_BUILD"])
            self.assertEqual("active", build_and_runtime["GD_LAUNCHER_RUNTIME"])

    def test_command_inherits_environment_and_exit_code(self) -> None:
        environment = dict(os.environ)
        environment["GD_LAUNCHER_CHILD"] = "active"
        command = [
            sys.executable,
            "-c",
            "import os, sys; sys.exit(7 if os.environ.get('GD_LAUNCHER_CHILD') "
            "== 'active' else 8)",
        ]

        exit_code = run_with_conan.run_command(
            command, Path(__file__).resolve().parent, environment
        )

        self.assertEqual(7, exit_code)

    def test_command_resolution_uses_activated_path(self) -> None:
        environment = {"Path": "C:\\conan\\bin"}
        completed = mock.Mock(returncode=0)
        with mock.patch.object(
            run_with_conan.shutil,
            "which",
            return_value="C:\\conan\\bin\\cmake.exe",
        ) as which:
            with mock.patch.object(
                run_with_conan.subprocess, "run", return_value=completed
            ) as run:
                exit_code = run_with_conan.run_command(
                    ["cmake", "--version"], Path.cwd(), environment
                )

        self.assertEqual(0, exit_code)
        which.assert_called_once_with("cmake", path="C:\\conan\\bin")
        self.assertEqual(
            "C:\\conan\\bin\\cmake.exe", run.call_args.args[0][0]
        )


if __name__ == "__main__":
    unittest.main()
