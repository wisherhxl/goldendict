#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import copy
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_management_acceptance as acceptance

MANIFEST_HASH = "a" * 64
CONDITIONS_HASH = "b" * 64
PAIR_ID = "c" * 64
QT5_REVISION = "1" * 40
QT6_REVISION = "2" * 40


class RealManagementAcceptanceTest(unittest.TestCase):
    @staticmethod
    def _write_json(path: Path, value: object) -> None:
        path.write_text(
            json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    @staticmethod
    def _catalog() -> dict[str, object]:
        return {
            "conditions_sha256": CONDITIONS_HASH,
            "dictionaries": [
                {
                    "format": "dsl",
                    "id": "dsl-one",
                    "primary_component": "dsl/one.dsl.dz",
                },
                {
                    "format": "mdict",
                    "id": "mdict-two",
                    "primary_component": "mdict/two.mdx",
                },
            ],
            "manifest_sha256": MANIFEST_HASH,
            "schema": acceptance.CATALOG_SCHEMA,
            "workflow": {
                "browse": [
                    {"dictionary_id": "dsl-one", "page_size": 2},
                    {"dictionary_id": "mdict-two", "page_size": 2},
                ],
                "groups": [
                    {
                        "dictionary_ids": ["mdict-two", "dsl-one"],
                        "id": 17,
                        "muted_dictionary_ids": ["dsl-one"],
                        "name": "Reference",
                        "popup_muted_dictionary_ids": ["mdict-two"],
                    },
                    {
                        "dictionary_ids": ["dsl-one", "mdict-two"],
                        "id": 23,
                        "muted_dictionary_ids": ["mdict-two"],
                        "name": "Reading",
                        "popup_muted_dictionary_ids": [],
                    },
                ],
            },
        }

    def _fixture(self, root: Path) -> tuple[Path, dict[str, object], Path, str]:
        corpus = root / "corpus"
        (corpus / "dsl").mkdir(parents=True)
        (corpus / "mdict").mkdir()
        (corpus / "dsl" / "one.dsl.dz").write_bytes(b"dsl")
        (corpus / "mdict" / "two.mdx").write_bytes(b"mdict")
        catalog = self._catalog()
        path = root / "management-catalog.json"
        self._write_json(path, catalog)
        return (
            corpus.resolve(),
            catalog,
            path,
            hashlib.sha256(path.read_bytes()).hexdigest(),
        )

    @staticmethod
    def _raw(
        corpus: Path,
        catalog: dict[str, object],
        catalog_hash: str,
        scenario: str = "clean-discovery",
    ) -> dict[str, object]:
        dictionaries = []
        for item in catalog["dictionaries"]:
            dictionaries.append(
                {
                    "article_count": 11,
                    "catalog_id": item["id"],
                    "components": [str(corpus / item["primary_component"])],
                    "description": f"Description for {item['id']}",
                    "headword_count": 12,
                    "name": f"Name for {item['id']}",
                    "source_language": "en",
                    "target_language": "en",
                }
            )
        browse = []
        for request in catalog["workflow"]["browse"]:
            browse.append(
                {
                    "dictionary_id": request["dictionary_id"],
                    "headwords": ["alpha", "beta", "gamma", "delta"],
                }
            )
        return {
            "browse": browse,
            "catalog_sha256": catalog_hash,
            "conditions_sha256": CONDITIONS_HASH,
            "dictionaries": dictionaries,
            "groups": copy.deepcopy(catalog["workflow"]["groups"]),
            "rescan_dictionary_ids": [item["id"] for item in catalog["dictionaries"]],
            "scenario": scenario,
            "schema": acceptance.RAW_SCHEMA,
        }

    @staticmethod
    def _normalize(
        raw: dict[str, object],
        catalog: dict[str, object],
        catalog_hash: str,
        corpus: Path,
        *,
        version: str = "qt5",
        revision: str = QT5_REVISION,
        scenario: str = "clean-discovery",
    ) -> dict[str, object]:
        return acceptance._normalize_raw(
            raw,
            catalog,
            catalog_hash=catalog_hash,
            corpus_root=corpus,
            pair_id=PAIR_ID,
            version=version,
            revision=revision,
            scenario=scenario,
            manifest_hash=MANIFEST_HASH,
            conditions_hash=CONDITIONS_HASH,
        )

    def test_catalog_accepts_confined_management_matrix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, path, catalog_hash = self._fixture(Path(temporary))
            actual, actual_hash = acceptance.read_catalog(
                path,
                corpus,
                expected_manifest_hash=MANIFEST_HASH,
                expected_conditions_hash=CONDITIONS_HASH,
            )
            self.assertEqual(catalog, actual)
            self.assertEqual(catalog_hash, actual_hash)

    def test_catalog_rejects_escaping_and_nonmember_mute_references(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, path, _ = self._fixture(Path(temporary))
            catalog["dictionaries"][0]["primary_component"] = "../outside.dsl"
            self._write_json(path, catalog)
            with self.assertRaisesRegex(
                acceptance.ManagementAcceptanceError, "safe POSIX path"
            ):
                acceptance.read_catalog(path, corpus)

            catalog = self._catalog()
            catalog["workflow"]["groups"][0]["dictionary_ids"] = ["dsl-one"]
            self._write_json(path, catalog)
            with self.assertRaisesRegex(
                acceptance.ManagementAcceptanceError, "escapes membership"
            ):
                acceptance.read_catalog(path, corpus)

    def test_normalization_hashes_private_text_and_keeps_management_state(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, _, catalog_hash = self._fixture(Path(temporary))
            normalized = self._normalize(
                self._raw(corpus, catalog, catalog_hash),
                catalog,
                catalog_hash,
                corpus,
            )
            description = normalized["dictionaries"][0]["description"]
            self.assertEqual(
                hashlib.sha256(b"Description for dsl-one").hexdigest(),
                description["sha256"],
            )
            self.assertEqual(catalog["workflow"]["groups"], normalized["groups"])
            self.assertEqual(
                hashlib.sha256(b"alpha").hexdigest(),
                normalized["browse"][0]["headwords"][0]["sha256"],
            )
            self.assertNotIn("alpha", json.dumps(normalized["browse"]))

    def test_normalization_compares_visible_description_tokens(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, _, catalog_hash = self._fixture(Path(temporary))
            qt5 = self._raw(corpus, catalog, catalog_hash)
            qt6 = copy.deepcopy(qt5)
            qt5["dictionaries"][1]["description"] = "Title\n\n\ufffc Details\u00a0here"
            qt6["dictionaries"][1]["description"] = "  Title   Details\n here  "

            qt5_normalized = self._normalize(qt5, catalog, catalog_hash, corpus)
            qt6_normalized = self._normalize(
                qt6,
                catalog,
                catalog_hash,
                corpus,
                version="qt6",
                revision=QT6_REVISION,
            )

            self.assertEqual(
                qt5_normalized["dictionaries"][1]["description"],
                qt6_normalized["dictionaries"][1]["description"],
            )

    def test_normalization_rejects_changed_persistence_or_rescan_selection(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, _, catalog_hash = self._fixture(Path(temporary))
            raw = self._raw(corpus, catalog, catalog_hash)
            raw["groups"][0]["name"] = "Changed"
            with self.assertRaisesRegex(
                acceptance.ManagementAcceptanceError, "groups do not match"
            ):
                self._normalize(raw, catalog, catalog_hash, corpus)

            raw = self._raw(corpus, catalog, catalog_hash)
            raw["rescan_dictionary_ids"].pop()
            with self.assertRaisesRegex(
                acceptance.ManagementAcceptanceError, "rescan changed"
            ):
                self._normalize(raw, catalog, catalog_hash, corpus)

    def test_comparison_detects_cross_version_or_restart_difference(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            corpus, catalog, _, catalog_hash = self._fixture(root)
            pair = {
                "conditions_sha256": CONDITIONS_HASH,
                "manifest_sha256": MANIFEST_HASH,
                "pair_id": PAIR_ID,
                "qt5_revision": QT5_REVISION,
                "qt6_revision": QT6_REVISION,
            }
            paths: dict[str, Path] = {}
            for version, revision in (("qt5", QT5_REVISION), ("qt6", QT6_REVISION)):
                for scenario in ("clean-discovery", "warm-restart"):
                    raw = self._raw(corpus, catalog, catalog_hash, scenario)
                    observation = self._normalize(
                        raw,
                        catalog,
                        catalog_hash,
                        corpus,
                        version=version,
                        revision=revision,
                        scenario=scenario,
                    )
                    key = f"{version}-{scenario}"
                    paths[key] = root / f"{key}.json"
                    self._write_json(paths[key], observation)

            result = acceptance.compare(
                paths["qt5-clean-discovery"],
                paths["qt5-warm-restart"],
                paths["qt6-clean-discovery"],
                paths["qt6-warm-restart"],
                catalog=catalog,
                catalog_hash=catalog_hash,
                pair=pair,
            )
            self.assertTrue(result["equivalent"])
            self.assertEqual(0, result["difference_count"])

            changed = json.loads(paths["qt6-warm-restart"].read_text(encoding="utf-8"))
            changed["dictionaries"][0]["name"] = "Different"
            self._write_json(paths["qt6-warm-restart"], changed)
            result = acceptance.compare(
                paths["qt5-clean-discovery"],
                paths["qt5-warm-restart"],
                paths["qt6-clean-discovery"],
                paths["qt6-warm-restart"],
                catalog=catalog,
                catalog_hash=catalog_hash,
                pair=pair,
            )
            self.assertFalse(result["equivalent"])
            self.assertEqual(1, result["difference_count"])
            self.assertEqual("qt6-warm", result["differences"][0]["observation"])

    def test_comparison_rejects_catalog_incomplete_observations(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            corpus, catalog, _, catalog_hash = self._fixture(root)
            pair = {
                "conditions_sha256": CONDITIONS_HASH,
                "manifest_sha256": MANIFEST_HASH,
                "pair_id": PAIR_ID,
                "qt5_revision": QT5_REVISION,
                "qt6_revision": QT6_REVISION,
            }
            paths: dict[str, Path] = {}
            observations: dict[str, dict[str, object]] = {}
            for version, revision in (("qt5", QT5_REVISION), ("qt6", QT6_REVISION)):
                for scenario in ("clean-discovery", "warm-restart"):
                    raw = self._raw(corpus, catalog, catalog_hash, scenario)
                    observation = self._normalize(
                        raw,
                        catalog,
                        catalog_hash,
                        corpus,
                        version=version,
                        revision=revision,
                        scenario=scenario,
                    )
                    key = f"{version}-{scenario}"
                    observations[key] = observation
                    paths[key] = root / f"{key}.json"
                    self._write_json(paths[key], observation)

            cases = (
                ("dictionaries", "dictionary count mismatches catalog"),
                ("groups", "groups do not match catalog"),
                ("browse", "browse count mismatches catalog"),
            )
            for field, message in cases:
                with self.subTest(field=field):
                    changed = copy.deepcopy(observations["qt6-warm-restart"])
                    changed[field] = []
                    self._write_json(paths["qt6-warm-restart"], changed)
                    with self.assertRaisesRegex(
                        acceptance.ManagementAcceptanceError, message
                    ):
                        acceptance.compare(
                            paths["qt5-clean-discovery"],
                            paths["qt5-warm-restart"],
                            paths["qt6-clean-discovery"],
                            paths["qt6-warm-restart"],
                            catalog=catalog,
                            catalog_hash=catalog_hash,
                            pair=pair,
                        )

    def test_difference_collection_enforces_bound_on_every_append(self) -> None:
        differences: list[dict[str, object]] = []
        with (
            mock.patch.object(acceptance, "MAX_DIFFERENCES", 2),
            self.assertRaisesRegex(
                acceptance.ManagementAcceptanceError, "exceeds its difference bound"
            ),
        ):
            acceptance._collect_differences(
                ["one", "two", "three"],
                [],
                "management",
                "qt6-warm",
                differences,
            )
        self.assertEqual(2, len(differences))


if __name__ == "__main__":
    unittest.main()
