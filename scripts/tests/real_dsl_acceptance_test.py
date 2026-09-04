#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import base64
import copy
import hashlib
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_dsl_acceptance as acceptance

MANIFEST_HASH = "a" * 64
CONDITIONS_HASH = "b" * 64


def _catalog(root: Path) -> dict[str, object]:
    dictionaries = []
    for index in range(5):
        source = root / f"dict-{index}" / "fixture.dsl"
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text("entry\n\tarticle", encoding="utf-8")
        archive = source.with_name("fixture.dsl.files.zip")
        resource_id = f"sound-{index}.wav"
        resource = f"resource-{index}".encode()
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
            output.writestr(resource_id, resource)
        dictionaries.append(
            {
                "additional_components": [],
                "archive": {
                    "aggregate_uncompressed_size": len(resource),
                    "component": archive.relative_to(root).as_posix(),
                    "member_count": 1,
                    "size": archive.stat().st_size,
                },
                "primary_component": source.relative_to(root).as_posix(),
                "probe": {
                    "id": f"probe-{index}",
                    "query": {"match_mode": "exact", "text": f"word-{index}"},
                    "resource": {
                        "id": resource_id,
                        "sha256": hashlib.sha256(resource).hexdigest(),
                        "size": len(resource),
                    },
                },
            }
        )
    orphan = root / "orphan" / "orphan.dsl.files.zip"
    orphan.parent.mkdir(parents=True)
    with zipfile.ZipFile(orphan, "w") as output:
        output.writestr("orphan.wav", b"orphan")
    return {
        "conditions_sha256": CONDITIONS_HASH,
        "dictionaries": dictionaries,
        "manifest_sha256": MANIFEST_HASH,
        "orphan_archives": [
            {
                "aggregate_uncompressed_size": 6,
                "component": orphan.relative_to(root).as_posix(),
                "member_count": 1,
                "resource": {
                    "id": "orphan.wav",
                    "sha256": hashlib.sha256(b"orphan").hexdigest(),
                    "size": 6,
                },
                "size": orphan.stat().st_size,
            }
        ],
        "schema": acceptance.CATALOG_SCHEMA,
    }


def _write_catalog(root: Path, catalog: dict[str, object]) -> Path:
    path = root / "catalog.json"
    path.write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return path


def _raw(
    root: Path,
    catalog: dict[str, object],
    catalog_hash: str,
    *,
    available: bool = True,
) -> dict[str, object]:
    dictionaries = []
    for index, expected in enumerate(catalog["dictionaries"]):  # type: ignore[index]
        probe = expected["probe"]
        resource = f"resource-{index}".encode()
        resource_id = probe["resource"]["id"]
        components = [str(root / Path(expected["primary_component"]))]
        components.extend(
            str(root / Path(component))
            for component in expected["additional_components"]
        )
        if available:
            components.append(str(root / Path(expected["archive"]["component"])))
        dictionaries.append(
            {
                "archive_owned": available,
                "components": components,
                "id": f"dsl-{index}",
                "name": f"DSL {index}",
                "probe": {
                    "article_markup": f"<div>Visible {index} {resource_id}</div>",
                    "headword": probe["query"]["text"],
                    "id": probe["id"],
                    "query": probe["query"]["text"],
                    "resource_available": available,
                    "resource_data_base64": (
                        base64.b64encode(resource).decode() if available else ""
                    ),
                    "resource_id": resource_id,
                    "resource_media_type": "audio/wav",
                },
            }
        )
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": CONDITIONS_HASH,
        "dictionaries": dictionaries,
        "errors": [],
        "orphan_archive_owned": False,
        "scenario": "clean-discovery",
        "schema": acceptance.RAW_SCHEMA,
    }


def _pair_contract() -> dict[str, object]:
    return {
        "conditions_sha256": CONDITIONS_HASH,
        "manifest_sha256": MANIFEST_HASH,
        "pair_id": "c" * 64,
        "revisions": {"qt5": "d" * 40, "qt6": "e" * 40},
    }


class RealDslAcceptanceTest(unittest.TestCase):
    def test_catalog_binds_archive_inventory_and_resources(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            observed, digest = acceptance.read_catalog(
                path,
                root,
                expected_manifest_hash=MANIFEST_HASH,
                expected_conditions_hash=CONDITIONS_HASH,
            )
            self.assertEqual(observed, catalog)
            self.assertEqual(digest, hashlib.sha256(path.read_bytes()).hexdigest())

    def test_catalog_rejects_changed_archive_inventory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            catalog["dictionaries"][0]["archive"]["member_count"] = 2  # type: ignore[index]
            with self.assertRaisesRegex(acceptance.DslAcceptanceError, "member count"):
                acceptance.read_catalog(_write_catalog(root, catalog), root)

    def test_catalog_rejects_archive_symlink_outside_corpus(self) -> None:
        with tempfile.TemporaryDirectory() as temporary, tempfile.TemporaryDirectory() as outside_temporary:
            root = Path(temporary)
            outside = Path(outside_temporary) / "outside.zip"
            with zipfile.ZipFile(outside, "w") as output:
                output.writestr("resource.wav", b"resource")
            catalog = _catalog(root)
            linked = root / "linked.zip"
            try:
                linked.symlink_to(outside)
            except OSError as error:
                self.skipTest(f"symlink creation is unavailable: {error}")
            archive = catalog["dictionaries"][0]["archive"]  # type: ignore[index]
            archive.update(
                {
                    "aggregate_uncompressed_size": 8,
                    "component": "linked.zip",
                    "member_count": 1,
                    "size": outside.stat().st_size,
                }
            )
            with self.assertRaisesRegex(
                acceptance.DslAcceptanceError, "outside the bound corpus"
            ):
                acceptance.read_catalog(_write_catalog(root, catalog), root)

    def test_metadata_bound_is_checked_before_read(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "oversized.json"
            with path.open("wb") as output:
                output.seek(acceptance.MAX_METADATA_BYTES)
                output.write(b"x")
            with mock.patch.object(
                Path, "open", side_effect=AssertionError("must not read")
            ) as opened:
                with self.assertRaisesRegex(
                    acceptance.DslAcceptanceError, "metadata size bound"
                ):
                    acceptance.read_catalog(path, Path(temporary))
                opened.assert_not_called()

    def test_confinement_rejects_canonical_escape(self) -> None:
        with tempfile.TemporaryDirectory() as temporary, tempfile.TemporaryDirectory() as outside_temporary:
            root = Path(temporary).resolve()
            outside = Path(outside_temporary).resolve() / "outside.zip"
            with mock.patch.object(
                Path, "resolve", return_value=outside
            ), self.assertRaisesRegex(
                acceptance.DslAcceptanceError, "outside the bound corpus"
            ):
                acceptance._confined_file(root, "linked.zip", "Archive")

    def test_normalization_retains_missing_archive_resource(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)
            normalized = acceptance.normalize_raw_observation(
                _raw(root, catalog, catalog_hash, available=False),
                catalog,
                catalog_hash=catalog_hash,
                corpus_root=root,
                pair_id="c" * 64,
                version="qt6",
                revision="d" * 40,
                scenario="clean-discovery",
                manifest_hash=MANIFEST_HASH,
                conditions_hash=CONDITIONS_HASH,
            )
            first = normalized["dictionaries"][0]  # type: ignore[index]
            self.assertFalse(first["archive_owned"])
            self.assertFalse(first["probe"]["resource"]["available"])
            self.assertIsNone(first["probe"]["resource"]["sha256"])

    def test_normalization_rejects_wrong_resource_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)
            raw = _raw(root, catalog, catalog_hash)
            raw["dictionaries"][0]["probe"]["resource_data_base64"] = (  # type: ignore[index]
                base64.b64encode(b"wrong").decode()
            )
            with self.assertRaisesRegex(acceptance.DslAcceptanceError, "do not match"):
                acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=catalog_hash,
                    corpus_root=root,
                    pair_id="c" * 64,
                    version="qt5",
                    revision="d" * 40,
                    scenario="clean-discovery",
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

    def test_normalization_accepts_successful_qt6_archive_shape(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)
            normalized = acceptance.normalize_raw_observation(
                _raw(root, catalog, catalog_hash),
                catalog,
                catalog_hash=catalog_hash,
                corpus_root=root,
                pair_id="c" * 64,
                version="qt6",
                revision="e" * 40,
                scenario="clean-discovery",
                manifest_hash=MANIFEST_HASH,
                conditions_hash=CONDITIONS_HASH,
            )
            self.assertTrue(normalized["dictionaries"][0]["archive_owned"])  # type: ignore[index]

    def test_normalization_rejects_unbound_component(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            unexpected = root / "unexpected.txt"
            unexpected.write_text("unexpected", encoding="utf-8")
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)
            raw = _raw(root, catalog, catalog_hash)
            raw["dictionaries"][0]["components"].append(str(unexpected))  # type: ignore[index]
            with self.assertRaisesRegex(
                acceptance.DslAcceptanceError, "components do not match"
            ):
                acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=catalog_hash,
                    corpus_root=root,
                    pair_id="c" * 64,
                    version="qt6",
                    revision="e" * 40,
                    scenario="clean-discovery",
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

    def test_comparison_retains_cross_version_component_difference(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            additional = root / "dict-0" / "fixture-abbreviations.dsl"
            additional.write_text("abbr\n\texpansion", encoding="utf-8")
            catalog["dictionaries"][0]["additional_components"] = [  # type: ignore[index]
                additional.relative_to(root).as_posix()
            ]
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)

            def normalized(version: str, scenario: str, omit_additional: bool):
                raw = _raw(root, catalog, catalog_hash)
                raw["scenario"] = scenario
                if omit_additional:
                    del raw["dictionaries"][0]["components"][1]  # type: ignore[index]
                return acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=catalog_hash,
                    corpus_root=root,
                    pair_id="c" * 64,
                    version=version,
                    revision=("d" if version == "qt5" else "e") * 40,
                    scenario=scenario,
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

            files = {}
            for name, value in {
                "qt5-clean": normalized("qt5", "clean-discovery", False),
                "qt5-warm": normalized("qt5", "warm-restart", False),
                "qt6-clean": normalized("qt6", "clean-discovery", True),
                "qt6-warm": normalized("qt6", "warm-restart", True),
            }.items():
                files[name] = root / f"{name}.json"
                files[name].write_text(json.dumps(value), encoding="utf-8")
            result = acceptance.compare(
                files["qt5-clean"],
                files["qt5-warm"],
                files["qt6-clean"],
                files["qt6-warm"],
                catalog=catalog,
                catalog_hash=catalog_hash,
                pair=_pair_contract(),
            )
            self.assertFalse(result["equivalent"])
            self.assertEqual(
                [item["observation"] for item in result["differences"]],
                ["qt6-clean", "qt6-warm"],
            )
            fields = result["differences"][0]["fields"]
            component_difference = next(
                item for item in fields if item["field"] == "dictionary.components"
            )
            self.assertEqual(len(component_difference["expected"]), 3)
            self.assertEqual(len(component_difference["actual"]), 2)

    def test_comparison_retains_observed_orphan_attachment(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)

            def normalized(version: str, scenario: str, orphan_owned: bool):
                raw = _raw(root, catalog, catalog_hash)
                raw["scenario"] = scenario
                raw["orphan_archive_owned"] = orphan_owned
                return acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=catalog_hash,
                    corpus_root=root,
                    pair_id="c" * 64,
                    version=version,
                    revision=("d" if version == "qt5" else "e") * 40,
                    scenario=scenario,
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

            files = {}
            for name, value in {
                "qt5-clean": normalized("qt5", "clean-discovery", False),
                "qt5-warm": normalized("qt5", "warm-restart", False),
                "qt6-clean": normalized("qt6", "clean-discovery", True),
                "qt6-warm": normalized("qt6", "warm-restart", True),
            }.items():
                files[name] = root / f"{name}.json"
                files[name].write_text(json.dumps(value), encoding="utf-8")
            result = acceptance.compare(
                files["qt5-clean"],
                files["qt5-warm"],
                files["qt6-clean"],
                files["qt6-warm"],
                catalog=catalog,
                catalog_hash=catalog_hash,
                pair=_pair_contract(),
            )
            self.assertFalse(result["equivalent"])
            self.assertEqual(
                [item["observation"] for item in result["differences"]],
                ["qt6-clean", "qt6-warm"],
            )
            orphan_difference = result["differences"][0]["fields"][0]
            self.assertEqual(orphan_difference["field"], "orphan_archive_owned")
            self.assertFalse(orphan_difference["expected"])
            self.assertTrue(orphan_difference["actual"])

    def test_read_observation_rejects_corrupted_nested_payloads(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)
            valid = acceptance.normalize_raw_observation(
                _raw(root, catalog, catalog_hash),
                catalog,
                catalog_hash=catalog_hash,
                corpus_root=root,
                pair_id="c" * 64,
                version="qt6",
                revision="e" * 40,
                scenario="clean-discovery",
                manifest_hash=MANIFEST_HASH,
                conditions_hash=CONDITIONS_HASH,
            )
            corruptions = []
            malformed_hash = copy.deepcopy(valid)
            malformed_hash["dictionaries"][0]["probe"]["article"][  # type: ignore[index]
                "markup_sha256"
            ] = "invalid"
            corruptions.append(malformed_hash)
            impossible_resource = copy.deepcopy(valid)
            impossible_resource["dictionaries"][0]["probe"]["resource"][  # type: ignore[index]
                "available"
            ] = False
            corruptions.append(impossible_resource)
            wrong_valid_hash = copy.deepcopy(valid)
            wrong_valid_hash["dictionaries"][0]["probe"]["resource"][  # type: ignore[index]
                "sha256"
            ] = ("f" * 64)
            corruptions.append(wrong_valid_hash)
            changed_headword = copy.deepcopy(valid)
            changed_headword["dictionaries"][0]["probe"]["headword"] = "other"  # type: ignore[index]
            corruptions.append(changed_headword)
            invented_archive = copy.deepcopy(valid)
            invented_archive["dictionaries"][0]["archive_component"] = (  # type: ignore[index]
                "dict-0/invented.dsl.files.zip"
            )
            invented_archive["dictionaries"][0]["dictionary"]["components"][  # type: ignore[index]
                -1
            ] = "dict-0/invented.dsl.files.zip"
            corruptions.append(invented_archive)
            extra_field = copy.deepcopy(valid)
            extra_field["dictionaries"][0]["probe"]["extra"] = True  # type: ignore[index]
            corruptions.append(extra_field)
            missing_field = copy.deepcopy(valid)
            del missing_field["dictionaries"][0]["dictionary"]["name"]  # type: ignore[index]
            corruptions.append(missing_field)
            for index, corruption in enumerate(corruptions):
                evidence = root / f"corrupt-{index}.json"
                evidence.write_text(json.dumps(corruption), encoding="utf-8")
                with self.subTest(index=index), self.assertRaises(
                    acceptance.DslAcceptanceError
                ):
                    acceptance.read_observation(
                        evidence,
                        catalog,
                        catalog_hash=catalog_hash,
                        pair=_pair_contract(),
                        expected_version="qt6",
                        expected_scenario="clean-discovery",
                    )

    def test_comparison_rejects_cross_run_revision_change(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)

            def normalized(version: str, scenario: str):
                raw = _raw(root, catalog, catalog_hash)
                raw["scenario"] = scenario
                return acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=catalog_hash,
                    corpus_root=root,
                    pair_id="c" * 64,
                    version=version,
                    revision=("d" if version == "qt5" else "e") * 40,
                    scenario=scenario,
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

            observations = {
                "qt5-clean": normalized("qt5", "clean-discovery"),
                "qt5-warm": normalized("qt5", "warm-restart"),
                "qt6-clean": normalized("qt6", "clean-discovery"),
                "qt6-warm": normalized("qt6", "warm-restart"),
            }
            observations["qt5-warm"]["revision"] = "a" * 40
            files = {}
            for name, value in observations.items():
                files[name] = root / f"{name}.json"
                files[name].write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(
                acceptance.DslAcceptanceError, "pair or catalog"
            ):
                acceptance.compare(
                    files["qt5-clean"],
                    files["qt5-warm"],
                    files["qt6-clean"],
                    files["qt6-warm"],
                    catalog=catalog,
                    catalog_hash=catalog_hash,
                    pair=_pair_contract(),
                )

    def test_comparison_publishes_product_difference(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            catalog = _catalog(root)
            path = _write_catalog(root, catalog)
            _, catalog_hash = acceptance.read_catalog(path, root)

            def normalized(version: str, scenario: str, available: bool):
                raw = _raw(root, catalog, catalog_hash, available=available)
                raw["scenario"] = scenario
                return acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=catalog_hash,
                    corpus_root=root,
                    pair_id="c" * 64,
                    version=version,
                    revision=("d" if version == "qt5" else "e") * 40,
                    scenario=scenario,
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

            files = {}
            for name, value in {
                "qt5-clean": normalized("qt5", "clean-discovery", True),
                "qt5-warm": normalized("qt5", "warm-restart", True),
                "qt6-clean": normalized("qt6", "clean-discovery", False),
                "qt6-warm": normalized("qt6", "warm-restart", False),
            }.items():
                files[name] = root / f"{name}.json"
                files[name].write_text(json.dumps(value), encoding="utf-8")
            result = acceptance.compare(
                files["qt5-clean"],
                files["qt5-warm"],
                files["qt6-clean"],
                files["qt6-warm"],
                catalog=catalog,
                catalog_hash=catalog_hash,
                pair=_pair_contract(),
            )
            self.assertFalse(result["equivalent"])
            self.assertEqual(
                [item["observation"] for item in result["differences"]],
                ["qt6-clean", "qt6-warm"],
            )
            fields = result["differences"][0]["fields"]
            self.assertTrue(
                any(
                    item["field"] == "resource.available"
                    and item["expected"] is True
                    and item["actual"] is False
                    for item in fields
                )
            )


if __name__ == "__main__":
    unittest.main()
