#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_dictionary_acceptance_mutation as mutation


class AcceptanceMutationTest(unittest.TestCase):
    def _fixture(self, root: Path) -> tuple[dict[str, str], dict[str, Path]]:
        corpus = root / "corpus"
        evidence = root / "evidence"
        corpus.mkdir()
        evidence.mkdir()
        marker = corpus / mutation.DISPOSABLE_MARKER
        marker.write_text(mutation.MARKER_CONTENT, encoding="utf-8")
        source = corpus / "fixture.idx"
        companion = corpus / "fixture.dict"
        source.write_bytes(b"index source")
        companion.write_bytes(b"dictionary source")
        return (
            {
                "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT": str(corpus),
                "GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT": str(evidence),
            },
            {
                "companion": companion,
                "corpus": corpus,
                "evidence": evidence,
                "marker": marker,
                "source": source,
            },
        )

    def test_changed_source_updates_only_timestamp_and_projects_scenario(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            before = paths["source"].stat()
            with mock.patch.dict(
                os.environ, environment, clear=True
            ), mock.patch.object(
                mutation.subprocess,
                "run",
                return_value=subprocess.CompletedProcess([], 0),
            ) as run:
                self.assertEqual(
                    0,
                    mutation.run_mutation(
                        paths["corpus"],
                        "fixture.idx",
                        "fixture.dict",
                        "changed-source",
                        ["observer"],
                    ),
                )
            after = paths["source"].stat()
            self.assertEqual(before.st_size, after.st_size)
            self.assertGreater(after.st_mtime_ns, before.st_mtime_ns)
            self.assertEqual(b"index source", paths["source"].read_bytes())
            run.assert_called_once_with(
                ["observer", "--scenario", "changed-source"], check=False
            )

    def test_unavailable_companion_is_restored_before_return(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))

            def run(
                command: list[str], **kwargs: object
            ) -> subprocess.CompletedProcess:
                self.assertFalse(paths["companion"].exists())
                quarantines = list(paths["evidence"].glob(".unavailable-companion-*"))
                self.assertEqual(1, len(quarantines))
                self.assertEqual(
                    b"dictionary source",
                    (quarantines[0] / "fixture.dict").read_bytes(),
                )
                return subprocess.CompletedProcess(command, 0)

            with mock.patch.dict(
                os.environ, environment, clear=True
            ), mock.patch.object(mutation.subprocess, "run", side_effect=run):
                self.assertEqual(
                    0,
                    mutation.run_mutation(
                        paths["corpus"],
                        "fixture.idx",
                        "fixture.dict",
                        "unavailable-companion",
                        ["observer"],
                    ),
                )
            self.assertEqual(b"dictionary source", paths["companion"].read_bytes())
            self.assertEqual(
                [], list(paths["evidence"].glob(".unavailable-companion-*"))
            )

    def test_unavailable_companion_is_restored_after_child_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            with mock.patch.dict(
                os.environ, environment, clear=True
            ), mock.patch.object(
                mutation.subprocess,
                "run",
                return_value=subprocess.CompletedProcess([], 7),
            ):
                self.assertEqual(
                    7,
                    mutation.run_mutation(
                        paths["corpus"],
                        "fixture.idx",
                        "fixture.dict",
                        "unavailable-companion",
                        ["observer"],
                    ),
                )
            self.assertEqual(b"dictionary source", paths["companion"].read_bytes())

    def test_rejects_unmarked_corpus_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            paths["marker"].unlink()
            with mock.patch.dict(
                os.environ, environment, clear=True
            ), mock.patch.object(
                mutation.subprocess, "run"
            ) as run, self.assertRaisesRegex(
                mutation.MutationError, "marker"
            ):
                mutation.run_mutation(
                    paths["corpus"],
                    "fixture.idx",
                    "fixture.dict",
                    "changed-source",
                    ["observer"],
                )
            run.assert_not_called()

    def test_rejects_scenario_override_and_path_escape(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            environment, paths = self._fixture(Path(temporary))
            with mock.patch.dict(os.environ, environment, clear=True):
                with self.assertRaisesRegex(mutation.MutationError, "override"):
                    mutation.run_mutation(
                        paths["corpus"],
                        "fixture.idx",
                        "fixture.dict",
                        "clean-discovery",
                        ["observer", "--scenario", "changed-source"],
                    )
                with self.assertRaisesRegex(mutation.MutationError, "normalized"):
                    mutation.run_mutation(
                        paths["corpus"],
                        "../outside.idx",
                        "fixture.dict",
                        "changed-source",
                        ["observer"],
                    )


if __name__ == "__main__":
    unittest.main()
