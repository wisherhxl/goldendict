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
    "cancellation",
    "cancellation-recovery",
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


def _write_config(
    path: Path,
    dictionary_root: Path,
    language: str,
    morphology_path: Path | None = None,
    morphology_ids: list[str] | None = None,
) -> None:
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
    if morphology_path is not None:
        hunspell = ET.SubElement(
            root, "hunspell", {"dictionariesPath": str(morphology_path)}
        )
        for dictionary_id in morphology_ids or []:
            ET.SubElement(hunspell, "enabled").text = dictionary_id
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


@contextmanager
def _observer_profile(config_root: Path, persistent: bool):
    if persistent:
        profile = config_root / "management-profile"
        profile.mkdir(exist_ok=True)
        yield profile
        return
    with tempfile.TemporaryDirectory(
        dir=config_root, prefix=".qt5-observer-profile-"
    ) as temporary_profile:
        yield Path(temporary_profile)


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
    *,
    mdict_catalog: Path | None = None,
    mdict_catalog_sha256: str | None = None,
    dsl_catalog: Path | None = None,
    dsl_catalog_sha256: str | None = None,
    lookup_catalog: Path | None = None,
    lookup_catalog_sha256: str | None = None,
    management_catalog: Path | None = None,
    management_catalog_sha256: str | None = None,
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
    resolved_mdict_catalog = (
        _resolve(mdict_catalog, "MDict acceptance catalog")
        if mdict_catalog is not None
        else None
    )
    if (resolved_mdict_catalog is None) != (mdict_catalog_sha256 is None):
        raise Qt5ObserverError("MDict catalog path and hash must be supplied together")
    if mdict_catalog_sha256 is not None and (
        len(mdict_catalog_sha256) != HASH_LENGTH
        or any(
            character not in "0123456789abcdef" for character in mdict_catalog_sha256
        )
        or _sha256(resolved_mdict_catalog) != mdict_catalog_sha256
    ):
        raise Qt5ObserverError("MDict catalog hash is invalid")
    resolved_dsl_catalog = (
        _resolve(dsl_catalog, "DSL acceptance catalog")
        if dsl_catalog is not None
        else None
    )
    if (resolved_dsl_catalog is None) != (dsl_catalog_sha256 is None):
        raise Qt5ObserverError("DSL catalog path and hash must be supplied together")
    if dsl_catalog_sha256 is not None and (
        len(dsl_catalog_sha256) != HASH_LENGTH
        or any(character not in "0123456789abcdef" for character in dsl_catalog_sha256)
        or _sha256(resolved_dsl_catalog) != dsl_catalog_sha256
    ):
        raise Qt5ObserverError("DSL catalog hash is invalid")
    resolved_lookup_catalog = (
        _resolve(lookup_catalog, "Lookup acceptance catalog")
        if lookup_catalog is not None
        else None
    )
    if (resolved_lookup_catalog is None) != (lookup_catalog_sha256 is None):
        raise Qt5ObserverError("Lookup catalog path and hash must be supplied together")
    if lookup_catalog_sha256 is not None and (
        len(lookup_catalog_sha256) != HASH_LENGTH
        or any(
            character not in "0123456789abcdef" for character in lookup_catalog_sha256
        )
        or _sha256(resolved_lookup_catalog) != lookup_catalog_sha256
    ):
        raise Qt5ObserverError("Lookup catalog hash is invalid")
    resolved_management_catalog = (
        _resolve(management_catalog, "Management acceptance catalog")
        if management_catalog is not None
        else None
    )
    if (resolved_management_catalog is None) != (management_catalog_sha256 is None):
        raise Qt5ObserverError(
            "Management catalog path and hash must be supplied together"
        )
    if management_catalog_sha256 is not None and (
        len(management_catalog_sha256) != HASH_LENGTH
        or any(
            character not in "0123456789abcdef"
            for character in management_catalog_sha256
        )
        or _sha256(resolved_management_catalog) != management_catalog_sha256
    ):
        raise Qt5ObserverError("Management catalog hash is invalid")
    morphology_path: Path | None = None
    morphology_ids: list[str] = []
    if resolved_lookup_catalog is not None:
        try:
            lookup_value = json.loads(resolved_lookup_catalog.read_bytes())
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
            raise Qt5ObserverError("Lookup catalog is invalid") from error
        dictionaries = lookup_value.get("dictionaries")
        if not isinstance(dictionaries, list):
            raise Qt5ObserverError("Lookup catalog dictionaries are invalid")
        for item in dictionaries:
            if not isinstance(item, dict) or item.get("format") != "hunspell":
                continue
            primary_component = item.get("primary_component")
            if not isinstance(primary_component, str):
                raise Qt5ObserverError("Hunspell lookup catalog item is invalid")
            affix_file = (dictionary_root / primary_component).resolve(strict=True)
            try:
                affix_file.relative_to(dictionary_root)
            except ValueError as error:
                raise Qt5ObserverError(
                    "Hunspell lookup catalog item escapes the corpus"
                ) from error
            if affix_file.suffix.casefold() != ".aff":
                raise Qt5ObserverError("Hunspell lookup component is invalid")
            dictionary_id = affix_file.stem
            if morphology_path is None:
                morphology_path = affix_file.parent
            elif morphology_path != affix_file.parent:
                raise Qt5ObserverError(
                    "Hunspell lookup catalog must use one morphology directory"
                )
            if dictionary_id in morphology_ids:
                raise Qt5ObserverError("Hunspell lookup catalog id is duplicated")
            morphology_ids.append(dictionary_id)
    if (
        sum(
            catalog is not None
            for catalog in (
                resolved_mdict_catalog,
                resolved_dsl_catalog,
                resolved_lookup_catalog,
                resolved_management_catalog,
            )
        )
        > 1
    ):
        raise Qt5ObserverError("Only one format-specific catalog may be supplied")
    if timeout_seconds < 1 or timeout_seconds > 24 * 60 * 60:
        raise Qt5ObserverError("Observer timeout is outside the supported bound")

    with _observer_profile(
        config_root, resolved_management_catalog is not None
    ) as profile:
        appdata = profile / "appdata"
        home = profile / "home"
        local_appdata = profile / "local-appdata"
        for directory in (appdata, home, local_appdata):
            directory.mkdir(exist_ok=True)
        configuration_path = _config_path(appdata, home)
        if resolved_management_catalog is None or scenario == "clean-discovery":
            if resolved_management_catalog is not None and configuration_path.exists():
                raise Qt5ObserverError("Clean management profile already exists")
            _write_config(
                configuration_path,
                dictionary_root,
                interface_language,
                morphology_path,
                morphology_ids,
            )
        elif not configuration_path.is_file():
            raise Qt5ObserverError("Warm management profile is missing")
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
        if resolved_mdict_catalog is not None:
            environment["GOLDENDICT_ACCEPTANCE_MDICT_CATALOG"] = str(
                resolved_mdict_catalog
            )
            environment["GOLDENDICT_ACCEPTANCE_MDICT_CATALOG_SHA256"] = str(
                mdict_catalog_sha256
            )
        if resolved_dsl_catalog is not None:
            environment["GOLDENDICT_ACCEPTANCE_DSL_CATALOG"] = str(resolved_dsl_catalog)
            environment["GOLDENDICT_ACCEPTANCE_DSL_CATALOG_SHA256"] = str(
                dsl_catalog_sha256
            )
        if resolved_lookup_catalog is not None:
            environment["GOLDENDICT_ACCEPTANCE_LOOKUP_CATALOG"] = str(
                resolved_lookup_catalog
            )
            environment["GOLDENDICT_ACCEPTANCE_LOOKUP_CATALOG_SHA256"] = str(
                lookup_catalog_sha256
            )
        if resolved_management_catalog is not None:
            environment["GOLDENDICT_ACCEPTANCE_MANAGEMENT_CATALOG"] = str(
                resolved_management_catalog
            )
            environment["GOLDENDICT_ACCEPTANCE_MANAGEMENT_CATALOG_SHA256"] = str(
                management_catalog_sha256
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
            diagnostic = output.with_name(output.name + ".failure")
            reason = ""
            try:
                reason = diagnostic.read_text(encoding="utf-8").strip()
            except OSError:
                pass
            diagnostic.unlink(missing_ok=True)
            output.unlink(missing_ok=True)
            detail = f": {reason}" if reason else ""
            raise Qt5ObserverError(
                f"Qt 5 observer failed with exit code {completed.returncode}{detail}"
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
    parser.add_argument("--mdict-catalog", type=Path)
    parser.add_argument("--mdict-catalog-sha256")
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
            mdict_catalog=options.mdict_catalog,
            mdict_catalog_sha256=options.mdict_catalog_sha256,
        )
    except Qt5ObserverError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
