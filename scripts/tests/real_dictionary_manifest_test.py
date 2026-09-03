#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_dictionary_manifest


class RealDictionaryManifestTest(unittest.TestCase):
    def _write(self, path: Path, content: bytes) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def test_manifest_is_deterministic_sorted_and_payload_free(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus = root / "private corpus"
            output = root / "evidence" / "manifest.json"
            private_payload = b"do-not-copy-payload"
            self._write(corpus / "中文" / "B.MDX", private_payload)
            self._write(corpus / "a" / "words.DSL.DZ", b"dsl")
            self._write(corpus / "a" / "resource.MdD", b"resource")

            real_dictionary_manifest.create_manifest(corpus, output)
            first = output.read_bytes()
            real_dictionary_manifest.create_manifest(corpus, output)
            second = output.read_bytes()
            parsed = json.loads(first)

            self.assertEqual(first, second)
            self.assertEqual(
                ["a/resource.MdD", "a/words.DSL.DZ", "中文/B.MDX"],
                [record["path"] for record in parsed["files"]],
            )
            self.assertEqual(3, parsed["file_count"])
            self.assertEqual(
                len(private_payload) + len(b"dsl") + len(b"resource"),
                parsed["total_bytes"],
            )
            self.assertNotIn(str(corpus), first.decode("utf-8"))
            self.assertNotIn(private_payload.decode("ascii"), first.decode("utf-8"))

    def test_records_size_hash_and_classification(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus = root / "corpus"
            samples = {
                "a.MDX": (b"a", "mdict_dictionary"),
                "b.mdd": (b"bb", "mdict_resource"),
                "c.dsl.dz": (b"ccc", "dsl_dictionary"),
                "d.DZ": (b"dddd", "compressed_data"),
                "e.ZIP": (b"eeeee", "resource_archive"),
                "f.AFF": (b"ffffff", "hunspell_affix"),
                "g.dic": (b"ggggggg", "hunspell_dictionary"),
                "h.ann": (b"hhhhhhhh", "annotation"),
                "i.CSS": (b"i" * 9, "stylesheet"),
                "j.JPG": (b"j" * 10, "image"),
                "k.ttf": (b"k" * 11, "font"),
                "COPYING": (b"l" * 12, "other"),
            }
            for name, (content, _) in samples.items():
                self._write(corpus / name, content)

            manifest = real_dictionary_manifest.build_manifest(corpus)
            records = {record["path"]: record for record in manifest["files"]}

            for name, (content, classification) in samples.items():
                self.assertEqual(classification, records[name]["classification"])
                self.assertEqual(len(content), records[name]["size"])
                self.assertEqual(
                    hashlib.sha256(content).hexdigest(), records[name]["sha256"]
                )

    def test_rejects_output_inside_corpus_before_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            corpus = Path(temporary_directory) / "corpus"
            self._write(corpus / "dictionary.dsl", b"data")
            output = corpus / "generated" / "manifest.json"

            with self.assertRaisesRegex(
                real_dictionary_manifest.ManifestError, "outside the corpus"
            ):
                real_dictionary_manifest.create_manifest(corpus, output)

            self.assertFalse(output.exists())

    def test_failed_atomic_replace_preserves_existing_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus = root / "corpus"
            output = root / "evidence" / "manifest.json"
            self._write(corpus / "dictionary.dsl", b"data")
            self._write(output, b"previous manifest")

            with mock.patch.object(
                real_dictionary_manifest.os,
                "replace",
                side_effect=OSError("injected replace failure"),
            ):
                with self.assertRaisesRegex(
                    real_dictionary_manifest.ManifestError,
                    "Cannot write manifest",
                ):
                    real_dictionary_manifest.create_manifest(corpus, output)

            self.assertEqual(b"previous manifest", output.read_bytes())
            self.assertEqual([output], list(output.parent.iterdir()))

    def test_rejects_symlink_or_reparse_point(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus = root / "corpus"
            target = root / "outside.dsl"
            self._write(target, b"outside")
            corpus.mkdir()
            link = corpus / "linked.dsl"
            try:
                link.symlink_to(target)
            except OSError as error:
                self.skipTest(f"Symbolic links are unavailable: {error}")

            with self.assertRaisesRegex(
                real_dictionary_manifest.ManifestError,
                "links and reparse points",
            ):
                real_dictionary_manifest.build_manifest(corpus)

    def test_rejects_detected_reparse_point_without_platform_privileges(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            corpus = Path(temporary_directory) / "corpus"
            self._write(corpus / "dictionary.dsl", b"data")

            with mock.patch.object(
                real_dictionary_manifest,
                "_is_reparse_point",
                side_effect=[False, True],
            ):
                with self.assertRaisesRegex(
                    real_dictionary_manifest.ManifestError,
                    "links and reparse points",
                ):
                    real_dictionary_manifest.build_manifest(corpus)

    def test_rejects_file_changed_while_hashing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            corpus = Path(temporary_directory) / "corpus"
            dictionary = corpus / "dictionary.dsl"
            self._write(dictionary, b"before")
            original_stream = real_dictionary_manifest._stream_sha256

            def mutate_after_read(source: object) -> str:
                digest = original_stream(source)  # type: ignore[arg-type]
                with dictionary.open("ab") as changed:
                    changed.write(b"after")
                return digest

            with mock.patch.object(
                real_dictionary_manifest,
                "_stream_sha256",
                side_effect=mutate_after_read,
            ):
                with self.assertRaisesRegex(
                    real_dictionary_manifest.ManifestError,
                    "changed while hashing",
                ):
                    real_dictionary_manifest.build_manifest(corpus)

    def test_rejects_file_added_after_initial_inventory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            corpus = Path(temporary_directory) / "corpus"
            dictionary = corpus / "a.dsl"
            self._write(dictionary, b"first")
            original_hash = real_dictionary_manifest._hash_regular_file

            def add_after_hash(path: Path) -> tuple[int, str, tuple[int, int, int, int]]:
                result = original_hash(path)
                self._write(corpus / "b.dsl", b"second")
                return result

            with mock.patch.object(
                real_dictionary_manifest,
                "_hash_regular_file",
                side_effect=add_after_hash,
            ):
                with self.assertRaisesRegex(
                    real_dictionary_manifest.ManifestError,
                    "inventory changed",
                ):
                    real_dictionary_manifest.build_manifest(corpus)

    def test_rejects_earlier_file_changed_after_its_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            corpus = Path(temporary_directory) / "corpus"
            first = corpus / "a.dsl"
            second = corpus / "b.dsl"
            self._write(first, b"first")
            self._write(second, b"second")
            original_hash = real_dictionary_manifest._hash_regular_file

            def change_first_after_second_hash(
                path: Path,
            ) -> tuple[int, str, tuple[int, int, int, int]]:
                result = original_hash(path)
                if path == second:
                    first.write_bytes(b"changed-first")
                return result

            with mock.patch.object(
                real_dictionary_manifest,
                "_hash_regular_file",
                side_effect=change_first_after_second_hash,
            ):
                with self.assertRaisesRegex(
                    real_dictionary_manifest.ManifestError,
                    "file changed while generating",
                ):
                    real_dictionary_manifest.build_manifest(corpus)

    def test_rejects_missing_and_non_directory_corpus(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            missing = root / "missing"
            regular_file = root / "file.dsl"
            regular_file.write_bytes(b"data")

            with self.assertRaises(real_dictionary_manifest.ManifestError):
                real_dictionary_manifest.build_manifest(missing)
            with self.assertRaisesRegex(
                real_dictionary_manifest.ManifestError, "not a directory"
            ):
                real_dictionary_manifest.build_manifest(regular_file)

    def test_cli_returns_error_without_creating_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            output = root / "manifest.json"

            with mock.patch.object(sys, "stderr"):
                result = real_dictionary_manifest.main([str(root / "missing"), str(output)])

            self.assertEqual(2, result)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
