#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Prepare a disposable, instrumented copy of the frozen Qt 5 source tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import zipfile
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

FROZEN_REVISION = "3d93dd66197aea10edf6c29998ddc9c213d0aaa8"
FROZEN_TREE = "5528de587cdee4ebd042a166f233692487bb234f"
PROVENANCE_SCHEMA = "goldendict-qt5-acceptance-source-v1"
OBSERVER_INCLUDE = "qt5_acceptance_observer.inc"
PROVENANCE_FILE = ".goldendict-qt5-acceptance-source.json"
INSTRUMENTED_FILES = (
    "config.cc",
    "main.cc",
    "mainwindow.cc",
    "mouseover.cc",
    OBSERVER_INCLUDE,
)


class PreparationError(RuntimeError):
    """Raised when the frozen source cannot be prepared safely."""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _git(checkout: Path, *arguments: str) -> str:
    try:
        completed = subprocess.run(
            ["git", "-C", str(checkout), *arguments],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise PreparationError("Cannot inspect the frozen Qt 5 checkout") from error
    return completed.stdout.strip()


def _verify_frozen_checkout(checkout: Path) -> None:
    if _git(checkout, "rev-parse", "HEAD") != FROZEN_REVISION:
        raise PreparationError("Qt 5 checkout is not at the frozen revision")
    if _git(checkout, "rev-parse", "HEAD^{tree}") != FROZEN_TREE:
        raise PreparationError("Qt 5 checkout tree does not match the frozen revision")
    if _git(checkout, "status", "--porcelain=v1", "--untracked-files=all"):
        raise PreparationError("Qt 5 checkout must be completely clean")


def _export_frozen_tree(checkout: Path, archive: Path) -> None:
    try:
        subprocess.run(
            [
                "git",
                "-C",
                str(checkout),
                "archive",
                "--format=zip",
                f"--output={archive}",
                FROZEN_REVISION,
            ],
            check=True,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise PreparationError("Cannot export the frozen Qt 5 tree") from error


def _extract_archive(archive: Path, destination: Path) -> None:
    with zipfile.ZipFile(archive) as source:
        for member in source.infolist():
            path = PurePosixPath(member.filename)
            if path.is_absolute() or ".." in path.parts or not path.parts:
                raise PreparationError("Qt 5 source archive contains an unsafe path")
        source.extractall(destination)


def _replace_once(path: Path, before: str, after: str) -> None:
    content = path.read_text(encoding="utf-8")
    if content.count(before) != 1:
        raise PreparationError(f"Instrumentation anchor changed: {path.name}")
    path.write_text(content.replace(before, after), encoding="utf-8", newline="")


def _instrument(source: Path, observer_include: Path) -> None:
    shutil.copy2(observer_include, source / OBSERVER_INCLUDE)
    _replace_once(
        source / "mainwindow.cc",
        '#include "mainwindow.hh"\n',
        '#include "mainwindow.hh"\n#include "qt5_acceptance_observer.inc"\n',
    )
    _replace_once(
        source / "mainwindow.cc",
        "  updateStatusLine();\n  updateGroupList();\n}\n",
        "  updateStatusLine();\n"
        "  updateGroupList();\n"
        "  Qt5AcceptanceObserver::Publish( groupInstances.front().dictionaries );\n"
        "}\n",
    )
    _replace_once(
        source / "main.cc",
        "  if( !showHelpAndExit && app.isRunning() )\n",
        "  if( !showHelpAndExit\n"
        '      && qgetenv( "GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH" ).isEmpty()\n'
        "      && app.isRunning() )\n",
    )
    _replace_once(
        source / "main.cc",
        "#ifdef Q_OS_WIN32\n#include <QtCore/qt_windows.h>\n#endif\n",
        "#ifdef Q_OS_WIN32\n"
        "#include <QtCore/qt_windows.h>\n"
        "#include <QAbstractNativeEventFilter>\n\n"
        "class AcceptanceAccessibilityFilter: public QAbstractNativeEventFilter\n"
        "{\n"
        "public:\n"
        "  bool nativeEventFilter( const QByteArray &, void * message, long * result ) override\n"
        "  {\n"
        "    MSG * msg = static_cast< MSG * >( message );\n"
        "    if( msg && msg->message == WM_GETOBJECT )\n"
        "    {\n"
        "      if( result )\n"
        "        *result = 0;\n"
        "      return true;\n"
        "    }\n"
        "    return false;\n"
        "  }\n"
        "};\n"
        "#endif\n",
    )
    _replace_once(
        source / "main.cc",
        '  QHotkeyApplication app( "GoldenDict", argc, argv );\n'
        "  LogFilePtrGuard logFilePtrGuard;\n",
        '  QHotkeyApplication app( "GoldenDict", argc, argv );\n'
        "#ifdef Q_OS_WIN32\n"
        "  AcceptanceAccessibilityFilter acceptanceAccessibilityFilter;\n"
        "  app.installNativeEventFilter( &acceptanceAccessibilityFilter );\n"
        "#endif\n"
        "  LogFilePtrGuard logFilePtrGuard;\n",
    )
    _replace_once(
        source / "config.cc",
        "QString getIndexDir() THROW_SPEC( exError )\n{\n  QDir result = getHomeDir();\n",
        "QString getIndexDir() THROW_SPEC( exError )\n"
        "{\n"
        '  QByteArray const rawResultPath = qgetenv( "GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH" );\n'
        "  if( !rawResultPath.isEmpty() )\n"
        "  {\n"
        '    QByteArray const acceptanceIndexPath = qgetenv( "GOLDENDICT_ACCEPTANCE_INDEX_ROOT" );\n'
        "    QDir acceptanceIndex( QString::fromUtf8( acceptanceIndexPath ) );\n"
        "    if( acceptanceIndexPath.isEmpty() || !acceptanceIndex.exists() )\n"
        "      throw exCantUseIndexDir();\n"
        "    return acceptanceIndex.absolutePath() + QDir::separator();\n"
        "  }\n\n"
        "  QDir result = getHomeDir();\n",
    )
    _replace_once(
        source / "mouseover.cc",
        "#ifndef _MSC_VER\n" "typedef struct tagCHANGEFILTERSTRUCT {\n",
        "#if !defined( _MSC_VER ) && !defined( MSGFLT_RESET )\n"
        "typedef struct tagCHANGEFILTERSTRUCT {\n",
    )


def prepare(checkout: Path, output: Path, observer_include: Path) -> Path:
    try:
        checkout = checkout.resolve(strict=True)
        observer_include = observer_include.resolve(strict=True)
        output = output.resolve(strict=False)
    except OSError as error:
        raise PreparationError("A preparation input does not exist") from error
    if output.exists():
        raise PreparationError("Output directory already exists")
    _verify_frozen_checkout(checkout)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary_root = Path(
        tempfile.mkdtemp(prefix=".qt5-acceptance-source-", dir=output.parent)
    )
    archive = temporary_root / "source.zip"
    staged_source = temporary_root / "source"
    try:
        _export_frozen_tree(checkout, archive)
        staged_source.mkdir()
        _extract_archive(archive, staged_source)
        _instrument(staged_source, observer_include)
        provenance = {
            "instrumented_files": {
                name: _sha256(staged_source / name) for name in INSTRUMENTED_FILES
            },
            "observer_include_sha256": _sha256(observer_include),
            "revision": FROZEN_REVISION,
            "schema": PROVENANCE_SCHEMA,
            "source_tree": FROZEN_TREE,
        }
        (staged_source / PROVENANCE_FILE).write_text(
            json.dumps(provenance, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        os.replace(staged_source, output)
    except (OSError, zipfile.BadZipFile) as error:
        raise PreparationError("Cannot prepare the disposable Qt 5 source") from error
    finally:
        shutil.rmtree(temporary_root, ignore_errors=True)
    return output / PROVENANCE_FILE


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--observer-include",
        type=Path,
        default=Path(__file__).with_name(OBSERVER_INCLUDE),
    )
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        provenance = prepare(options.checkout, options.output, options.observer_include)
    except PreparationError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    print(provenance)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
