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
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_mdict_acceptance as acceptance

MANIFEST_HASH = "a" * 64
CONDITIONS_HASH = "b" * 64
CATALOG_HASH = ""
COMPONENTS = [
    "dict/book.mdx",
    "dict/book.mdd",
    "dict/book.1.mdd",
    "dict/book.2.mdd",
]


def _catalog() -> dict[str, object]:
    probes = []
    for index, component in enumerate(COMPONENTS[1:]):
        content = f"resource-{index}".encode()
        probes.append(
            {
                "id": f"probe-{index}",
                "query": {"match_mode": "exact", "text": f"word-{index}"},
                "resource": {
                    "id": f"sound-{index}.mp3",
                    "sha256": hashlib.sha256(content).hexdigest(),
                    "size": len(content),
                    "source_component": component,
                },
            }
        )
    return {
        "conditions_sha256": CONDITIONS_HASH,
        "dictionary": {
            "ordered_components": COMPONENTS,
            "primary_component": COMPONENTS[0],
        },
        "manifest_sha256": MANIFEST_HASH,
        "probes": probes,
        "schema": acceptance.CATALOG_SCHEMA,
    }


def _raw(root: Path, catalog_hash: str, markup_prefix: str = "") -> dict[str, object]:
    probes = []
    catalog = _catalog()
    for index, probe in enumerate(catalog["probes"]):  # type: ignore[index]
        resource = f"resource-{index}".encode()
        resource_id = probe["resource"]["id"]  # type: ignore[index]
        probes.append(
            {
                "article_markup": (
                    f"{markup_prefix}<style>hidden</style><div>Visible {index} "
                    f"<a href='{resource_id}'>resource</a></div>"
                ),
                "headword": probe["query"]["text"],  # type: ignore[index]
                "id": probe["id"],
                "query": probe["query"]["text"],  # type: ignore[index]
                "resource_data_base64": base64.b64encode(resource).decode(),
                "resource_id": resource_id,
                "resource_media_type": "audio/mpeg",
            }
        )
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": CONDITIONS_HASH,
        "dictionary": {
            "components": [str(root / Path(component)) for component in COMPONENTS],
            "id": "mdict-fixture",
            "name": "Fixture",
        },
        "errors": [],
        "probes": probes,
        "scenario": "clean-discovery",
        "schema": acceptance.RAW_SCHEMA,
    }


class RealMdictAcceptanceTest(unittest.TestCase):
    def _fixture(self, root: Path) -> tuple[Path, Path, dict[str, object], str]:
        for component in COMPONENTS:
            path = root / Path(component)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(component.encode())
        catalog = _catalog()
        catalog_path = root / "catalog.json"
        content = (json.dumps(catalog, ensure_ascii=False, indent=2) + "\n").encode()
        catalog_path.write_bytes(content)
        return root, catalog_path, catalog, hashlib.sha256(content).hexdigest()

    def _normalize(
        self,
        root: Path,
        catalog: dict[str, object],
        catalog_hash: str,
        *,
        version: str = "qt5",
        scenario: str = "clean-discovery",
        markup_prefix: str = "",
    ) -> dict[str, object]:
        raw = _raw(root, catalog_hash, markup_prefix)
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

    def test_validates_catalog_and_normalizes_bounded_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, catalog_path, catalog, catalog_hash = self._fixture(Path(temporary))
            self.assertEqual(
                acceptance.read_catalog(
                    catalog_path,
                    expected_manifest_hash=MANIFEST_HASH,
                    expected_conditions_hash=CONDITIONS_HASH,
                )[1],
                catalog_hash,
            )
            result = self._normalize(root, catalog, catalog_hash)
            article = result["probes"][0]["article"]  # type: ignore[index]
            self.assertEqual(
                article["visible_text_sha256"],
                hashlib.sha256(b"Visible 0 resource").hexdigest(),
            )
            self.assertNotIn("article_markup", json.dumps(result))

    def test_rejects_resource_bytes_that_do_not_match_catalog(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, _, catalog, catalog_hash = self._fixture(Path(temporary))
            raw = _raw(root, catalog_hash)
            raw["probes"][0]["resource_data_base64"] = base64.b64encode(  # type: ignore[index]
                b"wrong"
            ).decode()
            with self.assertRaisesRegex(
                acceptance.MdictAcceptanceError, "resource bytes do not match"
            ):
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

    def test_compares_visible_behavior_and_retains_markup_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, _, catalog, catalog_hash = self._fixture(Path(temporary))
            observations = {
                "qt5_clean": self._normalize(root, catalog, catalog_hash),
                "qt5_warm": self._normalize(
                    root, catalog, catalog_hash, scenario="warm-restart"
                ),
                "qt6_clean": self._normalize(
                    root,
                    catalog,
                    catalog_hash,
                    version="qt6",
                    markup_prefix="<!-- Qt 6 framing -->",
                ),
                "qt6_warm": self._normalize(
                    root,
                    catalog,
                    catalog_hash,
                    version="qt6",
                    scenario="warm-restart",
                    markup_prefix="<!-- Qt 6 framing -->",
                ),
            }
            paths: dict[str, Path] = {}
            for name, value in observations.items():
                path = root / f"{name}.json"
                path.write_text(json.dumps(value), encoding="utf-8")
                paths[name] = path
            comparison = acceptance.compare(
                paths["qt5_clean"],
                paths["qt5_warm"],
                paths["qt6_clean"],
                paths["qt6_warm"],
            )
            self.assertTrue(comparison["equivalent"])
            self.assertEqual(comparison["differences"], [])
            self.assertNotEqual(
                comparison["raw_markup_sha256"]["qt5-clean"],  # type: ignore[index]
                comparison["raw_markup_sha256"]["qt6-clean"],  # type: ignore[index]
            )

            changed = copy.deepcopy(observations["qt6_warm"])
            changed["probes"][0]["article"]["visible_text_sha256"] = "f" * 64  # type: ignore[index]
            paths["qt6_warm"].write_text(json.dumps(changed), encoding="utf-8")
            comparison = acceptance.compare(
                paths["qt5_clean"],
                paths["qt5_warm"],
                paths["qt6_clean"],
                paths["qt6_warm"],
            )
            self.assertFalse(comparison["equivalent"])
            self.assertEqual(comparison["differences"][0]["observation"], "qt6-warm")  # type: ignore[index]

    def test_publication_rolls_back_when_acknowledgement_write_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "observation.json"
            acknowledgement = root / "condition-ack.json"

            def fail_acknowledgement(path: Path, value: object) -> None:
                path.write_text(json.dumps(value), encoding="utf-8")
                if path == acknowledgement:
                    raise OSError("injected acknowledgement failure")

            with (
                mock.patch.object(
                    acceptance, "_atomic_json", side_effect=fail_acknowledgement
                ),
                self.assertRaisesRegex(
                    acceptance.MdictAcceptanceError, "could not be published"
                ),
            ):
                acceptance._publish_observation_pair(
                    output,
                    {"schema": "observation"},
                    acknowledgement,
                    {"schema": "acknowledgement"},
                )

            self.assertFalse(output.exists())
            self.assertFalse(acknowledgement.exists())


if __name__ == "__main__":
    unittest.main()
