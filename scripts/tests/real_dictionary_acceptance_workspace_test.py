#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import real_dictionary_acceptance_workspace
import real_dictionary_manifest


QT5_REVISION = "3d93dd66197aea10edf6c29998ddc9c213d0aaa8"
QT6_REVISION = "18f3cfc5c308d730e9b52f5b69c579d585b73c14"


class RealDictionaryAcceptanceWorkspaceTest(unittest.TestCase):
    def _write_json(self, path: Path, value: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    def _fixture(self, root: Path) -> tuple[Path, Path, Path, dict[str, object]]:
        corpus = root / "corpus"
        corpus.mkdir()
        (corpus / "dictionary.dsl").write_bytes(b"dictionary payload")
        manifest_path = root / "input" / "manifest.json"
        manifest_path.parent.mkdir()
        real_dictionary_manifest.create_manifest(corpus, manifest_path)
        conditions = {
            "group": {"name": "All", "ordered_dictionary_ids": []},
            "locale": "en_US",
            "platform": {
                "architecture": "x86_64",
                "display_scale_percent": 125,
                "operating_system": "windows",
                "style": "windowsvista",
                "theme": "light",
            },
            "preferences": {"interface_language": "en_US"},
            "queries": ["example", "missing"],
            "schema": real_dictionary_acceptance_workspace.CONDITIONS_SCHEMA,
        }
        conditions_path = root / "input" / "conditions.json"
        self._write_json(conditions_path, conditions)
        return corpus, manifest_path, conditions_path, conditions

    def _acknowledging_child_program(self, extra_statement: str = "") -> str:
        return (
            "import json, os, pathlib; "
            "ack = {'conditions_sha256': "
            "os.environ['GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256'], "
            "'corpus_manifest_sha256': "
            "os.environ['GOLDENDICT_ACCEPTANCE_CORPUS_MANIFEST_SHA256'], "
            "'pair_id': os.environ['GOLDENDICT_ACCEPTANCE_PAIR_ID'], "
            "'revision': os.environ['GOLDENDICT_ACCEPTANCE_REVISION'], "
            f"'schema': '{real_dictionary_acceptance_workspace.ACKNOWLEDGEMENT_SCHEMA}', "
            "'version': os.environ['GOLDENDICT_ACCEPTANCE_VERSION']}; "
            "pathlib.Path(os.environ['GOLDENDICT_ACCEPTANCE_ACK_PATH']).write_text("
            "json.dumps(ack, sort_keys=True), encoding='utf-8'); "
            + extra_statement
        )

    def _create(self, root: Path, workspace_name: str = "workspace") -> tuple[Path, str]:
        corpus, manifest, conditions, _ = self._fixture(root)
        workspace = root / workspace_name
        pair_id = real_dictionary_acceptance_workspace.create_workspace(
            workspace,
            corpus,
            manifest,
            conditions,
            QT5_REVISION,
            QT6_REVISION,
        )
        return workspace, pair_id

    def test_creates_and_validates_paired_isolated_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            corpus_before = real_dictionary_manifest.build_manifest(corpus)
            workspace = root / "acceptance"

            pair_id = real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            validated = real_dictionary_acceptance_workspace.validate_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )

            self.assertEqual(pair_id, validated)
            self.assertEqual(corpus_before, real_dictionary_manifest.build_manifest(corpus))
            self.assertTrue((workspace / "pair.json").is_file())
            for version in real_dictionary_acceptance_workspace.VERSIONS:
                self.assertTrue((workspace / version / "run.json").is_file())
                self.assertTrue((workspace / version / "environment.json").is_file())
                for name in real_dictionary_acceptance_workspace.DIRECTORY_NAMES:
                    self.assertTrue((workspace / version / name).is_dir())

    def test_qt5_and_qt6_environment_paths_are_confined_and_disjoint(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            workspace, _ = self._create(root)
            environments: dict[str, set[Path]] = {}
            for version in real_dictionary_acceptance_workspace.VERSIONS:
                environment = json.loads(
                    (workspace / version / "environment.json").read_text(encoding="utf-8")
                )
                paths = {Path(value).resolve() for value in environment.values()}
                version_root = (workspace / version).resolve()
                self.assertTrue(
                    all(
                        real_dictionary_manifest.is_within(path, version_root)
                        for path in paths
                    )
                )
                environments[version] = paths

            self.assertFalse(environments["qt5"] & environments["qt6"])

    def test_pair_metadata_is_location_independent(self) -> None:
        with tempfile.TemporaryDirectory() as first_directory:
            with tempfile.TemporaryDirectory() as second_directory:
                first_workspace, first_id = self._create(Path(first_directory))
                second_workspace, second_id = self._create(Path(second_directory))

                self.assertEqual(first_id, second_id)
                self.assertEqual(
                    (first_workspace / "pair.json").read_bytes(),
                    (second_workspace / "pair.json").read_bytes(),
                )

    def test_rejects_workspace_inside_corpus(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "must be disjoint",
            ):
                real_dictionary_acceptance_workspace.create_workspace(
                    corpus / "output",
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_rejects_existing_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            workspace.mkdir()

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "already exists",
            ):
                real_dictionary_acceptance_workspace.create_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_rejects_incomplete_conditions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, value = self._fixture(root)
            del value["platform"]
            self._write_json(conditions, value)

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "platform must be an object",
            ):
                real_dictionary_acceptance_workspace.create_workspace(
                    root / "workspace",
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_rejects_non_commit_revision(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "40-digit commit ID",
            ):
                real_dictionary_acceptance_workspace.create_workspace(
                    root / "workspace",
                    corpus,
                    manifest,
                    conditions,
                    "master",
                    QT6_REVISION,
                )

    def test_rejects_missing_metadata_with_domain_error(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus = root / "corpus"
            corpus.mkdir()

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "Corpus manifest does not exist",
            ):
                real_dictionary_acceptance_workspace.create_workspace(
                    root / "workspace",
                    corpus,
                    root / "missing-manifest.json",
                    root / "missing-conditions.json",
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_validation_rejects_changed_conditions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, value = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            value["locale"] = "ru_RU"
            self._write_json(conditions, value)

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "conditions do not match",
            ):
                real_dictionary_acceptance_workspace.validate_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_validation_rejects_changed_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["total_bytes"] += 1
            self._write_json(manifest, value)

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "Corpus content does not match",
            ):
                real_dictionary_acceptance_workspace.validate_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_validation_rejects_unrelated_corpus(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            unrelated = root / "unrelated-corpus"
            unrelated.mkdir()
            (unrelated / "other.dsl").write_bytes(b"different")

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "Corpus content does not match",
            ):
                real_dictionary_acceptance_workspace.validate_workspace(
                    workspace,
                    unrelated,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_validation_rejects_revision_without_external_match(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "revision does not match the expected commit",
            ):
                real_dictionary_acceptance_workspace.validate_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    "0" * 40,
                )

    def test_validation_rejects_run_metadata_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            run_path = workspace / "qt6" / "run.json"
            run = json.loads(run_path.read_text(encoding="utf-8"))
            run["conditions_sha256"] = "0" * 64
            self._write_json(run_path, run)

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "run metadata does not match",
            ):
                real_dictionary_acceptance_workspace.validate_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_validation_rejects_required_directory_replaced_by_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            required_directory = workspace / "qt6" / "temp"
            shutil.rmtree(required_directory)
            required_directory.write_bytes(b"not a directory")

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "required run directory is missing: temp",
            ):
                real_dictionary_acceptance_workspace.validate_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_validation_rejects_noncanonical_pair_run_reference(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            pair_path = workspace / "pair.json"
            pair = json.loads(pair_path.read_text(encoding="utf-8"))
            pair["runs"]["qt6"]["metadata"] = "qt5/run.json"
            self._write_json(pair_path, pair)

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "run reference is not canonical",
            ):
                real_dictionary_acceptance_workspace.validate_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                )

    def test_failed_atomic_publication_leaves_no_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"

            with mock.patch.object(
                real_dictionary_acceptance_workspace.os,
                "replace",
                side_effect=OSError("injected publication failure"),
            ):
                with self.assertRaisesRegex(
                    real_dictionary_acceptance_workspace.WorkspaceError,
                    "Cannot publish workspace",
                ):
                    real_dictionary_acceptance_workspace.create_workspace(
                        workspace,
                        corpus,
                        manifest,
                        conditions,
                        QT5_REVISION,
                        QT6_REVISION,
                    )

            self.assertFalse(workspace.exists())
            self.assertEqual(
                [root / "corpus", root / "input"], sorted(root.iterdir())
            )

    def test_run_projects_isolated_environment_and_working_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            child_program = self._acknowledging_child_program(
                "target = pathlib.Path(os.environ['GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT']) "
                "/ 'child.json'; "
                "target.write_text(json.dumps({'cwd': str(pathlib.Path.cwd()), "
                "'home': os.environ['HOME'], "
                "'parent_runtime': os.environ['GD_TEST_PARENT_RUNTIME'], "
                "'conditions_file': "
                "os.environ['GOLDENDICT_ACCEPTANCE_CONDITIONS_FILE']}), "
                "encoding='utf-8')"
            )

            with mock.patch.dict(
                os.environ, {"GD_TEST_PARENT_RUNTIME": "active"}, clear=False
            ):
                result = real_dictionary_acceptance_workspace.run_in_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    [
                        sys.executable,
                        "-c",
                        child_program,
                        "--dictionary-root",
                        str(corpus),
                    ],
                )

            self.assertEqual(0, result)
            child_result = json.loads(
                (workspace / "qt6" / "evidence" / "child.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(str((workspace / "qt6").resolve()), child_result["cwd"])
            self.assertEqual(
                str((workspace / "qt6" / "home").resolve()), child_result["home"]
            )
            self.assertEqual("active", child_result["parent_runtime"])
            self.assertEqual(
                str((workspace / "qt6" / "conditions.json").resolve()),
                child_result["conditions_file"],
            )

    def test_run_preserves_child_exit_code(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )

            result = real_dictionary_acceptance_workspace.run_in_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
                "qt5",
                [
                    sys.executable,
                    "-c",
                    "raise SystemExit(7)",
                    "--dictionary-root",
                    str(corpus),
                ],
            )

            self.assertEqual(7, result)

    def test_run_rejects_mismatched_dictionary_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            other_corpus = root / "other"
            other_corpus.mkdir()
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "dictionary root does not match",
            ):
                real_dictionary_acceptance_workspace.run_in_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    [
                        sys.executable,
                        "-c",
                        "pass",
                        "--dictionary-root",
                        str(other_corpus),
                    ],
                )

    def test_run_rejects_split_and_combined_dictionary_roots(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            other_corpus = root / "other"
            other_corpus.mkdir()
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "exactly one --dictionary-root",
            ):
                real_dictionary_acceptance_workspace.run_in_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    [
                        sys.executable,
                        "-c",
                        "pass",
                        "--dictionary-root",
                        str(corpus),
                        f"--dictionary-root={other_corpus}",
                    ],
                )

    def test_run_rejects_child_that_changes_corpus_and_manifest_together(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )
            child_program = (
                "import hashlib, json, os, pathlib; "
                "payload = b'changed payload'; "
                "source = pathlib.Path(os.environ['GD_TEST_CORPUS_FILE']); "
                "source.write_bytes(payload); "
                "manifest_path = pathlib.Path(os.environ['GD_TEST_MANIFEST_FILE']); "
                "manifest = json.loads(manifest_path.read_text(encoding='utf-8')); "
                "manifest['files'][0]['sha256'] = hashlib.sha256(payload).hexdigest(); "
                "manifest['files'][0]['size'] = len(payload); "
                "manifest['total_bytes'] = len(payload); "
                "manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, "
                "indent=2, sort_keys=True) + '\\n', encoding='utf-8')"
            )

            with mock.patch.dict(
                os.environ,
                {
                    "GD_TEST_CORPUS_FILE": str(corpus / "dictionary.dsl"),
                    "GD_TEST_MANIFEST_FILE": str(manifest),
                },
                clear=False,
            ):
                with self.assertRaises(
                    real_dictionary_acceptance_workspace.WorkspaceError
                ):
                    real_dictionary_acceptance_workspace.run_in_workspace(
                        workspace,
                        corpus,
                        manifest,
                        conditions,
                        QT5_REVISION,
                        QT6_REVISION,
                        "qt6",
                        [
                            sys.executable,
                            "-c",
                            child_program,
                            "--dictionary-root",
                            str(corpus),
                        ],
                    )

    def test_successful_run_requires_condition_acknowledgement(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            corpus, manifest, conditions, _ = self._fixture(root)
            workspace = root / "workspace"
            real_dictionary_acceptance_workspace.create_workspace(
                workspace,
                corpus,
                manifest,
                conditions,
                QT5_REVISION,
                QT6_REVISION,
            )

            with self.assertRaisesRegex(
                real_dictionary_acceptance_workspace.WorkspaceError,
                "did not acknowledge",
            ):
                real_dictionary_acceptance_workspace.run_in_workspace(
                    workspace,
                    corpus,
                    manifest,
                    conditions,
                    QT5_REVISION,
                    QT6_REVISION,
                    "qt6",
                    [
                        sys.executable,
                        "-c",
                        "pass",
                        "--dictionary-root",
                        str(corpus),
                    ],
                )


if __name__ == "__main__":
    unittest.main()
