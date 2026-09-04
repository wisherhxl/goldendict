#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Run the disposable Qt 5 acceptance binary in an isolated profile."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import platform
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from collections.abc import Iterable
from contextlib import contextmanager
from pathlib import Path

from prepare_qt5_acceptance_source import (
    FROZEN_REVISION,
    FROZEN_TREE,
    INSTRUMENTED_FILES,
    OBSERVER_INCLUDE,
    PROVENANCE_SCHEMA,
)

SUPPORTED_SCENARIOS = (
    "clean-discovery",
    "warm-restart",
    "explicit-rescan",
    "changed-source",
    "unavailable-companion",
    "companion-recovery",
)
HASH_LENGTH = 64
MAX_PROVENANCE_BYTES = 16 * 1024


class Qt5ObserverError(RuntimeError):
    """Raised when the disposable Qt 5 observer cannot run safely."""


def _required_environment(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise Qt5ObserverError(f"Required environment variable is missing: {name}")
    return value


def _resolve(path: Path, label: str, *, directory: bool = False) -> Path:
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise Qt5ObserverError(f"{label} does not exist") from error
    if directory != resolved.is_dir():
        expected = "directory" if directory else "file"
        raise Qt5ObserverError(f"{label} must be a {expected}")
    return resolved


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _verify_provenance(path: Path) -> None:
    path = _resolve(path, "Qt 5 source provenance")
    try:
        content = path.read_bytes()
        if len(content) > MAX_PROVENANCE_BYTES:
            raise Qt5ObserverError("Qt 5 source provenance is oversized")
        value = json.loads(content)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise Qt5ObserverError("Qt 5 source provenance is invalid") from error
    observer_include = Path(__file__).with_name(OBSERVER_INCLUDE)
    expected = {
        "instrumented_files": {
            name: _sha256(_resolve(path.parent / name, f"Instrumented {name}"))
            for name in INSTRUMENTED_FILES
        },
        "observer_include_sha256": _sha256(observer_include),
        "revision": FROZEN_REVISION,
        "schema": PROVENANCE_SCHEMA,
        "source_tree": FROZEN_TREE,
    }
    if value != expected:
        raise Qt5ObserverError("Qt 5 source provenance does not match this observer")


def _write_config(path: Path, dictionary_root: Path, language: str) -> None:
    root = ET.Element("config")
    paths = ET.SubElement(root, "paths")
    dictionary_path = ET.SubElement(paths, "path", {"recursive": "1"})
    dictionary_path.text = str(dictionary_root)
    ET.SubElement(root, "dictionaryOrder")
    ET.SubElement(root, "inactiveDictionaries")
    preferences = ET.SubElement(root, "preferences")
    ET.SubElement(preferences, "interfaceLanguage").text = language
    for name in (
        "enableScanPopup",
        "enableClipboardHotkey",
        "enableTrayIcon",
        "startToTray",
        "checkForNewReleases",
    ):
        ET.SubElement(preferences, name).text = "0"
    path.parent.mkdir(parents=True)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def _config_path(appdata: Path, home: Path) -> Path:
    if platform.system() == "Windows":
        return appdata / "GoldenDict" / "config"
    return home / ".goldendict" / "config"


@contextmanager
def _suppressed_windows_error_dialogs():
    if platform.system() != "Windows":
        yield
        return
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    flags = 0x0001 | 0x0002 | 0x8000
    kernel32.GetErrorMode.restype = ctypes.c_uint
    kernel32.SetErrorMode.argtypes = (ctypes.c_uint,)
    kernel32.SetErrorMode.restype = ctypes.c_uint
    previous = kernel32.GetErrorMode()
    kernel32.SetErrorMode(previous | flags)
    try:
        yield
    finally:
        kernel32.SetErrorMode(previous)


def _bound_directory(environment_name: str, supplied: Path, label: str) -> Path:
    expected = _resolve(
        Path(_required_environment(environment_name)), label, directory=True
    )
    if supplied != expected:
        raise Qt5ObserverError(f"{label} does not match the paired workspace")
    return supplied


def observe(
    legacy_executable: Path,
    provenance: Path,
    runtime_bins: list[Path],
    plugin_path: Path | None,
    dictionary_root: Path,
    index_root: Path,
    interface_language: str,
    conditions_sha256: str,
    scenario: str,
    output: Path,
    timeout_seconds: int,
) -> Path:
    if _required_environment("GOLDENDICT_ACCEPTANCE_VERSION") != "qt5":
        raise Qt5ObserverError("Qt 5 observer requires a Qt 5 paired run")
    if _required_environment("GOLDENDICT_ACCEPTANCE_REVISION") != FROZEN_REVISION:
        raise Qt5ObserverError("Paired run does not use the frozen Qt 5 revision")
    if scenario not in SUPPORTED_SCENARIOS:
        raise Qt5ObserverError("Qt 5 observer received an unsupported scenario")
    if (
        len(conditions_sha256) != HASH_LENGTH
        or any(character not in "0123456789abcdef" for character in conditions_sha256)
        or conditions_sha256
        != _required_environment("GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256")
    ):
        raise Qt5ObserverError("Conditions hash does not match the paired run")
    legacy_executable = _resolve(legacy_executable, "Qt 5 observer executable")
    _verify_provenance(provenance)
    dictionary_root = _bound_directory(
        "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT",
        _resolve(dictionary_root, "Dictionary root", directory=True),
        "Dictionary root",
    )
    index_root = _bound_directory(
        "GOLDENDICT_ACCEPTANCE_INDEX_ROOT",
        _resolve(index_root, "Index root", directory=True),
        "Index root",
    )
    config_root = _resolve(
        Path(_required_environment("APPDATA")),
        "Configuration root",
        directory=True,
    )
    evidence_root = _resolve(
        Path(_required_environment("GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT")),
        "Evidence root",
        directory=True,
    )
    output = output.resolve(strict=False)
    output_is_adapter_temporary = (
        output.parent.parent == evidence_root
        and output.parent.name.startswith(".raw-observation-")
    )
    if output.parent != evidence_root and not output_is_adapter_temporary:
        raise Qt5ObserverError("Raw output must remain inside the paired evidence root")
    if output.exists():
        raise Qt5ObserverError("Raw output already exists")
    resolved_runtime_bins = [
        _resolve(path, "Qt 5 runtime directory", directory=True)
        for path in runtime_bins
    ]
    resolved_plugin_path = (
        _resolve(plugin_path, "Qt 5 plugin directory", directory=True)
        if plugin_path is not None
        else None
    )
    if timeout_seconds < 1 or timeout_seconds > 24 * 60 * 60:
        raise Qt5ObserverError("Observer timeout is outside the supported bound")

    with tempfile.TemporaryDirectory(
        dir=config_root, prefix=".qt5-observer-profile-"
    ) as temporary_profile:
        profile = Path(temporary_profile)
        appdata = profile / "appdata"
        home = profile / "home"
        local_appdata = profile / "local-appdata"
        for directory in (appdata, home, local_appdata):
            directory.mkdir()
        _write_config(_config_path(appdata, home), dictionary_root, interface_language)
        environment = os.environ.copy()
        environment.update(
            {
                "APPDATA": str(appdata),
                "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256": conditions_sha256,
                "GOLDENDICT_ACCEPTANCE_INDEX_ROOT": str(index_root),
                "GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH": str(output),
                "GOLDENDICT_ACCEPTANCE_SCENARIO": scenario,
                "HOME": str(home),
                "LOCALAPPDATA": str(local_appdata),
                "QT_OPENGL": "software",
                "QT_QPA_PLATFORM": (
                    "windows" if platform.system() == "Windows" else "offscreen"
                ),
                "USERPROFILE": str(home),
                "XDG_CACHE_HOME": str(profile / "xdg-cache"),
                "XDG_CONFIG_HOME": str(profile / "xdg-config"),
                "XDG_DATA_HOME": str(profile / "xdg-data"),
            }
        )
        path_entries = [legacy_executable.parent, *resolved_runtime_bins]
        environment["PATH"] = os.pathsep.join(
            [*(str(path) for path in path_entries), os.environ.get("PATH", "")]
        )
        if resolved_plugin_path is not None:
            environment["QT_PLUGIN_PATH"] = str(resolved_plugin_path)
            environment["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(
                resolved_plugin_path / "platforms"
            )
        if (legacy_executable.parent / "portable").exists():
            raise Qt5ObserverError(
                "Qt 5 observer executable directory must not contain portable state"
            )
        try:
            with _suppressed_windows_error_dialogs():
                completed = subprocess.run(
                    [str(legacy_executable)],
                    cwd=legacy_executable.parent,
                    env=environment,
                    check=False,
                    timeout=timeout_seconds,
                )
        except (OSError, subprocess.TimeoutExpired) as error:
            output.unlink(missing_ok=True)
            raise Qt5ObserverError(
                "Qt 5 observer process could not complete"
            ) from error
        if completed.returncode != 0:
            output.unlink(missing_ok=True)
            raise Qt5ObserverError(
                f"Qt 5 observer failed with exit code {completed.returncode}"
            )
        if not output.is_file():
            raise Qt5ObserverError("Qt 5 observer did not publish a raw result")
    return output


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--legacy-executable", type=Path, required=True)
    parser.add_argument("--provenance", type=Path, required=True)
    parser.add_argument("--runtime-bin", type=Path, action="append", default=[])
    parser.add_argument("--plugin-path", type=Path)
    parser.add_argument("--timeout-seconds", type=int, default=3600)
    parser.add_argument("--dictionary-root", type=Path, required=True)
    parser.add_argument("--index-root", type=Path, required=True)
    parser.add_argument("--interface-language", required=True)
    parser.add_argument("--conditions-sha256", required=True)
    parser.add_argument("--scenario", choices=SUPPORTED_SCENARIOS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        output = observe(
            options.legacy_executable,
            options.provenance,
            options.runtime_bin,
            options.plugin_path,
            options.dictionary_root,
            options.index_root,
            options.interface_language,
            options.conditions_sha256,
            options.scenario,
            options.output,
            options.timeout_seconds,
        )
    except Qt5ObserverError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
