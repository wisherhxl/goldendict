#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from contextlib import nullcontext
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import prepare_qt5_acceptance_source as preparation
import qt5_real_dictionary_observer as observer

CONDITIONS_HASH = "a" * 64


class Qt5RealDictionaryObserverTest(unittest.TestCase):
    def _fixture(self, root: Path) -> tuple[dict[str, str], dict[str, Path]]:
        corpus = root / "corpus & dictionaries"
        index = root / "index"
        config = root / "config"
        evidence = root / "evidence"
        raw_directory = evidence / ".raw-observation-fixture"
        runtime = root / "runtime"
        plugins = root / "plugins"
        for directory in (
            corpus,
            index,
            config,
            evidence,
            raw_directory,
            runtime,
            plugins / "platforms",
        ):
            directory.mkdir(parents=True, exist_ok=True)
        executable = root / "legacy" / "GoldenDict.exe"
        executable.parent.mkdir()
        executable.write_bytes(b"fixture executable")
        include = Path(observer.__file__).with_name(preparation.OBSERVER_INCLUDE)
        prepared_source = root / "prepared-source"
        prepared_source.mkdir()
        for name in preparation.INSTRUMENTED_FILES:
            target = prepared_source / name
            if name == preparation.OBSERVER_INCLUDE:
                target.write_bytes(include.read_bytes())
            else:
                target.write_text(f"instrumented {name}\n", encoding="utf-8")
        provenance = prepared_source / preparation.PROVENANCE_FILE
        provenance.write_text(
            json.dumps(
                {
                    "instrumented_files": {
                        name: hashlib.sha256(
                            (prepared_source / name).read_bytes()
                        ).hexdigest()
                        for name in preparation.INSTRUMENTED_FILES
                    },
                    "observer_include_sha256": hashlib.sha256(
                        include.read_bytes()
                    ).hexdigest(),
                    "revision": preparation.FROZEN_REVISION,
                    "schema": preparation.PROVENANCE_SCHEMA,
                    "source_tree": preparation.FROZEN_TREE,
                }
            ),
            encoding="utf-8",
        )
        environment = {
            "APPDATA": str(config),
            "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256": CONDITIONS_HASH,
            "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT": str(corpus),
            "GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT": str(evidence),
            "GOLDENDICT_ACCEPTANCE_INDEX_ROOT": str(index),
            "GOLDENDICT_ACCEPTANCE_REVISION": preparation.FROZEN_REVISION,
            "GOLDENDICT_ACCEPTANCE_VERSION": "qt5",
        }
        paths = {
            "config": config,
            "corpus": corpus,
            "evidence": evidence,
            "executable": executable,
            "index": index,
            "output": raw_directory / "raw.json",
            "plugins": plugins,
            "provenance": provenance,
            "runtime": runtime,
        }
        return environment, paths

    @staticmethod
    def _call(paths: dict[str, Path], scenario: str = "clean-discovery") -> Path:
        return observer.observe(
            paths["executable"],
            paths["provenance"],
            [paths["runtime"]],
            paths["plugins"],
            paths["corpus"],
            paths["index"],
            "en_US",
            CONDITIONS_HASH,
            scenario,
            paths["output"],
            60,
        )

    def test_runs_with_isolated_profile_runtime_and_plugins(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            captured: dict[str, object] = {}

            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                child_environment = kwargs["env"]
                assert isinstance(child_environment, dict)
                captured["command"] = command
                captured["environment"] = child_environment
                config = Path(child_environment["APPDATA"]) / "GoldenDict" / "config"
                captured["config"] = config.read_bytes()
                Path(
                    child_environment["GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH"]
                ).write_text("{}", encoding="utf-8")
                return subprocess.CompletedProcess(command, 0)

            with (
                mock.patch.dict(os.environ, environment, clear=False),
                mock.patch.object(observer.platform, "system", return_value="Windows"),
                mock.patch.object(
                    observer,
                    "_suppressed_windows_error_dialogs",
                    return_value=nullcontext(),
                ),
                mock.patch.object(observer.subprocess, "run", side_effect=run),
            ):
                self.assertEqual(self._call(paths), paths["output"])

            child_environment = captured["environment"]
            assert isinstance(child_environment, dict)
            self.assertEqual(child_environment["QT_QPA_PLATFORM"], "windows")
            self.assertEqual(child_environment["QT_OPENGL"], "software")
            self.assertEqual(child_environment["QT_PLUGIN_PATH"], str(paths["plugins"]))
            self.assertTrue(
                child_environment["PATH"].startswith(str(paths["executable"].parent))
            )
            config = ET.fromstring(captured["config"])
            self.assertEqual(config.findtext("paths/path"), str(paths["corpus"]))
            self.assertEqual(config.findtext("preferences/interfaceLanguage"), "en_US")
            self.assertEqual(captured["command"], [str(paths["executable"])])

    def test_reports_and_removes_gui_subsystem_failure_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            diagnostic = paths["output"].with_name(paths["output"].name + ".failure")

            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                diagnostic.write_text(
                    "post-rescan dictionary selection\n", encoding="utf-8"
                )
                return subprocess.CompletedProcess(command, 4)

            with (
                mock.patch.dict(os.environ, environment, clear=False),
                mock.patch.object(
                    observer,
                    "_suppressed_windows_error_dialogs",
                    return_value=nullcontext(),
                ),
                mock.patch.object(observer.subprocess, "run", side_effect=run),
                self.assertRaisesRegex(
                    observer.Qt5ObserverError,
                    "exit code 4: post-rescan dictionary selection",
                ),
            ):
                self._call(paths)

            self.assertFalse(diagnostic.exists())

    def test_projects_hash_bound_mdict_catalog(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            catalog = paths["evidence"] / "catalog.json"
            catalog.write_text('{"schema":"fixture"}\n', encoding="utf-8")
            catalog_hash = hashlib.sha256(catalog.read_bytes()).hexdigest()
            captured: dict[str, str] = {}

            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                child_environment = kwargs["env"]
                assert isinstance(child_environment, dict)
                captured.update(child_environment)
                Path(
                    child_environment["GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH"]
                ).write_text("{}", encoding="utf-8")
                return subprocess.CompletedProcess(command, 0)

            with (
                mock.patch.dict(os.environ, environment, clear=False),
                mock.patch.object(
                    observer,
                    "_suppressed_windows_error_dialogs",
                    return_value=nullcontext(),
                ),
                mock.patch.object(observer.subprocess, "run", side_effect=run),
            ):
                observer.observe(
                    paths["executable"],
                    paths["provenance"],
                    [paths["runtime"]],
                    paths["plugins"],
                    paths["corpus"],
                    paths["index"],
                    "en_US",
                    CONDITIONS_HASH,
                    "clean-discovery",
                    paths["output"],
                    60,
                    mdict_catalog=catalog,
                    mdict_catalog_sha256=catalog_hash,
                )

            self.assertEqual(
                captured["GOLDENDICT_ACCEPTANCE_MDICT_CATALOG"], str(catalog)
            )
            self.assertEqual(
                captured["GOLDENDICT_ACCEPTANCE_MDICT_CATALOG_SHA256"],
                catalog_hash,
            )

    def test_management_catalog_reuses_one_profile_for_clean_and_warm_runs(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            catalog = paths["evidence"] / "management-catalog.json"
            catalog.write_text('{"schema":"fixture"}\n', encoding="utf-8")
            catalog_hash = hashlib.sha256(catalog.read_bytes()).hexdigest()
            appdata_paths: list[str] = []
            configuration_contents: list[bytes] = []

            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                child_environment = kwargs["env"]
                assert isinstance(child_environment, dict)
                appdata_paths.append(child_environment["APPDATA"])
                configuration = (
                    Path(child_environment["APPDATA"]) / "GoldenDict" / "config"
                )
                configuration_contents.append(configuration.read_bytes())
                self.assertEqual(
                    child_environment["GOLDENDICT_ACCEPTANCE_MANAGEMENT_CATALOG"],
                    str(catalog),
                )
                self.assertEqual(
                    child_environment[
                        "GOLDENDICT_ACCEPTANCE_MANAGEMENT_CATALOG_SHA256"
                    ],
                    catalog_hash,
                )
                Path(
                    child_environment["GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH"]
                ).write_text("{}", encoding="utf-8")
                return subprocess.CompletedProcess(command, 0)

            def observe(scenario: str) -> None:
                observer.observe(
                    paths["executable"],
                    paths["provenance"],
                    [paths["runtime"]],
                    paths["plugins"],
                    paths["corpus"],
                    paths["index"],
                    "en_US",
                    CONDITIONS_HASH,
                    scenario,
                    paths["output"],
                    60,
                    management_catalog=catalog,
                    management_catalog_sha256=catalog_hash,
                )

            with (
                mock.patch.dict(os.environ, environment, clear=False),
                mock.patch.object(
                    observer,
                    "_suppressed_windows_error_dialogs",
                    return_value=nullcontext(),
                ),
                mock.patch.object(observer.subprocess, "run", side_effect=run),
            ):
                observe("clean-discovery")
                paths["output"] = paths["output"].with_name("raw-warm.json")
                observe("warm-restart")

            self.assertEqual(2, len(appdata_paths))
            self.assertEqual(appdata_paths[0], appdata_paths[1])
            self.assertEqual(configuration_contents[0], configuration_contents[1])

    def test_rejects_mdict_catalog_hash_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            catalog = paths["evidence"] / "catalog.json"
            catalog.write_text("{}\n", encoding="utf-8")
            with (
                mock.patch.dict(os.environ, environment, clear=False),
                self.assertRaisesRegex(observer.Qt5ObserverError, "hash is invalid"),
            ):
                observer.observe(
                    paths["executable"],
                    paths["provenance"],
                    [paths["runtime"]],
                    paths["plugins"],
                    paths["corpus"],
                    paths["index"],
                    "en_US",
                    CONDITIONS_HASH,
                    "clean-discovery",
                    paths["output"],
                    60,
                    mdict_catalog=catalog,
                    mdict_catalog_sha256="0" * 64,
                )

    def test_projects_each_supported_lifecycle_scenario(self) -> None:
        def make_run(captured: list[str]):
            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                child_environment = kwargs["env"]
                assert isinstance(child_environment, dict)
                captured.append(child_environment["GOLDENDICT_ACCEPTANCE_SCENARIO"])
                Path(
                    child_environment["GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH"]
                ).write_text("{}", encoding="utf-8")
                return subprocess.CompletedProcess(command, 0)

            return run

        for scenario in observer.SUPPORTED_SCENARIOS:
            with self.subTest(
                scenario=scenario
            ), tempfile.TemporaryDirectory() as temporary:
                environment, paths = self._fixture(Path(temporary))
                captured: list[str] = []

                with (
                    mock.patch.dict(os.environ, environment, clear=False),
                    mock.patch.object(
                        observer,
                        "_suppressed_windows_error_dialogs",
                        return_value=nullcontext(),
                    ),
                    mock.patch.object(
                        observer.subprocess, "run", side_effect=make_run(captured)
                    ),
                ):
                    self._call(paths, scenario)
                self.assertEqual([scenario], captured)

    def test_writes_legacy_non_windows_profile_location(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            captured_config: list[Path] = []

            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                child_environment = kwargs["env"]
                assert isinstance(child_environment, dict)
                config = Path(child_environment["HOME"]) / ".goldendict" / "config"
                captured_config.append(config)
                self.assertTrue(config.is_file())
                Path(
                    child_environment["GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH"]
                ).write_text("{}", encoding="utf-8")
                return subprocess.CompletedProcess(command, 0)

            with (
                mock.patch.dict(os.environ, environment, clear=False),
                mock.patch.object(observer.platform, "system", return_value="Linux"),
                mock.patch.object(observer.subprocess, "run", side_effect=run),
            ):
                self.assertEqual(self._call(paths), paths["output"])

            self.assertEqual(len(captured_config), 1)

    def test_rejects_portable_state_next_to_executable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            (paths["executable"].parent / "portable").mkdir()
            with (
                mock.patch.dict(os.environ, environment, clear=False),
                self.assertRaisesRegex(observer.Qt5ObserverError, "portable state"),
            ):
                self._call(paths)

    def test_rejects_provenance_from_other_instrumentation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            value = json.loads(paths["provenance"].read_text(encoding="utf-8"))
            value["observer_include_sha256"] = "0" * 64
            paths["provenance"].write_text(json.dumps(value), encoding="utf-8")
            with (
                mock.patch.dict(os.environ, environment, clear=False),
                self.assertRaisesRegex(observer.Qt5ObserverError, "does not match"),
            ):
                self._call(paths)

    def test_rejects_prepared_source_changed_after_provenance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            (paths["provenance"].parent / "main.cc").write_text(
                "changed\n", encoding="utf-8"
            )
            with (
                mock.patch.dict(os.environ, environment, clear=False),
                self.assertRaisesRegex(observer.Qt5ObserverError, "does not match"),
            ):
                self._call(paths)

    def test_failed_process_removes_partial_raw_result(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))

            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                child_environment = kwargs["env"]
                assert isinstance(child_environment, dict)
                Path(
                    child_environment["GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH"]
                ).write_text("partial", encoding="utf-8")
                return subprocess.CompletedProcess(command, 9)

            with (
                mock.patch.dict(os.environ, environment, clear=False),
                mock.patch.object(
                    observer,
                    "_suppressed_windows_error_dialogs",
                    return_value=nullcontext(),
                ),
                mock.patch.object(observer.subprocess, "run", side_effect=run),
                self.assertRaisesRegex(observer.Qt5ObserverError, "exit code 9"),
            ):
                self._call(paths)
            self.assertFalse(paths["output"].exists())

    def test_rejects_raw_output_outside_evidence_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            paths["output"] = Path(temporary) / "outside" / "raw.json"
            paths["output"].parent.mkdir()
            with (
                mock.patch.dict(os.environ, environment, clear=False),
                self.assertRaisesRegex(observer.Qt5ObserverError, "evidence root"),
            ):
                self._call(paths)

    def test_rejects_raw_output_in_unrecognized_evidence_subdirectory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            paths["output"] = paths["evidence"] / "unrecognized" / "raw.json"
            paths["output"].parent.mkdir()
            with (
                mock.patch.dict(os.environ, environment, clear=False),
                self.assertRaisesRegex(observer.Qt5ObserverError, "evidence root"),
            ):
                self._call(paths)

    def test_rejects_mismatched_paired_revision(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            environment["GOLDENDICT_ACCEPTANCE_REVISION"] = "b" * 40
            with (
                mock.patch.dict(os.environ, environment, clear=False),
                self.assertRaisesRegex(observer.Qt5ObserverError, "frozen"),
            ):
                self._call(paths)


if __name__ == "__main__":
    unittest.main()
