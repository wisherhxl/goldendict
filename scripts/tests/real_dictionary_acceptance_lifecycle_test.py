#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_dictionary_acceptance_lifecycle as lifecycle
import real_dictionary_acceptance_result as result

PAIR_ID = "a" * 64
QT5_REVISION = "b" * 40
QT6_REVISION = "c" * 40
HASH = "d" * 64


def observation(version: str, revision: str, scenario: str) -> dict[str, object]:
    disposition = {
        "clean-discovery": "created",
        "warm-restart": "reused",
        "explicit-rescan": "reused",
        "changed-source": "rebuilt",
        "companion-recovery": "created",
    }.get(scenario)
    phase = {
        "clean-discovery": "discovery",
        "warm-restart": "restart",
        "explicit-rescan": "rescan",
        "changed-source": "source-change",
        "unavailable-companion": "companion-unavailable",
        "companion-recovery": "companion-recovery",
    }[scenario]
    unavailable = scenario == "unavailable-companion"
    return {
        "conditions_sha256": HASH,
        "corpus_manifest_sha256": HASH,
        "diagnostics": [],
        "dictionaries": (
            []
            if unavailable
            else [
                {
                    "article_count": 1,
                    "edition": "",
                    "enabled": True,
                    "headword_count": 1,
                    "id": f"{version}-id",
                    "logical_key": "dsl:sample.dsl",
                    "name": "Sample",
                    "order": 0,
                    "source_components": ["sample.dsl"],
                    "source_language": "en",
                    "target_language": "de",
                }
            ]
        ),
        "indexes": (
            []
            if unavailable
            else [
                {
                    "dictionary_key": "dsl:sample.dsl",
                    "disposition": disposition,
                    "elapsed_milliseconds": None,
                    "file_name": f"{version}.gdidx",
                    "role": "headword",
                    "sha256": (
                        "e" * 64
                        if scenario
                        in (
                            "changed-source",
                            "companion-recovery",
                        )
                        else HASH
                    ),
                    "size": 1,
                }
            ]
        ),
        "outcome": "completed",
        "pair_id": PAIR_ID,
        "phases": [
            {"dictionary_key": None, "name": phase, "sequence": 0, "status": "started"},
            {
                "dictionary_key": None,
                "name": phase,
                "sequence": 1,
                "status": "completed",
            },
        ],
        "revision": revision,
        "scenario": scenario,
        "schema": result.OBSERVATION_SCHEMA,
        "version": version,
    }


class AcceptanceLifecycleTest(unittest.TestCase):
    def _workspace(self, root: Path) -> Path:
        workspace = root / "workspace"
        for version in result.VERSIONS:
            for name in lifecycle.acceptance_workspace.DIRECTORY_NAMES:
                (workspace / version / name).mkdir(parents=True)
        (workspace / "pair.json").write_text(
            json.dumps(
                {
                    "conditions_sha256": HASH,
                    "corpus": {"manifest_sha256": HASH},
                    "pair_id": PAIR_ID,
                }
            ),
            encoding="utf-8",
        )
        return workspace

    def test_runs_three_scenarios_and_archives_each_result(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            scenarios: list[str] = []

            def run(*args: object, **kwargs: object) -> int:
                command = args[7]
                assert isinstance(command, list)
                scenario = command[command.index("--scenario") + 1]
                scenarios.append(scenario)
                result.write_observation(
                    workspace / "qt6" / "evidence" / "observation.json",
                    observation("qt6", QT6_REVISION, scenario),
                )
                return 0

            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "run_in_workspace",
                    side_effect=run,
                ),
            ):
                outputs = lifecycle.run_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )

            self.assertEqual(list(lifecycle.SCENARIOS), scenarios)
            self.assertTrue(all(path.is_file() for path in outputs))

    def test_resumes_after_an_archived_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            archive = workspace / "qt5" / "evidence" / "lifecycle"
            archive.mkdir()
            result.write_observation(
                archive / "clean-discovery.json",
                observation("qt5", QT5_REVISION, "clean-discovery"),
            )
            scenarios: list[str] = []

            def run(*args: object, **kwargs: object) -> int:
                command = args[7]
                assert isinstance(command, list)
                scenario = command[command.index("--scenario") + 1]
                scenarios.append(scenario)
                result.write_observation(
                    workspace / "qt5" / "evidence" / "observation.json",
                    observation("qt5", QT5_REVISION, scenario),
                )
                return 0

            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "run_in_workspace",
                    side_effect=run,
                ),
            ):
                lifecycle.run_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt5",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )
            self.assertEqual(["warm-restart", "explicit-rescan"], scenarios)

    def test_reruns_unarchived_observation_after_interruption(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            result.write_observation(
                workspace / "qt6" / "evidence" / "observation.json",
                observation("qt6", QT6_REVISION, "clean-discovery"),
            )
            index = workspace / "qt6" / "indexes" / "generated.index"
            index.write_bytes(b"generated")
            cached = workspace / "qt6" / "cache" / "nested" / "discovery.cache"
            cached.parent.mkdir()
            cached.write_bytes(b"cached")
            scenarios: list[str] = []

            def run(*args: object, **kwargs: object) -> int:
                command = args[7]
                assert isinstance(command, list)
                scenario = command[command.index("--scenario") + 1]
                scenarios.append(scenario)
                if scenario == "clean-discovery":
                    self.assertEqual(
                        [], list((workspace / "qt6" / "indexes").iterdir())
                    )
                    self.assertEqual([], list((workspace / "qt6" / "cache").iterdir()))
                    index.write_bytes(b"regenerated")
                else:
                    self.assertEqual(b"regenerated", index.read_bytes())
                result.write_observation(
                    workspace / "qt6" / "evidence" / "observation.json",
                    observation("qt6", QT6_REVISION, scenario),
                )
                return 0

            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "run_in_workspace",
                    side_effect=run,
                ),
            ):
                lifecycle.run_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )
            self.assertEqual(list(lifecycle.SCENARIOS), scenarios)

    def test_rejects_resumed_archive_with_wrong_manifest_hash(self) -> None:
        self._assert_rejects_misbound_archive("corpus_manifest_sha256")

    def test_rejects_resumed_archive_with_wrong_conditions_hash(self) -> None:
        self._assert_rejects_misbound_archive("conditions_sha256")

    def _assert_rejects_misbound_archive(self, field: str) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            archive = workspace / "qt6" / "evidence" / "lifecycle"
            archive.mkdir()
            misbound = observation("qt6", QT6_REVISION, "clean-discovery")
            misbound[field] = "e" * 64
            result.write_observation(archive / "clean-discovery.json", misbound)
            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace, "run_in_workspace"
                ) as run,
                self.assertRaises(result.ResultError),
            ):
                lifecycle.run_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )
            run.assert_not_called()

    def test_nonzero_run_is_not_resumable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))

            def failed_run(*args: object, **kwargs: object) -> int:
                result.write_observation(
                    workspace / "qt5" / "evidence" / "observation.json",
                    observation("qt5", QT5_REVISION, "clean-discovery"),
                )
                return 7

            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "run_in_workspace",
                    side_effect=failed_run,
                ),
                self.assertRaisesRegex(lifecycle.LifecycleError, "exit code 7"),
            ):
                lifecycle.run_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt5",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )

            self.assertFalse(
                (
                    workspace
                    / "qt5"
                    / "evidence"
                    / "lifecycle"
                    / "clean-discovery.json"
                ).exists()
            )

    def test_rejects_reparse_lifecycle_archive_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            archive = workspace / "qt6" / "evidence" / "lifecycle"
            archive.mkdir()
            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle,
                    "_is_reparse_point",
                    side_effect=lambda path: path == archive,
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace, "run_in_workspace"
                ) as run,
                self.assertRaisesRegex(lifecycle.LifecycleError, "reparse point"),
            ):
                lifecycle.run_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )
            run.assert_not_called()

    def test_rejects_reparse_entry_during_clean_run_reset(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            redirected = workspace / "qt5" / "indexes" / "redirected"
            redirected.mkdir()
            marker = redirected / "preserved.index"
            marker.write_bytes(b"preserved")
            original = lifecycle._is_reparse_point

            def is_reparse(path: Path) -> bool:
                return path == redirected or original(path)

            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle, "_is_reparse_point", side_effect=is_reparse
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace, "run_in_workspace"
                ) as run,
                self.assertRaisesRegex(lifecycle.LifecycleError, "reparse point"),
            ):
                lifecycle.run_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt5",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )
            run.assert_not_called()
            self.assertEqual(b"preserved", marker.read_bytes())

    def test_rejects_reparse_comparison_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            for version, revision in (
                ("qt5", QT5_REVISION),
                ("qt6", QT6_REVISION),
            ):
                archive = workspace / version / "evidence" / "lifecycle"
                archive.mkdir()
                for scenario in lifecycle.SCENARIOS:
                    result.write_observation(
                        archive / f"{scenario}.json",
                        observation(version, revision, scenario),
                    )
            comparison = workspace / "comparisons"
            comparison.mkdir()
            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle,
                    "_is_reparse_point",
                    side_effect=lambda path: path == comparison,
                ),
                self.assertRaisesRegex(lifecycle.LifecycleError, "reparse point"),
            ):
                lifecycle.compare_lifecycle(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_rejects_identity_change_before_archiving(self) -> None:
        changed = observation("qt6", QT6_REVISION, "warm-restart")
        changed["dictionaries"][0]["name"] = "Changed"  # type: ignore[index]
        with self.assertRaisesRegex(lifecycle.LifecycleError, "identity changed"):
            lifecycle._validate_transitions(
                [
                    observation("qt6", QT6_REVISION, "clean-discovery"),
                    changed,
                ]
            )

    def test_rejects_non_reused_warm_index(self) -> None:
        warm = observation("qt6", QT6_REVISION, "warm-restart")
        warm["indexes"][0]["disposition"] = "rebuilt"  # type: ignore[index]
        with self.assertRaisesRegex(lifecycle.LifecycleError, "did not reuse"):
            lifecycle._validate_transitions(
                [
                    observation("qt6", QT6_REVISION, "clean-discovery"),
                    warm,
                ]
            )

    def test_rejects_empty_clean_index_set(self) -> None:
        clean = observation("qt6", QT6_REVISION, "clean-discovery")
        clean["indexes"] = []
        with self.assertRaisesRegex(lifecycle.LifecycleError, "at least one index"):
            lifecycle._validate_transitions([clean])

    def test_rejects_changed_index_artifact(self) -> None:
        warm = observation("qt6", QT6_REVISION, "warm-restart")
        warm["indexes"][0]["sha256"] = "e" * 64  # type: ignore[index]
        with self.assertRaisesRegex(lifecycle.LifecycleError, "Index identity changed"):
            lifecycle._validate_transitions(
                [
                    observation("qt6", QT6_REVISION, "clean-discovery"),
                    warm,
                ]
            )

    def test_compares_complete_paired_lifecycle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            for version, revision in (
                ("qt5", QT5_REVISION),
                ("qt6", QT6_REVISION),
            ):
                archive = workspace / version / "evidence" / "lifecycle"
                archive.mkdir()
                for scenario in lifecycle.SCENARIOS:
                    value = observation(version, revision, scenario)
                    result.write_observation(archive / f"{scenario}.json", value)
            with mock.patch.object(
                lifecycle.acceptance_workspace,
                "validate_workspace",
                return_value=PAIR_ID,
            ):
                summary = lifecycle.compare_lifecycle(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                )
            self.assertTrue(summary.is_file())
            self.assertTrue(
                all(
                    (workspace / "comparisons" / f"{scenario}.json").is_file()
                    for scenario in lifecycle.SCENARIOS
                )
            )

    def test_runs_recovery_state_machine_and_archives_each_result(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            scenarios: list[str] = []

            def run(*args: object, **kwargs: object) -> int:
                command = args[7]
                assert isinstance(command, list)
                scenario = command[command.index("--scenario") + 1]
                scenarios.append(scenario)
                result.write_observation(
                    workspace / "qt6" / "evidence" / "observation.json",
                    observation("qt6", QT6_REVISION, scenario),
                )
                return 0

            with (
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "validate_workspace",
                    return_value=PAIR_ID,
                ),
                mock.patch.object(
                    lifecycle.acceptance_workspace,
                    "run_in_workspace",
                    side_effect=run,
                ),
            ):
                outputs = lifecycle.run_recovery_version(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    ["observer", "--scenario", lifecycle.SCENARIO_TOKEN],
                )

            self.assertEqual(list(lifecycle.RECOVERY_SCENARIOS), scenarios)
            self.assertTrue(all(path.is_file() for path in outputs))

    def test_recovery_state_machine_rejects_incomplete_behavior(self) -> None:
        clean = observation("qt6", QT6_REVISION, "clean-discovery")
        changed = observation("qt6", QT6_REVISION, "changed-source")
        unavailable = observation("qt6", QT6_REVISION, "unavailable-companion")
        recovered = observation("qt6", QT6_REVISION, "companion-recovery")

        changed["indexes"][0]["disposition"] = "reused"  # type: ignore[index]
        with self.assertRaisesRegex(lifecycle.LifecycleError, "must rebuild"):
            lifecycle._validate_recovery_transitions([clean, changed])

        changed = observation("qt6", QT6_REVISION, "changed-source")
        unavailable["dictionaries"] = clean["dictionaries"]
        with self.assertRaisesRegex(lifecycle.LifecycleError, "must remove"):
            lifecycle._validate_recovery_transitions([clean, changed, unavailable])

        unavailable = observation("qt6", QT6_REVISION, "unavailable-companion")
        recovered["indexes"][0]["disposition"] = "reused"  # type: ignore[index]
        with self.assertRaisesRegex(lifecycle.LifecycleError, "must recreate"):
            lifecycle._validate_recovery_transitions(
                [clean, changed, unavailable, recovered]
            )

    def test_compares_complete_paired_recovery_lifecycle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = self._workspace(Path(temporary))
            for version, revision in (
                ("qt5", QT5_REVISION),
                ("qt6", QT6_REVISION),
            ):
                archive = workspace / version / "evidence" / "recovery"
                archive.mkdir()
                for scenario in lifecycle.RECOVERY_SCENARIOS:
                    result.write_observation(
                        archive / f"{scenario}.json",
                        observation(version, revision, scenario),
                    )
            with mock.patch.object(
                lifecycle.acceptance_workspace,
                "validate_workspace",
                return_value=PAIR_ID,
            ):
                summary = lifecycle.compare_recovery(
                    workspace,
                    Path("corpus"),
                    Path("manifest"),
                    Path("conditions"),
                    QT5_REVISION,
                    QT6_REVISION,
                )
            self.assertTrue(summary.is_file())
            summary_value = json.loads(summary.read_text(encoding="utf-8"))
            self.assertEqual(lifecycle.RECOVERY_SUMMARY_SCHEMA, summary_value["schema"])
            self.assertTrue(summary_value["equivalent"])
            self.assertTrue(
                all(
                    (workspace / "recovery-comparisons" / f"{scenario}.json").is_file()
                    for scenario in lifecycle.RECOVERY_SCENARIOS
                )
            )

    def test_requires_one_scenario_placeholder(self) -> None:
        for command in (["observer"], [lifecycle.SCENARIO_TOKEN] * 2):
            with (
                self.subTest(command=command),
                self.assertRaisesRegex(lifecycle.LifecycleError, "exactly one"),
            ):
                lifecycle._scenario_command(command, "clean-discovery")


if __name__ == "__main__":
    unittest.main()
