#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import prepare_qt5_acceptance_source as preparation


class PrepareQt5AcceptanceSourceTest(unittest.TestCase):
    @staticmethod
    def _archive(archive: Path) -> None:
        files = {
            "mainwindow.cc": (
                '#include "mainwindow.hh"\n'
                "void MainWindow::makeDictionaries()\n"
                "{\n"
                "  updateStatusLine();\n"
                "  updateGroupList();\n"
                "}\n"
            ),
            "main.cc": (
                "#ifdef Q_OS_WIN32\n"
                "#include <QtCore/qt_windows.h>\n"
                "#endif\n"
                '  QHotkeyApplication app( "GoldenDict", argc, argv );\n'
                "  LogFilePtrGuard logFilePtrGuard;\n"
                "  if( !showHelpAndExit && app.isRunning() )\n"
            ),
            "config.cc": (
                "QString getIndexDir() THROW_SPEC( exError )\n"
                "{\n"
                "  QDir result = getHomeDir();\n"
                "}\n"
            ),
            "mouseover.cc": (
                "#ifndef _MSC_VER\n"
                "typedef struct tagCHANGEFILTERSTRUCT {\n"
                "  int value;\n"
                "};\n"
                "#endif\n"
            ),
        }
        with zipfile.ZipFile(archive, "w") as output:
            for name, content in files.items():
                output.writestr(name, content)

    def test_prepares_instrumented_tree_with_bound_provenance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            checkout = root / "checkout"
            checkout.mkdir()
            observer = root / preparation.OBSERVER_INCLUDE
            observer.write_text("observer\n", encoding="utf-8")
            output = root / "prepared"
            with (
                mock.patch.object(preparation, "_verify_frozen_checkout"),
                mock.patch.object(
                    preparation,
                    "_export_frozen_tree",
                    side_effect=lambda _checkout, archive: self._archive(archive),
                ),
            ):
                provenance = preparation.prepare(checkout, output, observer)

            self.assertEqual(provenance, output / preparation.PROVENANCE_FILE)
            main_window = (output / "mainwindow.cc").read_text(encoding="utf-8")
            self.assertIn('#include "qt5_acceptance_observer.inc"', main_window)
            self.assertIn("Qt5AcceptanceObserver::Publish", main_window)
            self.assertIn(
                "GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH",
                (output / "main.cc").read_text(encoding="utf-8"),
            )
            self.assertIn(
                "AcceptanceAccessibilityFilter",
                (output / "main.cc").read_text(encoding="utf-8"),
            )
            self.assertIn(
                "GOLDENDICT_ACCEPTANCE_INDEX_ROOT",
                (output / "config.cc").read_text(encoding="utf-8"),
            )
            self.assertIn(
                "!defined( MSGFLT_RESET )",
                (output / "mouseover.cc").read_text(encoding="utf-8"),
            )
            value = json.loads(provenance.read_text(encoding="utf-8"))
            self.assertEqual(value["revision"], preparation.FROZEN_REVISION)
            self.assertEqual(value["source_tree"], preparation.FROZEN_TREE)
            self.assertEqual(
                set(value["instrumented_files"]),
                set(preparation.INSTRUMENTED_FILES),
            )
            for name, digest in value["instrumented_files"].items():
                self.assertEqual(digest, preparation._sha256(output / name))

    def test_rejects_existing_output_before_export(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            checkout = root / "checkout"
            observer = root / preparation.OBSERVER_INCLUDE
            output = root / "prepared"
            checkout.mkdir()
            output.mkdir()
            observer.write_text("observer\n", encoding="utf-8")
            with (
                mock.patch.object(preparation, "_verify_frozen_checkout"),
                self.assertRaisesRegex(preparation.PreparationError, "already exists"),
            ):
                preparation.prepare(checkout, output, observer)

    def test_rejects_changed_instrumentation_anchor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            checkout = root / "checkout"
            checkout.mkdir()
            observer = root / preparation.OBSERVER_INCLUDE
            observer.write_text("observer\n", encoding="utf-8")

            def changed_archive(_checkout: Path, archive: Path) -> None:
                with zipfile.ZipFile(archive, "w") as output:
                    output.writestr("mainwindow.cc", '#include "mainwindow.hh"\n')
                    output.writestr("main.cc", "changed\n")
                    output.writestr(
                        "config.cc",
                        "QString getIndexDir() THROW_SPEC( exError )\n"
                        "{\n"
                        "  QDir result = getHomeDir();\n"
                        "}\n",
                    )
                    output.writestr(
                        "mouseover.cc",
                        "#ifndef _MSC_VER\n"
                        "typedef struct tagCHANGEFILTERSTRUCT {\n"
                        "};\n",
                    )

            with (
                mock.patch.object(preparation, "_verify_frozen_checkout"),
                mock.patch.object(
                    preparation, "_export_frozen_tree", side_effect=changed_archive
                ),
                self.assertRaises(preparation.PreparationError),
            ):
                preparation.prepare(checkout, root / "prepared", observer)

    def test_rejects_archive_path_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "source.zip"
            destination = root / "destination"
            destination.mkdir()
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../outside", "unsafe")
            with self.assertRaisesRegex(preparation.PreparationError, "unsafe path"):
                preparation._extract_archive(archive, destination)
            self.assertFalse((root / "outside").exists())

    def test_rejects_checkout_at_other_revision(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            checkout = Path(temporary)
            with (
                mock.patch.object(
                    preparation,
                    "_git",
                    side_effect=("b" * 40, preparation.FROZEN_TREE, ""),
                ),
                self.assertRaisesRegex(preparation.PreparationError, "frozen revision"),
            ):
                preparation._verify_frozen_checkout(checkout)

    def test_rejects_dirty_frozen_checkout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            checkout = Path(temporary)
            with (
                mock.patch.object(
                    preparation,
                    "_git",
                    side_effect=(
                        preparation.FROZEN_REVISION,
                        preparation.FROZEN_TREE,
                        "?? unexpected.txt",
                    ),
                ),
                self.assertRaisesRegex(
                    preparation.PreparationError, "completely clean"
                ),
            ):
                preparation._verify_frozen_checkout(checkout)


if __name__ == "__main__":
    unittest.main()
