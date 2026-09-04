#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Adapt a version-specific dictionary probe to the paired result contract."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import stat
import subprocess
import tempfile
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

import real_dictionary_acceptance_result as acceptance_result
import real_dictionary_acceptance_workspace as acceptance_workspace

RAW_SCHEMA = "goldendict-real-dictionary-raw-observation-v1"
SUPPORTED_SCENARIOS = (
    "clean-discovery",
    "warm-restart",
    "explicit-rescan",
    "changed-source",
    "unavailable-companion",
    "companion-recovery",
)
SCENARIO_PHASES = {
    "clean-discovery": "discovery",
    "warm-restart": "restart",
    "explicit-rescan": "rescan",
    "changed-source": "source-change",
    "unavailable-companion": "companion-unavailable",
    "companion-recovery": "companion-recovery",
}
MAX_RAW_BYTES = acceptance_result.MAX_RESULT_BYTES
MAX_RAW_DICTIONARIES = acceptance_result.MAX_DICTIONARIES
MAX_RAW_ERRORS = acceptance_result.MAX_DIAGNOSTICS
FORMATS = (
    "aard",
    "bgl",
    "dictd",
    "dsl",
    "epwing",
    "gls",
    "lsa",
    "mdict",
    "sdict",
    "slob",
    "stardict",
    "xdxf",
    "zim",
    "zipsounds",
)
ERROR_CODES = (
    "cancelled",
    "deadline-exceeded",
    "dictionary-unavailable",
    "internal",
    "invalid-query",
    "unsupported",
)
RESERVED_OBSERVER_OPTIONS = (
    "--conditions-sha256",
    "--dictionary-root",
    "--index-root",
    "--interface-language",
    "--output",
    "--scenario",
)


class ObserverError(RuntimeError):
    """Raised when a version-specific probe cannot produce safe evidence."""


def _required_environment(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise ObserverError(f"Required environment variable is missing: {name}")
    return value


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            while chunk := source.read(4 * 1024 * 1024):
                digest.update(chunk)
    except OSError as error:
        raise ObserverError(f"Cannot hash file: {path}") from error
    return digest.hexdigest()


def _read_json(path: Path, maximum_bytes: int, label: str) -> object:
    try:
        with path.open("rb") as source:
            content = source.read(maximum_bytes + 1)
    except OSError as error:
        raise ObserverError(f"Cannot read {label}: {path}") from error
    if len(content) > maximum_bytes:
        raise ObserverError(f"{label} exceeds the file-size bound")
    try:
        return json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ObserverError(f"{label} is not valid UTF-8 JSON: {path}") from error


def _exact_keys(value: dict[str, object], expected: set[str], label: str) -> None:
    if set(value) != expected:
        raise ObserverError(f"{label} has an unexpected structure")


def _bounded_text(value: object, label: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not allow_empty and not value):
        raise ObserverError(f"{label} must be a string")
    try:
        encoded = value.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ObserverError(f"{label} must be valid UTF-8") from error
    if len(encoded) > acceptance_result.MAX_TEXT_BYTES or "\x00" in value:
        raise ObserverError(f"{label} exceeds the text bound")
    return value


def _count(value: object, label: str) -> int:
    if (
        not isinstance(value, int)
        or isinstance(value, bool)
        or value < 0
        or value > (1 << 63) - 1
    ):
        raise ObserverError(f"{label} must be a bounded non-negative integer")
    return value


def _validate_raw(value: object) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ObserverError("Raw observation must be an object")
    _exact_keys(
        value,
        {
            "catalog",
            "conditions_sha256",
            "elapsed_milliseconds",
            "errors",
            "outcome",
            "phases",
            "scenario",
            "schema",
        },
        "Raw observation",
    )
    if value["schema"] != RAW_SCHEMA:
        raise ObserverError(f"Raw observation schema must be {RAW_SCHEMA}")
    scenario = value["scenario"]
    if scenario not in SUPPORTED_SCENARIOS:
        raise ObserverError("Raw observer performed an unsupported scenario")
    if value["outcome"] != "completed":
        raise ObserverError("Dictionary lifecycle observation did not complete")
    if not acceptance_result.HASH_PATTERN.fullmatch(
        _bounded_text(value["conditions_sha256"], "Raw conditions hash")
    ):
        raise ObserverError("Raw conditions hash must be a lowercase SHA-256")
    _count(value["elapsed_milliseconds"], "Raw elapsed time")
    catalog = value["catalog"]
    errors = value["errors"]
    phases = value["phases"]
    if not isinstance(catalog, list) or len(catalog) > MAX_RAW_DICTIONARIES:
        raise ObserverError("Raw catalog must be a bounded array")
    if not isinstance(errors, list) or len(errors) > MAX_RAW_ERRORS:
        raise ObserverError("Raw errors must be a bounded array")
    if not isinstance(phases, list) or len(phases) != 2:
        raise ObserverError("Raw lifecycle observation must contain two phases")
    ids: set[str] = set()
    for index, item in enumerate(catalog):
        label = f"Raw catalog[{index}]"
        if not isinstance(item, dict):
            raise ObserverError(f"{label} must be an object")
        _exact_keys(
            item,
            {
                "article_count",
                "components",
                "edition",
                "enabled",
                "format",
                "headword_count",
                "id",
                "name",
                "order",
                "source_language",
                "target_language",
            },
            label,
        )
        dictionary_id = _bounded_text(item["id"], f"{label}.id")
        if dictionary_id in ids:
            raise ObserverError("Raw dictionary IDs must be unique")
        ids.add(dictionary_id)
        if item["enabled"] is not True:
            raise ObserverError(
                f"{label}.enabled must be true for all-enabled discovery"
            )
        if item["order"] != index:
            raise ObserverError(
                "Raw catalog order must be contiguous and authoritative"
            )
        if item["format"] not in FORMATS:
            raise ObserverError(f"{label}.format is unsupported")
        for field in ("name", "edition", "source_language", "target_language"):
            _bounded_text(item[field], f"{label}.{field}", allow_empty=True)
        _count(item["article_count"], f"{label}.article_count")
        _count(item["headword_count"], f"{label}.headword_count")
        components = item["components"]
        if not isinstance(components, list) or not components:
            raise ObserverError(f"{label}.components must be a non-empty array")
        for component_index, component in enumerate(components):
            _bounded_text(component, f"{label}.components[{component_index}]")
    for index, item in enumerate(errors):
        label = f"Raw errors[{index}]"
        if not isinstance(item, dict):
            raise ObserverError(f"{label} must be an object")
        _exact_keys(item, {"code", "dictionary_id", "message"}, label)
        if item["code"] not in ERROR_CODES:
            raise ObserverError(f"{label}.code is unsupported")
        _bounded_text(item["dictionary_id"], f"{label}.dictionary_id", allow_empty=True)
        _bounded_text(item["message"], f"{label}.message")
    phase_name = SCENARIO_PHASES[str(scenario)]
    expected_phases = ((phase_name, "started"), (phase_name, "completed"))
    for index, item in enumerate(phases):
        label = f"Raw phases[{index}]"
        if not isinstance(item, dict):
            raise ObserverError(f"{label} must be an object")
        _exact_keys(item, {"dictionary_id", "name", "sequence", "status"}, label)
        if item["dictionary_id"] != "":
            raise ObserverError(f"{label}.dictionary_id must be empty")
        if item["sequence"] != index:
            raise ObserverError("Raw phases must use contiguous zero-based sequence")
        if (item["name"], item["status"]) != expected_phases[index]:
            raise ObserverError("Raw lifecycle phases are incomplete")
    return value


def _validate_observer_arguments(arguments: list[str]) -> None:
    for argument in arguments:
        for option in RESERVED_OBSERVER_OPTIONS:
            if argument == option or argument.startswith(f"{option}="):
                raise ObserverError(
                    f"Observer arguments must not override reserved option {option}"
                )


def _normalized_machine(value: str) -> str:
    lowered = value.casefold()
    aliases = {"amd64": "x86_64", "x64": "x86_64", "aarch64": "arm64"}
    return aliases.get(lowered, lowered)


def _validate_conditions(conditions: object) -> dict[str, object]:
    if not isinstance(conditions, dict):
        raise ObserverError("Conditions must be an object")
    required = {"group", "locale", "platform", "preferences", "queries", "schema"}
    _exact_keys(conditions, required, "Conditions")
    if conditions["schema"] != acceptance_workspace.CONDITIONS_SCHEMA:
        raise ObserverError("Conditions schema is unsupported")
    _bounded_text(conditions["locale"], "Conditions locale")
    group = conditions["group"]
    if not isinstance(group, dict):
        raise ObserverError("Conditions group must be an object")
    _exact_keys(group, {"mode", "name", "ordered_dictionary_ids"}, "Conditions group")
    if (
        group["mode"] != "all-enabled"
        or not isinstance(group["name"], str)
        or not group["name"]
        or group["ordered_dictionary_ids"] != []
    ):
        raise ObserverError(
            "Dictionary lifecycle observation requires the all-enabled "
            "empty-order group"
        )
    if conditions["preferences"] != {"profile": "clean-default"}:
        raise ObserverError(
            "Dictionary lifecycle observation requires the clean-default "
            "preference profile"
        )
    if conditions["queries"] != []:
        raise ObserverError("Dictionary lifecycle observation does not accept queries")
    platform_conditions = conditions["platform"]
    if not isinstance(platform_conditions, dict):
        raise ObserverError("Conditions platform must be an object")
    operating_system = platform_conditions.get("operating_system")
    architecture = platform_conditions.get("architecture")
    if not isinstance(operating_system, str) or (
        operating_system.casefold() != platform.system().casefold()
    ):
        raise ObserverError("Conditions operating system does not match the host")
    if not isinstance(architecture, str) or (
        _normalized_machine(architecture) != _normalized_machine(platform.machine())
    ):
        raise ObserverError("Conditions architecture does not match the host")
    return conditions


def _manifest_files(manifest: object) -> dict[str, dict[str, object]]:
    if not isinstance(manifest, dict) or not isinstance(manifest.get("files"), list):
        raise ObserverError("Manifest does not contain a file array")
    records: dict[str, dict[str, object]] = {}
    for index, item in enumerate(manifest["files"]):
        if not isinstance(item, dict) or not isinstance(item.get("path"), str):
            raise ObserverError(f"Manifest file {index} is malformed")
        path = item["path"]
        _validate_relative_path(path, f"Manifest file {index}")
        if path in records:
            raise ObserverError("Manifest file paths must be unique")
        records[path] = item
    return records


def _validate_relative_path(value: str, label: str) -> str:
    if "\\" in value or ":" in value:
        raise ObserverError(f"{label} must be a normalized relative path")
    path = PurePosixPath(value)
    if (
        not value
        or path.is_absolute()
        or value.startswith("//")
        or value == "."
        or path.as_posix() != value
        or any(part == ".." for part in path.parts)
    ):
        raise ObserverError(f"{label} must be normalized and confined")
    return value


def _relative_component(path_text: str, corpus_root: Path) -> str:
    try:
        path = Path(path_text).resolve(strict=True)
        relative = path.relative_to(corpus_root)
    except (OSError, ValueError) as error:
        raise ObserverError("Raw component is not a corpus file") from error
    component = relative.as_posix()
    return _validate_relative_path(component, "Raw corpus component")


def _expand_components(
    kind: str,
    components: list[str],
    manifest: dict[str, dict[str, object]],
) -> list[str]:
    selected = set(components)
    primary = components[0]
    primary_lower = primary.casefold()
    manifest_casefold = {path.casefold(): path for path in manifest}
    if kind == "dsl":
        if primary_lower.endswith(".dsl.dz"):
            base = primary[: -len(".dsl.dz")]
        elif primary_lower.endswith(".dsl"):
            base = primary[: -len(".dsl")]
        else:
            raise ObserverError("DSL observation has an invalid primary component")
        candidates = (
            f"{base}_abrv.dsl",
            f"{base}_abrv.dsl.dz",
            f"{base}.dsl.files.zip",
            f"{base}.dsl.dz.files.zip",
        )
        for candidate in candidates:
            found = manifest_casefold.get(candidate.casefold())
            if found is not None:
                selected.add(found)
    elif kind == "stardict":
        if not primary_lower.endswith(".ifo"):
            raise ObserverError("StarDict observation has an invalid primary component")
        base = primary[: -len(".ifo")]
        for suffix in (".idx", ".syn"):
            found = manifest_casefold.get(f"{base}{suffix}".casefold())
            if found is not None:
                selected.add(found)
        plain_dictionary = manifest_casefold.get(f"{base}.dict".casefold())
        compressed_dictionary = manifest_casefold.get(f"{base}.dict.dz".casefold())
        selected_dictionary = plain_dictionary or compressed_dictionary
        if selected_dictionary is not None:
            selected.add(selected_dictionary)
    elif kind == "mdict":
        if not primary_lower.endswith(".mdx"):
            raise ObserverError("MDict observation has an invalid primary component")
        base = primary[: -len(".mdx")]
        volume = 0
        while True:
            suffix = ".mdd" if volume == 0 else f".{volume}.mdd"
            found = manifest_casefold.get(f"{base}{suffix}".casefold())
            if found is None:
                break
            selected.add(found)
            volume += 1
    for component in selected:
        if component not in manifest:
            raise ObserverError(
                f"Observed component is absent from manifest: {component}"
            )
    return sorted(selected, key=lambda item: (item.casefold(), item))


def _snapshot_indexes(index_root: Path) -> dict[str, tuple[int, str, int]]:
    try:
        index_root.mkdir(parents=True, exist_ok=True)
        entries = sorted(index_root.iterdir(), key=lambda item: item.name)
    except OSError as error:
        raise ObserverError(f"Cannot inspect index directory: {index_root}") from error
    if len(entries) > MAX_RAW_DICTIONARIES * 4:
        raise ObserverError("Index directory exceeds the file-count bound")
    snapshot: dict[str, tuple[int, str, int]] = {}
    for path in entries:
        try:
            file_stat = path.stat(follow_symlinks=False)
        except OSError as error:
            raise ObserverError(f"Cannot inspect index file: {path}") from error
        if path.is_symlink() or not stat.S_ISREG(file_stat.st_mode):
            raise ObserverError(f"Index entries must be regular files: {path}")
        snapshot[path.name] = (
            file_stat.st_size,
            _sha256_file(path),
            file_stat.st_mtime_ns,
        )
    return snapshot


def _index_role(file_name: str) -> str:
    lowered = file_name.casefold()
    return (
        "full-text" if lowered.endswith(".gdfts") or "_fts" in lowered else "headword"
    )


def _index_observations(
    dictionaries: list[dict[str, object]],
    before: dict[str, tuple[int, str, int]],
    after: dict[str, tuple[int, str, int]],
) -> list[dict[str, object]]:
    id_to_key = {item["id"]: item["logical_key"] for item in dictionaries}
    observations: list[dict[str, object]] = []
    for file_name in sorted(set(before) | set(after)):
        matches = [
            dictionary_id
            for dictionary_id in id_to_key
            if file_name.startswith(dictionary_id)
        ]
        if not matches:
            continue
        dictionary_id = max(matches, key=len)
        previous = before.get(file_name)
        current = after.get(file_name)
        if current is None:
            size, digest, _ = previous  # type: ignore[misc]
            disposition = "removed"
        else:
            size, digest, _ = current
            if previous is None:
                disposition = "created"
            elif previous == current:
                disposition = "reused"
            else:
                disposition = "rebuilt"
        observations.append(
            {
                "dictionary_key": id_to_key[dictionary_id],
                "disposition": disposition,
                "elapsed_milliseconds": None,
                "file_name": file_name,
                "role": _index_role(file_name),
                "sha256": digest,
                "size": size,
            }
        )
    return sorted(observations, key=lambda item: (item["dictionary_key"], item["role"]))


def _diagnostics(
    raw_errors: list[dict[str, object]],
    dictionaries: list[dict[str, object]],
    corpus_root: Path,
    manifest: dict[str, dict[str, object]],
) -> list[dict[str, object]]:
    id_to_key = {item["id"]: item["logical_key"] for item in dictionaries}
    absolute_components = [
        (str(corpus_root / component).casefold(), component) for component in manifest
    ]
    diagnostics: list[dict[str, object]] = []
    for error in raw_errors:
        message = str(error["message"]).replace("\\", "/").casefold()
        source_component = next(
            (
                component
                for absolute, component in absolute_components
                if absolute.replace("\\", "/") in message
            ),
            None,
        )
        dictionary_id = str(error["dictionary_id"])
        diagnostics.append(
            {
                "category": "open" if dictionary_id else "discovery",
                "dictionary_key": id_to_key.get(dictionary_id),
                "message_code": error["code"],
                "severity": "error",
                "source_component": source_component,
            }
        )
    return sorted(
        diagnostics,
        key=lambda item: acceptance_result.canonical_json(
            [
                item["severity"],
                item["category"],
                item["dictionary_key"],
                item["source_component"],
                item["message_code"],
            ]
        ),
    )


def _atomic_json(path: Path, value: object) -> None:
    content = acceptance_result.canonical_json(value)
    temporary: Path | None = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{path.name}.", dir=path.parent
        )
        temporary = Path(temporary_name)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except OSError as error:
        raise ObserverError(f"Cannot publish JSON evidence: {path}") from error
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _clear_final_evidence(result_path: Path, acknowledgement_path: Path) -> None:
    try:
        acknowledgement_path.unlink(missing_ok=True)
        result_path.unlink(missing_ok=True)
    except OSError as error:
        raise ObserverError("Cannot clear incomplete acceptance evidence") from error


def _publish_evidence(
    result_path: Path,
    observation: dict[str, object],
    acknowledgement_path: Path,
    acknowledgement: dict[str, object],
) -> None:
    try:
        acceptance_result.write_observation(result_path, observation)
        _atomic_json(acknowledgement_path, acknowledgement)
    except (ObserverError, acceptance_result.ResultError):
        _clear_final_evidence(result_path, acknowledgement_path)
        raise


def observe(
    version: str,
    executable: Path,
    observer_arguments: list[str],
    dictionary_root: Path,
    manifest_path: Path,
    scenario: str,
) -> Path:
    expected_version = _required_environment("GOLDENDICT_ACCEPTANCE_VERSION")
    if version != expected_version:
        raise ObserverError("Observer version does not match the paired run")
    if scenario not in SUPPORTED_SCENARIOS:
        raise ObserverError("The observer received an unsupported lifecycle scenario")
    _validate_observer_arguments(observer_arguments)
    try:
        corpus_root = dictionary_root.resolve(strict=True)
        index_root = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_INDEX_ROOT")
        ).resolve(strict=True)
        evidence_root = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT")
        ).resolve(strict=True)
        result_path = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_RESULT_PATH")
        ).resolve(strict=False)
        acknowledgement_path = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_ACK_PATH")
        ).resolve(strict=False)
        conditions_path = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_CONDITIONS_FILE")
        ).resolve(strict=True)
        executable_path = executable.resolve(strict=True)
        manifest_file = manifest_path.resolve(strict=True)
    except OSError as error:
        raise ObserverError("Observer input path does not exist") from error
    if (
        Path(_required_environment("GOLDENDICT_ACCEPTANCE_CORPUS_ROOT")).resolve()
        != corpus_root
    ):
        raise ObserverError("Dictionary root does not match the paired corpus")
    if (
        result_path.parent != evidence_root
        or acknowledgement_path.parent != evidence_root
    ):
        raise ObserverError("Result paths must remain in the paired evidence directory")
    manifest_hash = _sha256_file(manifest_file)
    if manifest_hash != _required_environment(
        "GOLDENDICT_ACCEPTANCE_CORPUS_MANIFEST_SHA256"
    ):
        raise ObserverError("Manifest hash does not match the paired run")
    manifest = _read_json(manifest_file, MAX_RAW_BYTES, "Manifest")
    manifest_files = _manifest_files(manifest)
    conditions = _validate_conditions(
        _read_json(conditions_path, MAX_RAW_BYTES, "Conditions")
    )
    conditions_hash = hashlib.sha256(
        acceptance_result.canonical_json(conditions)
    ).hexdigest()
    if conditions_hash != _required_environment(
        "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256"
    ):
        raise ObserverError("Conditions hash does not match the paired run")
    _clear_final_evidence(result_path, acknowledgement_path)

    before = _snapshot_indexes(index_root)
    if scenario == "clean-discovery" and before:
        raise ObserverError("Clean discovery requires an empty index directory")
    if scenario not in ("clean-discovery", "companion-recovery") and not before:
        raise ObserverError(f"{scenario} requires previously created indexes")
    with tempfile.TemporaryDirectory(
        dir=evidence_root, prefix=".raw-observation-"
    ) as temporary:
        raw_path = Path(temporary) / "raw.json"
        command = [
            str(executable_path),
            *observer_arguments,
            "--dictionary-root",
            str(corpus_root),
            "--index-root",
            str(index_root),
            "--interface-language",
            conditions["locale"],
            "--conditions-sha256",
            conditions_hash,
            "--scenario",
            scenario,
            "--output",
            str(raw_path),
        ]
        try:
            completed = subprocess.run(command, check=False)
        except OSError as error:
            raise ObserverError(
                "Version-specific observer could not be launched"
            ) from error
        if completed.returncode != 0:
            raise ObserverError(
                f"Version-specific observer failed with exit code {completed.returncode}"
            )
        raw = _validate_raw(_read_json(raw_path, MAX_RAW_BYTES, "Raw observation"))
        if raw["conditions_sha256"] != conditions_hash:
            raise ObserverError("Raw observer did not acknowledge applied conditions")
    after = _snapshot_indexes(index_root)

    dictionaries: list[dict[str, object]] = []
    for item in raw["catalog"]:
        components = [
            _relative_component(str(component), corpus_root)
            for component in item["components"]
        ]
        primary_component = components[0]
        components = _expand_components(str(item["format"]), components, manifest_files)
        dictionaries.append(
            {
                "article_count": item["article_count"],
                "edition": item["edition"],
                "enabled": item["enabled"],
                "headword_count": item["headword_count"],
                "id": item["id"],
                "logical_key": f"{item['format']}:{primary_component}",
                "name": item["name"],
                "order": item["order"],
                "source_components": components,
                "source_language": item["source_language"],
                "target_language": item["target_language"],
            }
        )
    observation = {
        "conditions_sha256": _required_environment(
            "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256"
        ),
        "corpus_manifest_sha256": manifest_hash,
        "diagnostics": _diagnostics(
            raw["errors"], dictionaries, corpus_root, manifest_files
        ),
        "dictionaries": dictionaries,
        "indexes": _index_observations(dictionaries, before, after),
        "outcome": raw["outcome"],
        "pair_id": _required_environment("GOLDENDICT_ACCEPTANCE_PAIR_ID"),
        "phases": [
            {
                "dictionary_key": None,
                "name": phase["name"],
                "sequence": phase["sequence"],
                "status": phase["status"],
            }
            for phase in raw["phases"]
        ],
        "revision": _required_environment("GOLDENDICT_ACCEPTANCE_REVISION"),
        "scenario": scenario,
        "schema": acceptance_result.OBSERVATION_SCHEMA,
        "version": version,
    }
    acknowledgement = {
        "conditions_sha256": observation["conditions_sha256"],
        "corpus_manifest_sha256": observation["corpus_manifest_sha256"],
        "pair_id": observation["pair_id"],
        "revision": observation["revision"],
        "schema": acceptance_workspace.ACKNOWLEDGEMENT_SCHEMA,
        "version": version,
    }
    _publish_evidence(result_path, observation, acknowledgement_path, acknowledgement)
    return result_path


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", choices=acceptance_result.VERSIONS, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--observer-argument", action="append", default=[])
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--scenario", choices=SUPPORTED_SCENARIOS, required=True)
    parser.add_argument("--dictionary-root", type=Path, required=True)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        output = observe(
            options.version,
            options.executable,
            options.observer_argument,
            options.dictionary_root,
            options.manifest,
            options.scenario,
        )
    except (ObserverError, acceptance_result.ResultError) as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
