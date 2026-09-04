#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_dictionary_acceptance_result as result

HASH_A = "a" * 64
HASH_B = "b" * 64
HASH_C = "c" * 64
QT5_REVISION = "5" * 40
QT6_REVISION = "6" * 40


def observation(version: str = "qt5") -> dict[str, object]:
    return {
        "conditions_sha256": HASH_B,
        "corpus_manifest_sha256": HASH_C,
        "diagnostics": [],
        "dictionaries": [
            {
                "article_count": 2,
                "edition": "1",
                "enabled": True,
                "headword_count": 3,
                "id": f"{version}-generated-id",
                "logical_key": "dsl:sample/test.dsl",
                "name": "Test",
                "order": 0,
                "source_components": [
                    "sample/test.dsl",
                    "sample/test.dsl.files/icon.png",
                ],
                "source_language": "en",
                "target_language": "zh",
            }
        ],
        "indexes": [
            {
                "dictionary_key": "dsl:sample/test.dsl",
                "disposition": "created",
                "elapsed_milliseconds": 15,
                "file_name": "qt5-generated.idx",
                "role": "headword",
                "sha256": HASH_A,
                "size": 42,
            }
        ],
        "outcome": "completed",
        "pair_id": HASH_A,
        "phases": [
            {
                "dictionary_key": None,
                "name": "discovery",
                "sequence": 0,
                "status": "started",
            },
            {
                "dictionary_key": "dsl:sample/test.dsl",
                "name": "index",
                "sequence": 1,
                "status": "completed",
            },
        ],
        "revision": QT5_REVISION if version == "qt5" else QT6_REVISION,
        "scenario": "clean-discovery",
        "schema": result.OBSERVATION_SCHEMA,
        "version": version,
    }


class AcceptanceResultTest(unittest.TestCase):
    def test_round_trip_is_canonical_and_bound(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "observation.json"
            value = observation()
            result.write_observation(path, value)

            self.assertEqual(result.canonical_json(value), path.read_bytes())
            self.assertEqual(
                value,
                result.read_observation(
                    path,
                    expected_pair_id=HASH_A,
                    expected_version="qt5",
                    expected_revision=QT5_REVISION,
                    expected_manifest_hash=HASH_C,
                    expected_conditions_hash=HASH_B,
                ),
            )

    def test_rejects_noncanonical_json(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "observation.json"
            path.write_text(json.dumps(observation(), indent=2), encoding="utf-8")
            with self.assertRaisesRegex(result.ResultError, "not canonical"):
                result.read_observation(path)

    def test_rejects_absolute_parent_and_unnormalized_source_paths(self) -> None:
        for source in (
            "C:/private/test.dsl",
            "/private/test.dsl",
            "a/../test.dsl",
            "a\\test.dsl",
            "a//test.dsl",
            "a/./test.dsl",
            "a/test.dsl/",
            "prefix/C:/private/test.dsl",
            ".",
        ):
            with self.subTest(source=source):
                value = observation()
                value["dictionaries"][0]["source_components"] = [source]  # type: ignore[index]
                with self.assertRaises(result.ResultError):
                    result.validate_observation(value)

    def test_rejects_absolute_or_malformed_logical_keys(self) -> None:
        for logical_key in (
            "C:\\private\\test.dsl",
            "/private/test.dsl",
            "dsl:C:/private/test.dsl",
            "dsl:a//test.dsl",
            "DSL:a/test.dsl",
            "missing-prefix",
        ):
            with self.subTest(logical_key=logical_key):
                value = observation()
                value["dictionaries"][0]["logical_key"] = logical_key  # type: ignore[index]
                with self.assertRaises(result.ResultError):
                    result.validate_observation(value)

    def test_rejects_extra_fields_unknown_references_and_duplicate_order(self) -> None:
        extra = observation()
        extra["private_path"] = "C:/secret"
        with self.assertRaisesRegex(result.ResultError, "keys differ"):
            result.validate_observation(extra)

        unknown = observation()
        unknown["diagnostics"] = [
            {
                "category": "open",
                "dictionary_key": "dsl:sample/missing.dsl",
                "message_code": "missing-companion",
                "severity": "error",
                "source_component": "missing/test.mdd",
            }
        ]
        with self.assertRaisesRegex(result.ResultError, "unknown dictionary"):
            result.validate_observation(unknown)

        duplicate = observation()
        second = copy.deepcopy(duplicate["dictionaries"][0])  # type: ignore[index]
        second["logical_key"] = "dsl:sample/other.dsl"
        second["source_components"] = ["sample/other.dsl"]
        duplicate["dictionaries"].append(second)  # type: ignore[union-attr]
        with self.assertRaisesRegex(result.ResultError, "order values"):
            result.validate_observation(duplicate)

    def test_diagnostic_classification_fields_are_canonical_tokens(self) -> None:
        for field, value in (
            ("category", "C:/Users/dev/private/corpus"),
            ("message_code", "/private/secret.dsl"),
            ("message_code", "payload with spaces"),
        ):
            with self.subTest(field=field, value=value):
                candidate = observation()
                diagnostic = {
                    "category": "open",
                    "dictionary_key": "dsl:sample/test.dsl",
                    "message_code": "missing-companion",
                    "severity": "error",
                    "source_component": "sample/test.dsl",
                }
                diagnostic[field] = value
                candidate["diagnostics"] = [diagnostic]
                with self.assertRaisesRegex(result.ResultError, "canonical token"):
                    result.validate_observation(candidate)

    def test_rejects_unencodable_json_text_as_a_result_error(self) -> None:
        candidate = observation()
        candidate["dictionaries"][0]["name"] = "\ud800"  # type: ignore[index]
        with self.assertRaisesRegex(result.ResultError, "valid UTF-8"):
            result.validate_observation(candidate)

    def test_rejects_binding_mismatch_and_oversized_input(self) -> None:
        with self.assertRaisesRegex(result.ResultError, "expected run"):
            result.validate_observation(observation(), expected_revision=QT6_REVISION)

        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "large.json"
            path.write_bytes(b" " * (result.MAX_RESULT_BYTES + 1))
            with self.assertRaisesRegex(result.ResultError, "file-size"):
                result.read_observation(path)

    def test_oversized_input_read_is_bounded(self) -> None:
        stream = mock.MagicMock()
        stream.__enter__.return_value = stream
        stream.read.return_value = b" " * 9
        with mock.patch.object(result, "MAX_RESULT_BYTES", 8), mock.patch.object(
            Path, "open", return_value=stream
        ), self.assertRaisesRegex(result.ResultError, "file-size"):
            result.read_observation(Path("oversized.json"))
        stream.read.assert_called_once_with(9)

    def test_failed_atomic_replace_preserves_previous_observation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "observation.json"
            path.write_bytes(b"previous")
            with mock.patch.object(
                result.os, "replace", side_effect=OSError("injected")
            ), self.assertRaisesRegex(result.ResultError, "Cannot publish"):
                result.write_observation(path, observation())
            self.assertEqual(b"previous", path.read_bytes())
            self.assertEqual([path], list(path.parent.iterdir()))

    def test_failed_atomic_replace_preserves_previous_comparison(self) -> None:
        comparison = result.compare_observations(observation("qt5"), observation("qt6"))
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "comparison.json"
            path.write_bytes(b"previous")
            with mock.patch.object(
                result.os, "replace", side_effect=OSError("injected")
            ), self.assertRaisesRegex(result.ResultError, "Cannot publish"):
                result.write_comparison(path, comparison)
            self.assertEqual(b"previous", path.read_bytes())
            self.assertEqual([path], list(path.parent.iterdir()))

    def test_comparator_ignores_generated_ids_and_index_bytes(self) -> None:
        qt5 = observation("qt5")
        qt6 = observation("qt6")
        qt6["indexes"][0]["sha256"] = HASH_B  # type: ignore[index]
        qt6["indexes"][0]["size"] = 84  # type: ignore[index]

        comparison = result.compare_observations(qt5, qt6)

        self.assertTrue(comparison["equivalent"])
        self.assertEqual(0, comparison["difference_count"])
        self.assertNotEqual(
            comparison["version_specific"]["qt5_dictionary_ids"],  # type: ignore[index]
            comparison["version_specific"]["qt6_dictionary_ids"],  # type: ignore[index]
        )

    def test_comparator_reports_material_dictionary_and_diagnostic_differences(
        self,
    ) -> None:
        qt5 = observation("qt5")
        qt6 = observation("qt6")
        qt6["dictionaries"][0]["article_count"] = 4  # type: ignore[index]
        qt6["diagnostics"] = [
            {
                "category": "open",
                "dictionary_key": "dsl:sample/test.dsl",
                "message_code": "missing-companion",
                "severity": "warning",
                "source_component": "sample/test.dsl.files/icon.png",
            }
        ]

        comparison = result.compare_observations(qt5, qt6)

        self.assertFalse(comparison["equivalent"])
        self.assertEqual(2, comparison["difference_count"])
        self.assertEqual(
            ["diagnostics", "dictionaries/dsl:sample/test.dsl/article_count"],
            sorted(item["field"] for item in comparison["differences"]),  # type: ignore[union-attr]
        )

    def test_comparator_reports_disposition_for_a_shared_private_role(self) -> None:
        qt5 = observation("qt5")
        qt6 = observation("qt6")
        qt6["indexes"][0]["sha256"] = HASH_B  # type: ignore[index]
        qt6["indexes"][0]["size"] = 84  # type: ignore[index]
        qt6["indexes"][0]["file_name"] = "qt6-generated.gdidx"  # type: ignore[index]
        qt6["indexes"][0]["elapsed_milliseconds"] = 31  # type: ignore[index]
        qt6["indexes"][0]["disposition"] = "reused"  # type: ignore[index]

        comparison = result.compare_observations(qt5, qt6)

        self.assertEqual(1, comparison["difference_count"])
        self.assertEqual(
            "indexes/dsl:sample/test.dsl/headword",
            comparison["differences"][0]["field"],  # type: ignore[index]
        )

    def test_comparator_treats_private_index_roles_as_version_specific(self) -> None:
        qt5 = observation("qt5")
        qt6 = observation("qt6")
        qt6["indexes"][0]["role"] = "full-text"  # type: ignore[index]

        comparison = result.compare_observations(qt5, qt6)

        self.assertTrue(comparison["equivalent"])
        self.assertEqual(0, comparison["difference_count"])
        self.assertEqual(
            "headword",
            comparison["version_specific"]["qt5_indexes"][0]["role"],  # type: ignore[index]
        )
        self.assertEqual(
            "full-text",
            comparison["version_specific"]["qt6_indexes"][0]["role"],  # type: ignore[index]
        )

    def test_comparator_treats_version_only_index_as_version_specific(self) -> None:
        qt6 = observation("qt6")
        qt6["indexes"] = []

        comparison = result.compare_observations(observation("qt5"), qt6)

        self.assertTrue(comparison["equivalent"])
        self.assertEqual(0, comparison["difference_count"])
        self.assertEqual(
            "headword",
            comparison["version_specific"]["qt5_indexes"][0]["role"],  # type: ignore[index]
        )
        self.assertEqual(
            [], comparison["version_specific"]["qt6_indexes"]  # type: ignore[index]
        )

    def test_comparison_cross_checks_material_index_disposition_evidence(self) -> None:
        qt6 = observation("qt6")
        qt6["indexes"][0]["disposition"] = "reused"  # type: ignore[index]
        material = result.compare_observations(observation("qt5"), qt6)

        omitted = copy.deepcopy(material)
        omitted["differences"] = []
        omitted["difference_count"] = 0
        omitted["equivalent"] = True
        with self.assertRaisesRegex(result.ResultError, "index differences"):
            result.validate_comparison(omitted)

        spurious = result.compare_observations(observation("qt5"), observation("qt6"))
        spurious["differences"] = [
            {
                "field": "indexes/dsl:sample/test.dsl/headword",
                "kind": "value",
                "qt5": "created",
                "qt6": "reused",
            }
        ]
        spurious["difference_count"] = 1
        spurious["equivalent"] = False
        with self.assertRaisesRegex(result.ResultError, "index differences"):
            result.validate_comparison(spurious)

        contradictory = copy.deepcopy(material)
        contradictory["differences"][0]["qt6"] = "rebuilt"  # type: ignore[index]
        with self.assertRaisesRegex(result.ResultError, "index differences"):
            result.validate_comparison(contradictory)

    def test_comparison_canonicalizes_version_specific_index_order(self) -> None:
        qt5 = observation("qt5")
        qt6 = observation("qt6")
        for value in (qt5, qt6):
            second = copy.deepcopy(value["indexes"][0])  # type: ignore[index]
            second["role"] = "full-text"
            second["file_name"] = "full-text.idx"
            value["indexes"].append(second)  # type: ignore[union-attr]

        first = result.compare_observations(qt5, qt6)
        qt5["indexes"].reverse()  # type: ignore[union-attr]
        qt6["indexes"].reverse()  # type: ignore[union-attr]
        second = result.compare_observations(qt5, qt6)

        self.assertEqual(result.canonical_json(first), result.canonical_json(second))

    def test_comparator_compares_only_roles_present_in_both_versions(self) -> None:
        qt5 = observation("qt5")
        qt6 = observation("qt6")
        second = copy.deepcopy(qt6["indexes"][0])  # type: ignore[index]
        second["role"] = "full-text"
        second["file_name"] = "full-text.idx"
        second["disposition"] = "rebuilt"
        qt6["indexes"].append(second)  # type: ignore[union-attr]

        equivalent = result.compare_observations(qt5, qt6)
        self.assertTrue(equivalent["equivalent"])

        qt6["indexes"][0]["disposition"] = "reused"  # type: ignore[index]
        different = result.compare_observations(qt5, qt6)
        self.assertEqual(
            "reused",
            different["differences"][0]["qt6"],  # type: ignore[index]
        )

    def test_comparator_rejects_mismatched_pair_inputs(self) -> None:
        qt6 = observation("qt6")
        qt6["pair_id"] = HASH_B
        with self.assertRaisesRegex(result.ResultError, "pair_id"):
            result.compare_observations(observation("qt5"), qt6)

    def test_comparison_validator_rejects_bool_count_and_untyped_differences(
        self,
    ) -> None:
        empty = result.compare_observations(observation("qt5"), observation("qt6"))
        empty["difference_count"] = False
        with self.assertRaisesRegex(result.ResultError, "non-negative integer"):
            result.validate_comparison(empty)

        qt6 = observation("qt6")
        qt6["outcome"] = "failed"
        comparison = result.compare_observations(observation("qt5"), qt6)
        comparison["differences"][0]["kind"] = "arbitrary"  # type: ignore[index]
        with self.assertRaisesRegex(result.ResultError, "kind is unsupported"):
            result.validate_comparison(comparison)

        comparison = result.compare_observations(observation("qt5"), qt6)
        comparison["differences"][0]["qt5"] = {"not-json-serializable"}  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary_directory, self.assertRaisesRegex(
            result.ResultError, "invalid outcome"
        ):
            result.write_comparison(
                Path(temporary_directory) / "comparison.json", comparison
            )

        comparison = result.compare_observations(observation("qt5"), qt6)
        comparison["differences"][0]["qt6"] = "completed"  # type: ignore[index]
        with self.assertRaisesRegex(result.ResultError, "does not describe"):
            result.validate_comparison(comparison)

        comparison = result.compare_observations(observation("qt5"), qt6)
        comparison["differences"].append(copy.deepcopy(comparison["differences"][0]))  # type: ignore[union-attr,index]
        comparison["difference_count"] = 2
        with self.assertRaisesRegex(result.ResultError, "fields must be unique"):
            result.validate_comparison(comparison)


if __name__ == "__main__":
    unittest.main()
