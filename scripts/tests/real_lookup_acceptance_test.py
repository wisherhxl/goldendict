#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_lookup_acceptance as acceptance

MANIFEST_HASH = "a" * 64
CONDITIONS_HASH = "b" * 64
CATALOG_HASH = "c" * 64
PAIR_ID = "d" * 64
QT5_REVISION = "1" * 40
QT6_REVISION = "2" * 40


class RealLookupAcceptanceTest(unittest.TestCase):
    def _write_json(self, path: Path, value: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    def _catalog(self) -> dict[str, object]:
        def probe(identifier: str, operation: str, category: str, query: str):
            return {
                "category": category,
                "id": identifier,
                "operation": operation,
                "query": query,
                "result_limit": 4,
            }

        return {
            "conditions_sha256": CONDITIONS_HASH,
            "dictionaries": [
                {
                    "format": "dsl",
                    "id": "dsl-english",
                    "primary_component": "dsl/example.dsl.dz",
                    "probes": [
                        probe("dsl-exact", "lookup", "exact", "example"),
                        probe("dsl-alias", "lookup", "alias", "an example"),
                        probe("dsl-unicode", "lookup", "unicode", "例"),
                        probe("dsl-suggest", "suggest", "suggestion", "exa"),
                    ],
                },
                {
                    "format": "mdict",
                    "id": "mdict-english",
                    "primary_component": "mdict/example.mdx",
                    "probes": [
                        probe("mdict-media", "lookup", "media", "sound"),
                        probe("mdict-missing", "lookup", "missing", "absent"),
                        probe("mdict-multi", "lookup", "multi-word", "two words"),
                        probe("mdict-punctuation", "lookup", "punctuation", "isn't"),
                        probe("mdict-suggest", "suggest", "suggestion", "sou"),
                    ],
                },
            ],
            "manifest_sha256": MANIFEST_HASH,
            "schema": acceptance.CATALOG_SCHEMA,
        }

    def _fixture(self, root: Path):
        corpus = root / "corpus"
        (corpus / "dsl").mkdir(parents=True)
        (corpus / "mdict").mkdir()
        (corpus / "dsl" / "example.dsl.dz").write_bytes(b"dsl")
        (corpus / "mdict" / "example.mdx").write_bytes(b"mdict")
        catalog = self._catalog()
        catalog_path = root / "catalog.json"
        self._write_json(catalog_path, catalog)
        catalog_hash = hashlib.sha256(catalog_path.read_bytes()).hexdigest()
        return corpus, catalog, catalog_path, catalog_hash

    def _raw(self, corpus: Path, catalog: dict[str, object]) -> dict[str, object]:
        dictionaries = []
        for item in catalog["dictionaries"]:
            components = [str(corpus / Path(item["primary_component"]))]
            if item["format"] == "hunspell":
                components.append(str(Path(components[0]).with_suffix(".dic")))
            probes = []
            for probe in item["probes"]:
                entries = []
                suggestions = []
                if probe["operation"] == "lookup" and probe["category"] != "missing":
                    entries.append(
                        {
                            "article_markup": (
                                '<div class="dsl_definition" '
                                'style="font-family: Test; color: red">'
                                ' Visible <a href="bword:target">link</a>'
                                '<audio src="gdau://dictionary/audio/test.wav"></audio>'
                                ' <a href="https://example.invalid/">external</a>'
                                "</div>"
                            ),
                            "headword": probe["query"],
                            "plain_text": "Visible link external",
                            "resources": [
                                {"id": "audio/test.wav", "media_type": "audio/wav"}
                            ],
                        }
                    )
                elif probe["operation"] == "suggest":
                    suggestions = [probe["query"] + "mple"]
                elif probe["operation"] == "morphology":
                    suggestions = [probe["query"].rstrip("s")]
                probes.append(
                    {
                        "entries": entries,
                        "errors": [],
                        "id": probe["id"],
                        "operation": probe["operation"],
                        "suggestions": suggestions,
                    }
                )
            dictionaries.append(
                {
                    "catalog_id": item["id"],
                    "components": components,
                    "id": "runtime-" + item["id"],
                    "name": item["id"],
                    "probes": probes,
                }
            )
        return {
            "catalog_sha256": CATALOG_HASH,
            "conditions_sha256": CONDITIONS_HASH,
            "dictionaries": dictionaries,
            "errors": [],
            "scenario": "clean-discovery",
            "schema": acceptance.RAW_SCHEMA,
        }

    def test_catalog_requires_complete_bounded_family_matrix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, path, expected_hash = self._fixture(Path(temporary))
            actual, catalog_hash = acceptance.read_catalog(
                path,
                corpus,
                expected_manifest_hash=MANIFEST_HASH,
                expected_conditions_hash=CONDITIONS_HASH,
            )
            self.assertEqual(catalog, actual)
            self.assertEqual(expected_hash, catalog_hash)
            queries = acceptance.catalog_queries(actual)
            self.assertEqual(9, len(queries))
            self.assertEqual("dsl-english", queries[0]["dictionary"])

    def test_catalog_rejects_escape_and_incomplete_cases(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            corpus, catalog, path, _ = self._fixture(root)
            catalog["dictionaries"][0]["primary_component"] = "../outside.dsl"
            self._write_json(path, catalog)
            with self.assertRaisesRegex(acceptance.LookupAcceptanceError, "confined"):
                acceptance.read_catalog(path, corpus)

            catalog = self._catalog()
            catalog["dictionaries"][0]["probes"] = catalog["dictionaries"][0]["probes"][
                :-1
            ]
            self._write_json(path, catalog)
            with self.assertRaisesRegex(
                acceptance.LookupAcceptanceError, "lookup and suggestion"
            ):
                acceptance.read_catalog(path, corpus)

    def test_catalog_rejects_inconsistent_suggestion_operation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            corpus, catalog, path, _ = self._fixture(root)
            catalog["dictionaries"][0]["probes"][0]["operation"] = "suggest"
            self._write_json(path, catalog)
            with self.assertRaisesRegex(
                acceptance.LookupAcceptanceError, "inconsistent"
            ):
                acceptance.read_catalog(path, corpus)

    def test_catalog_accepts_all_real_hunspell_pairs_and_morphology(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            corpus, catalog, path, _ = self._fixture(root)
            morphology = corpus / "morphology"
            morphology.mkdir()
            for dictionary_id in (
                "de_DE",
                "en_US",
                "es_ES",
                "fr_FR",
                "it_IT",
                "pt_BR",
                "ru_RU",
            ):
                (morphology / f"{dictionary_id}.aff").write_bytes(b"SET UTF-8\n")
                (morphology / f"{dictionary_id}.dic").write_bytes(b"1\nword\n")
                catalog["dictionaries"].append(
                    {
                        "format": "hunspell",
                        "id": dictionary_id,
                        "primary_component": f"morphology/{dictionary_id}.aff",
                        "probes": [
                            {
                                "category": "morphology",
                                "id": f"{dictionary_id}-stem",
                                "operation": "morphology",
                                "query": "words",
                                "result_limit": 8,
                            }
                        ],
                    }
                )
            self._write_json(path, catalog)

            actual, catalog_hash = acceptance.read_catalog(path, corpus)

            self.assertEqual(16, len(acceptance.catalog_queries(actual)))
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), catalog_hash)
            raw = self._raw(corpus, actual)
            normalized = acceptance.normalize_raw_observation(
                raw,
                actual,
                catalog_hash=CATALOG_HASH,
                corpus_root=corpus.resolve(),
                pair_id=PAIR_ID,
                version="qt6",
                revision=QT6_REVISION,
                scenario="clean-discovery",
                manifest_hash=MANIFEST_HASH,
                conditions_hash=CONDITIONS_HASH,
            )
            hunspell_probe = normalized["dictionaries"][-1]["probes"][0]
            self.assertEqual("morphology", hunspell_probe["operation"])
            self.assertEqual(1, len(hunspell_probe["suggestions"]))
            self.assertEqual(
                ["morphology/ru_RU.aff", "morphology/ru_RU.dic"],
                normalized["dictionaries"][-1]["components"],
            )

    def test_normalization_hashes_text_and_retains_references(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, _, _ = self._fixture(Path(temporary))
            normalized = acceptance.normalize_raw_observation(
                self._raw(corpus, catalog),
                catalog,
                catalog_hash=CATALOG_HASH,
                corpus_root=corpus.resolve(),
                pair_id=PAIR_ID,
                version="qt5",
                revision=QT5_REVISION,
                scenario="clean-discovery",
                manifest_hash=MANIFEST_HASH,
                conditions_hash=CONDITIONS_HASH,
            )
            article = normalized["dictionaries"][0]["probes"][0]["entries"][0]
            self.assertEqual(
                [
                    {
                        "sha256": hashlib.sha256(b"target").hexdigest(),
                        "size": len(b"target"),
                    }
                ],
                article["links"],
            )
            self.assertEqual(["audio/test.wav"], article["resources"])
            self.assertEqual({"audio/test.wav": "audio/wav"}, article["media_types"])
            self.assertEqual(
                [
                    {
                        "sha256": hashlib.sha256(
                            b"https://example.invalid/"
                        ).hexdigest(),
                        "size": len(b"https://example.invalid/"),
                    }
                ],
                article["external_links"],
            )
            self.assertIn("dsl_definition", article["styles"])
            self.assertIn("color:red", article["styles"])
            self.assertEqual(
                hashlib.sha256(b"Visible link external").hexdigest(),
                article["visible_text_sha256"],
            )
            suggestion = normalized["dictionaries"][0]["probes"][3]["suggestions"][0]
            raw_suggestion = catalog["dictionaries"][0]["probes"][3]["query"] + "mple"
            self.assertEqual(
                hashlib.sha256(raw_suggestion.encode("utf-8")).hexdigest(),
                suggestion["sha256"],
            )
            self.assertNotIn(
                raw_suggestion,
                json.dumps(normalized["dictionaries"][0]["probes"][3]["suggestions"]),
            )

    def test_normalization_uses_reported_source_headword_not_query(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, _, _ = self._fixture(Path(temporary))
            raw = self._raw(corpus, catalog)
            raw["dictionaries"][0]["probes"][0]["entries"][0][
                "headword"
            ] = "Actual Source Headword"
            normalized = acceptance.normalize_raw_observation(
                raw,
                catalog,
                catalog_hash=CATALOG_HASH,
                corpus_root=corpus.resolve(),
                pair_id=PAIR_ID,
                version="qt6",
                revision=QT6_REVISION,
                scenario="clean-discovery",
                manifest_hash=MANIFEST_HASH,
                conditions_hash=CONDITIONS_HASH,
            )
            article = normalized["dictionaries"][0]["probes"][0]["entries"][0]
            self.assertEqual(
                hashlib.sha256(b"Actual Source Headword").hexdigest(),
                article["source_headwords"][0]["sha256"],
            )
            self.assertNotEqual(
                hashlib.sha256(b"example").hexdigest(),
                article["source_headwords"][0]["sha256"],
            )

    def test_style_normalization_matches_equivalent_legacy_markup(self) -> None:
        legacy = acceptance._article(
            '<span class="dsl_b">Bold</span>'
            '<span class="dsl_opt"> optional</span>'
            '<a class="dsl_ref" href="bword:target"> target</a>',
            [],
        )
        modern = acceptance._article(
            '<b>Bold</b><span class="gd-optional-part"> optional</span>'
            '<a href="goldendict://lookup/target"> target</a>',
            [],
        )
        self.assertEqual(legacy, modern)

    def test_article_retains_source_and_displayed_headwords_without_private_text(
        self,
    ) -> None:
        displayed = acceptance._article(
            '<div class="DSL_HEADWORDS">Canonical Alias</div><p>Definition</p>',
            [],
            "Selected Source Headword",
        )
        self.assertEqual(
            [
                {
                    "sha256": hashlib.sha256(b"Canonical Alias").hexdigest(),
                    "size": len(b"Canonical Alias"),
                }
            ],
            displayed["displayed_headwords"],
        )
        self.assertEqual(
            [
                {
                    "sha256": hashlib.sha256(b"Selected Source Headword").hexdigest(),
                    "size": len(b"Selected Source Headword"),
                }
            ],
            displayed["source_headwords"],
        )
        self.assertNotIn("Canonical Alias", json.dumps(displayed))
        self.assertNotIn("Selected Source Headword", json.dumps(displayed))

    def test_article_structure_detects_style_relocation(self) -> None:
        left_bold = acceptance._article("<b>left</b> right", [], "entry")
        right_bold = acceptance._article("left <b>right</b>", [], "entry")
        self.assertEqual(left_bold["styles"], right_bold["styles"])
        self.assertEqual(
            left_bold["visible_text_sha256"], right_bold["visible_text_sha256"]
        )
        self.assertNotEqual(
            left_bold["styled_text_runs"], right_bold["styled_text_runs"]
        )

    def test_exact_private_text_signatures_preserve_whitespace_differences(
        self,
    ) -> None:
        self.assertNotEqual(
            acceptance._exact_text_signature("term x"),
            acceptance._exact_text_signature("term  x"),
        )

        source_single = acceptance._article("definition", [], "term x")
        source_double = acceptance._article("definition", [], "term  x")
        self.assertNotEqual(
            source_single["source_headwords"], source_double["source_headwords"]
        )

        link_single = acceptance._article(
            '<a href="bword:target%20x">same</a>', [], "entry"
        )
        link_double = acceptance._article(
            '<a href="bword:target%20%20x">same</a>', [], "entry"
        )
        self.assertNotEqual(link_single["links"], link_double["links"])
        self.assertNotEqual(
            link_single["reference_sequence"], link_double["reference_sequence"]
        )

        styled_single = acceptance._article("<b>a </b>b", [], "entry")
        styled_double = acceptance._article("<b>a</b> b", [], "entry")
        self.assertEqual(
            styled_single["visible_text_sha256"],
            styled_double["visible_text_sha256"],
        )
        self.assertEqual(
            styled_single["styled_text_runs"], styled_double["styled_text_runs"]
        )

    def test_semantic_text_preserves_non_collapsible_html_spacing(self) -> None:
        ordinary = acceptance._article(
            '<span class="dsl_headwords">a b</span>', [], "entry"
        )
        nonbreaking = acceptance._article(
            '<span class="dsl_headwords">a&nbsp;b</span>', [], "entry"
        )
        em_space = acceptance._article(
            '<span class="dsl_headwords">a\u2003b</span>', [], "entry"
        )

        self.assertNotEqual(
            ordinary["visible_text_sha256"], nonbreaking["visible_text_sha256"]
        )
        self.assertNotEqual(
            ordinary["visible_text_sha256"], em_space["visible_text_sha256"]
        )
        self.assertNotEqual(
            ordinary["displayed_headwords"], nonbreaking["displayed_headwords"]
        )
        self.assertNotEqual(
            ordinary["displayed_headwords"], em_space["displayed_headwords"]
        )
        self.assertNotEqual(
            ordinary["styled_text_runs"], nonbreaking["styled_text_runs"]
        )
        self.assertNotEqual(ordinary["styled_text_runs"], em_space["styled_text_runs"])

    def test_article_structure_retains_reference_binding_order_and_multiplicity(
        self,
    ) -> None:
        original = acceptance._article(
            '<a href="bword:a">one</a><a href="bword:b">two</a>'
            '<a href="bword:b">two again</a>',
            [],
            "entry",
        )
        reassigned = acceptance._article(
            '<a href="bword:b">one</a><a href="bword:a">two</a>'
            '<a href="bword:b">two again</a>',
            [],
            "entry",
        )
        self.assertEqual(original["links"], reassigned["links"])
        self.assertEqual(
            [
                {
                    "kind": "link",
                    "target": {
                        "sha256": hashlib.sha256(b"a").hexdigest(),
                        "size": 1,
                    },
                },
                {
                    "kind": "link",
                    "target": {
                        "sha256": hashlib.sha256(b"b").hexdigest(),
                        "size": 1,
                    },
                },
                {
                    "kind": "link",
                    "target": {
                        "sha256": hashlib.sha256(b"b").hexdigest(),
                        "size": 1,
                    },
                },
            ],
            original["reference_sequence"],
        )
        self.assertNotEqual(
            original["reference_sequence"], reassigned["reference_sequence"]
        )
        self.assertNotEqual(
            original["styled_text_runs"], reassigned["styled_text_runs"]
        )

    def test_article_structure_retains_script_resource_without_script_text(
        self,
    ) -> None:
        article = acceptance._article(
            '<script src="bres:oalecd9.js">private executable text</script>'
            "visible text",
            [],
            "entry",
        )
        self.assertEqual(["oalecd9.js"], article["resources"])
        self.assertEqual(
            [{"kind": "resource", "target": "oalecd9.js"}],
            article["reference_sequence"],
        )
        self.assertEqual(
            hashlib.sha256(b"visible text").hexdigest(),
            article["visible_text_sha256"],
        )

    def test_article_structure_retains_mdict_layout_class(self) -> None:
        wrapped = acceptance._article(
            '<div class="mdict">definition</div>', [], "entry"
        )
        unwrapped = acceptance._article("<div>definition</div>", [], "entry")
        self.assertEqual(
            wrapped["visible_text_sha256"], unwrapped["visible_text_sha256"]
        )
        self.assertIn("mdict", wrapped["styles"])
        self.assertNotEqual(wrapped["styled_text_runs"], unwrapped["styled_text_runs"])

    def test_article_structure_anchors_non_text_resource_position(self) -> None:
        before = acceptance._article('<img src="bres:image.png">visible', [], "entry")
        after = acceptance._article('visible<img src="bres:image.png">', [], "entry")
        self.assertEqual(before["resources"], after["resources"])
        self.assertEqual(before["reference_sequence"], after["reference_sequence"])
        self.assertEqual(before["styled_text_runs"], after["styled_text_runs"])
        self.assertNotEqual(before["content_sequence"], after["content_sequence"])

    def test_normalization_rejects_raw_probe_reordering(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            corpus, catalog, _, _ = self._fixture(Path(temporary))
            raw = self._raw(corpus, catalog)
            raw["dictionaries"][0]["probes"].reverse()
            with self.assertRaisesRegex(
                acceptance.LookupAcceptanceError, "does not match"
            ):
                acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=CATALOG_HASH,
                    corpus_root=corpus.resolve(),
                    pair_id=PAIR_ID,
                    version="qt5",
                    revision=QT5_REVISION,
                    scenario="clean-discovery",
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

    def test_compare_retains_product_differences(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            corpus, catalog, _, _ = self._fixture(root)

            def observation(version: str, revision: str, scenario: str):
                raw = self._raw(corpus, catalog)
                raw["scenario"] = scenario
                return acceptance.normalize_raw_observation(
                    raw,
                    catalog,
                    catalog_hash=CATALOG_HASH,
                    corpus_root=corpus.resolve(),
                    pair_id=PAIR_ID,
                    version=version,
                    revision=revision,
                    scenario=scenario,
                    manifest_hash=MANIFEST_HASH,
                    conditions_hash=CONDITIONS_HASH,
                )

            values = {
                "qt5-clean": observation("qt5", QT5_REVISION, "clean-discovery"),
                "qt5-warm": observation("qt5", QT5_REVISION, "warm-restart"),
                "qt6-clean": observation("qt6", QT6_REVISION, "clean-discovery"),
                "qt6-warm": observation("qt6", QT6_REVISION, "warm-restart"),
            }
            styles = values["qt6-clean"]["dictionaries"][0]["probes"][0]["entries"][0][
                "styles"
            ]
            values["qt6-clean"]["dictionaries"][0]["probes"][0]["entries"][0][
                "styles"
            ] = styles[:-1]
            paths = {}
            for name, value in values.items():
                paths[name] = root / f"{name}.json"
                self._write_json(paths[name], value)
            pair = {
                "conditions_sha256": CONDITIONS_HASH,
                "manifest_sha256": MANIFEST_HASH,
                "pair_id": PAIR_ID,
                "revisions": {"qt5": QT5_REVISION, "qt6": QT6_REVISION},
            }
            comparison = acceptance.compare(
                paths["qt5-clean"],
                paths["qt5-warm"],
                paths["qt6-clean"],
                paths["qt6-warm"],
                catalog=catalog,
                catalog_hash=CATALOG_HASH,
                pair=pair,
            )
            self.assertFalse(comparison["equivalent"])
            self.assertEqual(1, comparison["difference_count"])
            self.assertEqual("qt6-clean", comparison["differences"][0]["observation"])
            self.assertIn("/styles/", comparison["differences"][0]["field"])


if __name__ == "__main__":
    unittest.main()
