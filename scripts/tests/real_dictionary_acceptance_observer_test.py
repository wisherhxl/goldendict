#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import hashlib
import json
import os
import platform
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_dictionary_acceptance_observer as observer
import real_dictionary_acceptance_result as result
import real_dictionary_acceptance_workspace as workspace

PAIR_ID = "a" * 64
REVISION = "c" * 40

FAKE_OBSERVER = r"""
import json
import os
from pathlib import Path
import sys

def option(name):
    return sys.argv[sys.argv.index(name) + 1]

if os.environ.get("FAKE_OBSERVER_FAIL"):
    raise SystemExit(7)
dictionary_id = "dsl-0123456789abcdef"
scenario = option("--scenario")
if os.environ.get("FAKE_CREATE_INDEX"):
    index_path = Path(option("--index-root")) / f"{dictionary_id}.gdidx"
    if os.environ.get("FAKE_REWRITE_SAME"):
        index_path.write_bytes(index_path.read_bytes())
        metadata = index_path.stat()
        os.utime(index_path, ns=(metadata.st_atime_ns, metadata.st_mtime_ns + 2_000_000_000))
    else:
        content = b"rebuilt" if scenario == "changed-source" else b"index"
        if not index_path.exists() or index_path.read_bytes() != content:
            index_path.write_bytes(content)
source = os.environ["FAKE_COMPONENT"]
phase = {
    "clean-discovery": "discovery",
    "warm-restart": "restart",
    "explicit-rescan": "rescan",
    "changed-source": "source-change",
    "unavailable-companion": "companion-unavailable",
    "companion-recovery": "companion-recovery",
}[scenario]
raw = {
    "catalog": [{
        "article_count": 2,
        "components": [source],
        "edition": "",
        "enabled": True,
        "format": "dsl",
        "headword_count": 3,
        "id": dictionary_id,
        "name": "Fixture",
        "order": 0,
        "source_language": "en",
        "target_language": "zh",
    }],
    "conditions_sha256": option("--conditions-sha256"),
    "elapsed_milliseconds": 4,
    "errors": [{
        "code": "dictionary-unavailable",
        "dictionary_id": "",
        "message": source + ": companion unavailable",
    }],
    "outcome": "completed",
    "phases": [
        {"dictionary_id": "", "name": phase, "sequence": 0, "status": "started"},
        {"dictionary_id": "", "name": phase, "sequence": 1, "status": "completed"},
    ],
    "scenario": scenario,
    "schema": "goldendict-real-dictionary-raw-observation-v1",
}
Path(option("--output")).write_text(json.dumps(raw), encoding="utf-8")
"""


class AcceptanceObserverTest(unittest.TestCase):
    def _fixture(self, root: Path) -> tuple[dict[str, str], dict[str, Path]]:
        corpus = root / "corpus"
        index = root / "indexes"
        evidence = root / "evidence"
        config = root / "config"
        for directory in (corpus, index, evidence, config):
            directory.mkdir()
        dictionary = corpus / "sample.dsl"
        dictionary.write_bytes(b"fixture")
        manifest = root / "manifest.json"
        manifest.write_text(
            json.dumps(
                {
                    "files": [
                        {
                            "classification": "dsl_dictionary",
                            "path": "sample.dsl",
                            "sha256": hashlib.sha256(b"fixture").hexdigest(),
                            "size": 7,
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        conditions = config / "conditions.json"
        condition_value = {
            "group": {
                "mode": "all-enabled",
                "name": "All",
                "ordered_dictionary_ids": [],
            },
            "locale": "en_US",
            "platform": {
                "architecture": platform.machine(),
                "operating_system": platform.system(),
            },
            "preferences": {"profile": "clean-default"},
            "queries": [],
            "schema": workspace.CONDITIONS_SCHEMA,
        }
        conditions.write_text(json.dumps(condition_value), encoding="utf-8")
        fake = root / "fake_observer.py"
        fake.write_text(FAKE_OBSERVER, encoding="utf-8")
        paths = {
            "ack": evidence / "condition-ack.json",
            "conditions": conditions,
            "corpus": corpus,
            "dictionary": dictionary,
            "evidence": evidence,
            "fake": fake,
            "index": index,
            "manifest": manifest,
            "result": evidence / "observation.json",
        }
        environment = {
            "FAKE_COMPONENT": str(dictionary),
            "FAKE_CREATE_INDEX": "1",
            "GOLDENDICT_ACCEPTANCE_ACK_PATH": str(paths["ack"]),
            "GOLDENDICT_ACCEPTANCE_CONDITIONS_FILE": str(conditions),
            "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256": hashlib.sha256(
                result.canonical_json(condition_value)
            ).hexdigest(),
            "GOLDENDICT_ACCEPTANCE_CORPUS_MANIFEST_SHA256": hashlib.sha256(
                manifest.read_bytes()
            ).hexdigest(),
            "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT": str(corpus),
            "GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT": str(evidence),
            "GOLDENDICT_ACCEPTANCE_INDEX_ROOT": str(index),
            "GOLDENDICT_ACCEPTANCE_PAIR_ID": PAIR_ID,
            "GOLDENDICT_ACCEPTANCE_RESULT_PATH": str(paths["result"]),
            "GOLDENDICT_ACCEPTANCE_REVISION": REVISION,
            "GOLDENDICT_ACCEPTANCE_VERSION": "qt6",
        }
        return environment, paths

    def test_adapts_raw_catalog_diagnostics_and_created_index(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            with mock.patch.dict(os.environ, environment, clear=True):
                output = observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "clean-discovery",
                )

            self.assertEqual(paths["result"], output)
            observation = result.read_observation(
                output,
                expected_pair_id=PAIR_ID,
                expected_version="qt6",
                expected_revision=REVISION,
            )
            self.assertEqual(
                "dsl:sample.dsl", observation["dictionaries"][0]["logical_key"]
            )
            self.assertEqual("created", observation["indexes"][0]["disposition"])
            self.assertEqual(
                "sample.dsl", observation["diagnostics"][0]["source_component"]
            )
            self.assertNotIn(str(paths["corpus"]), output.read_text(encoding="utf-8"))
            acknowledgement = json.loads(paths["ack"].read_text(encoding="utf-8"))
            self.assertEqual(
                workspace.ACKNOWLEDGEMENT_SCHEMA, acknowledgement["schema"]
            )
            self.assertFalse(
                any(
                    path.name.startswith(".raw-observation-")
                    for path in paths["evidence"].iterdir()
                )
            )

    def test_rejects_component_outside_corpus_without_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            environment, paths = self._fixture(root)
            outside = root / "outside.dsl"
            outside.write_bytes(b"outside")
            environment["FAKE_COMPONENT"] = str(outside)
            with mock.patch.dict(
                os.environ, environment, clear=True
            ), self.assertRaisesRegex(observer.ObserverError, "not a corpus file"):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "clean-discovery",
                )
            self.assertFalse(paths["result"].exists())
            self.assertFalse(paths["ack"].exists())

    def test_failed_probe_does_not_publish_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            environment["FAKE_OBSERVER_FAIL"] = "1"
            with mock.patch.dict(
                os.environ, environment, clear=True
            ), self.assertRaisesRegex(observer.ObserverError, "exit code 7"):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "clean-discovery",
                )
            self.assertFalse(paths["result"].exists())
            self.assertFalse(paths["ack"].exists())

    def test_rejects_condition_binding_mismatch_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            environment["GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256"] = "f" * 64
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch(
                "real_dictionary_acceptance_observer.subprocess.run"
            ) as run, self.assertRaisesRegex(
                observer.ObserverError, "Conditions hash does not match"
            ):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "clean-discovery",
                )
            run.assert_not_called()
            self.assertFalse(paths["result"].exists())
            self.assertFalse(paths["ack"].exists())

    def test_rejects_reserved_probe_options_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            for option in observer.RESERVED_OBSERVER_OPTIONS:
                for arguments in ([option, "injected"], [f"{option}=injected"]):
                    with self.subTest(
                        option=option, arguments=arguments
                    ), mock.patch.dict(os.environ, environment, clear=True), mock.patch(
                        "real_dictionary_acceptance_observer.subprocess.run"
                    ) as run, self.assertRaisesRegex(
                        observer.ObserverError, "must not override reserved option"
                    ):
                        observer.observe(
                            "qt6",
                            Path(sys.executable),
                            arguments,
                            paths["corpus"],
                            paths["manifest"],
                            "clean-discovery",
                        )
                    run.assert_not_called()

    def test_rejects_unsupported_scenario_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch(
                "real_dictionary_acceptance_observer.subprocess.run"
            ) as run, self.assertRaisesRegex(
                observer.ObserverError, "unsupported lifecycle scenario"
            ):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "cancellation",
                )
            run.assert_not_called()

    def test_records_reused_indexes_for_nonclean_scenarios(self) -> None:
        for scenario, phase in (
            ("warm-restart", "restart"),
            ("explicit-rescan", "rescan"),
            ("unavailable-companion", "companion-unavailable"),
            ("companion-recovery", "companion-recovery"),
        ):
            with self.subTest(
                scenario=scenario
            ), tempfile.TemporaryDirectory() as temporary:
                environment, paths = self._fixture(Path(temporary))
                (paths["index"] / "dsl-0123456789abcdef.gdidx").write_bytes(b"index")
                with mock.patch.dict(os.environ, environment, clear=True):
                    output = observer.observe(
                        "qt6",
                        Path(sys.executable),
                        [str(paths["fake"])],
                        paths["corpus"],
                        paths["manifest"],
                        scenario,
                    )
                observation = result.read_observation(output)
                self.assertEqual(scenario, observation["scenario"])
                self.assertEqual(phase, observation["phases"][0]["name"])
                self.assertEqual("reused", observation["indexes"][0]["disposition"])

    def test_records_rebuilt_index_for_changed_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            (paths["index"] / "dsl-0123456789abcdef.gdidx").write_bytes(b"index")
            with mock.patch.dict(os.environ, environment, clear=True):
                output = observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "changed-source",
                )
            observation = result.read_observation(output)
            self.assertEqual("source-change", observation["phases"][0]["name"])
            self.assertEqual("rebuilt", observation["indexes"][0]["disposition"])

    def test_records_same_content_rewrite_as_rebuilt_index(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            environment["FAKE_REWRITE_SAME"] = "1"
            (paths["index"] / "dsl-0123456789abcdef.gdidx").write_bytes(b"index")
            with mock.patch.dict(os.environ, environment, clear=True):
                output = observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "changed-source",
                )
            observation = result.read_observation(output)
            self.assertEqual("rebuilt", observation["indexes"][0]["disposition"])

    def test_rejects_restart_without_existing_indexes_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch(
                "real_dictionary_acceptance_observer.subprocess.run"
            ) as run, self.assertRaisesRegex(
                observer.ObserverError, "requires previously created indexes"
            ):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "warm-restart",
                )
            run.assert_not_called()

    def test_allows_recovery_to_recreate_removed_indexes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            with mock.patch.dict(os.environ, environment, clear=True):
                output = observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "companion-recovery",
                )
            observation = result.read_observation(output)
            self.assertEqual("created", observation["indexes"][0]["disposition"])

    def test_rejects_nonempty_index_for_clean_discovery_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            (paths["index"] / "existing.gdidx").write_bytes(b"existing")
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch(
                "real_dictionary_acceptance_observer.subprocess.run"
            ) as run, self.assertRaisesRegex(
                observer.ObserverError, "requires an empty index directory"
            ):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "clean-discovery",
                )
            run.assert_not_called()

    def test_acknowledgement_failure_removes_partial_observation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch(
                "real_dictionary_acceptance_observer._atomic_json",
                side_effect=observer.ObserverError("forced acknowledgement failure"),
            ), self.assertRaisesRegex(
                observer.ObserverError, "forced acknowledgement failure"
            ):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "clean-discovery",
                )
            self.assertFalse(paths["result"].exists())
            self.assertFalse(paths["ack"].exists())

    def test_observation_failure_does_not_publish_acknowledgement(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            environment, paths = self._fixture(Path(temporary_directory))
            with mock.patch.dict(os.environ, environment, clear=True), mock.patch(
                "real_dictionary_acceptance_observer.acceptance_result.write_observation",
                side_effect=result.ResultError("forced observation failure"),
            ), self.assertRaisesRegex(result.ResultError, "forced observation failure"):
                observer.observe(
                    "qt6",
                    Path(sys.executable),
                    [str(paths["fake"])],
                    paths["corpus"],
                    paths["manifest"],
                    "clean-discovery",
                )
            self.assertFalse(paths["result"].exists())
            self.assertFalse(paths["ack"].exists())

    def test_expands_manifest_bound_dictionary_companions(self) -> None:
        manifest = {
            path: {"path": path}
            for path in (
                "dsl/sample.dsl.dz",
                "dsl/sample_abrv.dsl",
                "dsl/sample.dsl.files.zip",
                "mdict/sample.mdx",
                "mdict/sample.mdd",
                "mdict/sample.1.mdd",
                "mdict/sample.2.mdd",
                "stardict/sample.ifo",
                "stardict/sample.idx",
                "stardict/sample.dict",
                "stardict/sample.dict.dz",
                "stardict/sample.syn",
            )
        }
        self.assertEqual(
            [
                "dsl/sample.dsl.dz",
                "dsl/sample.dsl.files.zip",
                "dsl/sample_abrv.dsl",
            ],
            observer._expand_components("dsl", ["dsl/sample.dsl.dz"], manifest),
        )
        self.assertEqual(
            [
                "stardict/sample.dict",
                "stardict/sample.idx",
                "stardict/sample.ifo",
                "stardict/sample.syn",
            ],
            observer._expand_components("stardict", ["stardict/sample.ifo"], manifest),
        )
        self.assertEqual(
            [
                "mdict/sample.1.mdd",
                "mdict/sample.2.mdd",
                "mdict/sample.mdd",
                "mdict/sample.mdx",
            ],
            observer._expand_components("mdict", ["mdict/sample.mdx"], manifest),
        )


if __name__ == "__main__":
    unittest.main()
