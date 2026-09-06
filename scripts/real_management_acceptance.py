#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Collect and compare bounded real-corpus dictionary-management evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

import qt5_real_dictionary_observer
import real_dictionary_acceptance_workspace as acceptance_workspace

CATALOG_SCHEMA = "goldendict-real-management-catalog-v1"
RAW_SCHEMA = "goldendict-real-management-raw-observation-v1"
OBSERVATION_SCHEMA = "goldendict-real-management-observation-v1"
COMPARISON_SCHEMA = "goldendict-real-management-comparison-v1"
SCENARIOS = ("clean-discovery", "warm-restart")
HASH_PATTERN = re.compile(r"[0-9a-f]{64}")
REVISION_PATTERN = re.compile(r"[0-9a-f]{40}")
MAX_METADATA_BYTES = 1024 * 1024
MAX_DIFFERENCES = 4096


class ManagementAcceptanceError(RuntimeError):
    """Raised when management evidence violates its bounded contract."""


def _canonical_json(value: object) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("utf-8")


def _sha256(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def _read_json(path: Path, label: str) -> tuple[dict[str, object], bytes]:
    try:
        content = path.read_bytes()
    except OSError as error:
        raise ManagementAcceptanceError(f"Cannot read {label}") from error
    if not content or len(content) > MAX_METADATA_BYTES:
        raise ManagementAcceptanceError(f"{label} is empty or oversized")
    try:
        value = json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ManagementAcceptanceError(f"{label} is not valid UTF-8 JSON") from error
    if not isinstance(value, dict):
        raise ManagementAcceptanceError(f"{label} root must be an object")
    return value, content


def _exact_keys(value: dict[str, object], expected: set[str], label: str) -> None:
    if set(value) != expected:
        raise ManagementAcceptanceError(f"{label} has unexpected fields")


def _text(value: object, label: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not allow_empty and not value) or "\0" in value:
        raise ManagementAcceptanceError(f"{label} must be bounded text")
    if len(value.encode("utf-8")) > 16 * 1024:
        raise ManagementAcceptanceError(f"{label} is oversized")
    return value


def _hash(value: object, label: str) -> str:
    if not isinstance(value, str) or HASH_PATTERN.fullmatch(value) is None:
        raise ManagementAcceptanceError(f"{label} must be a SHA-256 value")
    return value


def _relative_path(value: object, label: str) -> str:
    text = _text(value, label)
    path = PurePosixPath(text)
    if path.is_absolute() or ".." in path.parts or path.as_posix() != text:
        raise ManagementAcceptanceError(f"{label} must be a safe POSIX path")
    return text


def _confined_file(root: Path, relative: str, label: str) -> Path:
    try:
        path = (root / Path(*PurePosixPath(relative).parts)).resolve(strict=True)
        path.relative_to(root)
    except (OSError, ValueError) as error:
        raise ManagementAcceptanceError(f"{label} escapes the corpus") from error
    if not path.is_file():
        raise ManagementAcceptanceError(f"{label} is not a regular file")
    return path


def _unique_refs(value: object, known: set[str], label: str) -> list[str]:
    if not isinstance(value, list) or len(value) > 32:
        raise ManagementAcceptanceError(f"{label} must be a bounded array")
    result = [_text(item, f"{label} item") for item in value]
    if len(result) != len(set(result)) or any(item not in known for item in result):
        raise ManagementAcceptanceError(f"{label} contains duplicate or unknown IDs")
    return result


def read_catalog(
    path: Path,
    corpus_root: Path,
    *,
    expected_manifest_hash: str | None = None,
    expected_conditions_hash: str | None = None,
) -> tuple[dict[str, object], str]:
    catalog, content = _read_json(path, "management catalog")
    _exact_keys(
        catalog,
        {"conditions_sha256", "dictionaries", "manifest_sha256", "schema", "workflow"},
        "Management catalog",
    )
    if catalog["schema"] != CATALOG_SCHEMA:
        raise ManagementAcceptanceError("Management catalog schema is invalid")
    manifest_hash = _hash(catalog["manifest_sha256"], "Catalog manifest hash")
    conditions_hash = _hash(catalog["conditions_sha256"], "Catalog conditions hash")
    if expected_manifest_hash is not None and manifest_hash != expected_manifest_hash:
        raise ManagementAcceptanceError("Management catalog manifest hash mismatches")
    if (
        expected_conditions_hash is not None
        and conditions_hash != expected_conditions_hash
    ):
        raise ManagementAcceptanceError("Management catalog conditions hash mismatches")
    dictionaries = catalog["dictionaries"]
    if not isinstance(dictionaries, list) or not 2 <= len(dictionaries) <= 16:
        raise ManagementAcceptanceError("Management catalog needs 2-16 dictionaries")
    known: set[str] = set()
    primary: set[str] = set()
    for index, item in enumerate(dictionaries):
        label = f"Management dictionary {index}"
        if not isinstance(item, dict):
            raise ManagementAcceptanceError(f"{label} must be an object")
        _exact_keys(item, {"format", "id", "primary_component"}, label)
        identifier = _text(item["id"], f"{label} id")
        format_name = _text(item["format"], f"{label} format")
        component = _relative_path(item["primary_component"], f"{label} component")
        if format_name not in {"dsl", "mdict"}:
            raise ManagementAcceptanceError(f"{label} format is unsupported")
        if identifier in known or component.casefold() in primary:
            raise ManagementAcceptanceError("Management dictionaries must be unique")
        _confined_file(corpus_root, component, f"{label} component")
        known.add(identifier)
        primary.add(component.casefold())
    workflow = catalog["workflow"]
    if not isinstance(workflow, dict):
        raise ManagementAcceptanceError("Management workflow must be an object")
    _exact_keys(workflow, {"browse", "groups"}, "Management workflow")
    groups = workflow["groups"]
    if not isinstance(groups, list) or not 2 <= len(groups) <= 8:
        raise ManagementAcceptanceError("Management workflow needs 2-8 groups")
    group_ids: set[int] = set()
    for index, group in enumerate(groups):
        label = f"Management group {index}"
        if not isinstance(group, dict):
            raise ManagementAcceptanceError(f"{label} must be an object")
        _exact_keys(
            group,
            {
                "dictionary_ids",
                "id",
                "muted_dictionary_ids",
                "name",
                "popup_muted_dictionary_ids",
            },
            label,
        )
        group_id = group["id"]
        if not isinstance(group_id, int) or isinstance(group_id, bool) or group_id <= 0:
            raise ManagementAcceptanceError(f"{label} id is invalid")
        if group_id in group_ids:
            raise ManagementAcceptanceError("Management group IDs must be unique")
        group_ids.add(group_id)
        _text(group["name"], f"{label} name")
        members = _unique_refs(group["dictionary_ids"], known, f"{label} members")
        if not members:
            raise ManagementAcceptanceError(f"{label} must contain dictionaries")
        member_set = set(members)
        muted = _unique_refs(group["muted_dictionary_ids"], known, f"{label} muted")
        popup = _unique_refs(
            group["popup_muted_dictionary_ids"], known, f"{label} popup muted"
        )
        if not set(muted).issubset(member_set) or not set(popup).issubset(member_set):
            raise ManagementAcceptanceError(f"{label} mute state escapes membership")
    browse = workflow["browse"]
    if not isinstance(browse, list) or not browse or len(browse) > len(known):
        raise ManagementAcceptanceError("Management browse matrix is invalid")
    browsed: set[str] = set()
    for index, item in enumerate(browse):
        label = f"Management browse {index}"
        if not isinstance(item, dict):
            raise ManagementAcceptanceError(f"{label} must be an object")
        _exact_keys(item, {"dictionary_id", "page_size"}, label)
        identifier = _text(item["dictionary_id"], f"{label} dictionary")
        size = item["page_size"]
        if identifier not in known or identifier in browsed:
            raise ManagementAcceptanceError(f"{label} dictionary is invalid")
        if not isinstance(size, int) or isinstance(size, bool) or not 1 <= size <= 16:
            raise ManagementAcceptanceError(f"{label} page size is invalid")
        browsed.add(identifier)
    return catalog, _sha256(content)


def _component_from_absolute(value: object, corpus_root: Path, label: str) -> str:
    text = _text(value, label)
    try:
        path = Path(text).resolve(strict=True)
        relative = path.relative_to(corpus_root)
    except (OSError, ValueError) as error:
        raise ManagementAcceptanceError(f"{label} escapes the corpus") from error
    return PurePosixPath(*relative.parts).as_posix()


def _signature(value: object, label: str) -> dict[str, object]:
    text = _text(value, label, allow_empty=True)
    encoded = text.encode("utf-8")
    return {"sha256": _sha256(encoded), "utf8_bytes": len(encoded)}


def _visible_description(value: object, label: str) -> str:
    text = _text(value, label, allow_empty=True)
    # Qt 5 converts MDict HTML metadata through QTextDocument while Qt 6
    # exposes bounded plain metadata. Compare the visible token stream here;
    # line layout belongs to the separate paired screenshot gate.
    return " ".join(text.replace("\ufffc", " ").split())


def _normalize_raw(
    raw: dict[str, object],
    catalog: dict[str, object],
    *,
    catalog_hash: str,
    corpus_root: Path,
    pair_id: str,
    version: str,
    revision: str,
    scenario: str,
    manifest_hash: str,
    conditions_hash: str,
) -> dict[str, object]:
    _exact_keys(
        raw,
        {
            "browse",
            "catalog_sha256",
            "conditions_sha256",
            "dictionaries",
            "groups",
            "rescan_dictionary_ids",
            "scenario",
            "schema",
        },
        "Raw management observation",
    )
    if (
        raw["schema"] != RAW_SCHEMA
        or raw["catalog_sha256"] != catalog_hash
        or raw["conditions_sha256"] != conditions_hash
        or raw["scenario"] != scenario
    ):
        raise ManagementAcceptanceError("Raw management identity is invalid")
    expected_dictionaries = catalog["dictionaries"]
    raw_dictionaries = raw["dictionaries"]
    if not isinstance(expected_dictionaries, list) or not isinstance(
        raw_dictionaries, list
    ):
        raise ManagementAcceptanceError("Raw management dictionaries are invalid")
    if len(expected_dictionaries) != len(raw_dictionaries):
        raise ManagementAcceptanceError("Raw management dictionary count mismatches")
    normalized_dictionaries: list[dict[str, object]] = []
    for index, (expected, item) in enumerate(
        zip(expected_dictionaries, raw_dictionaries, strict=True)
    ):
        label = f"Raw management dictionary {index}"
        if not isinstance(expected, dict) or not isinstance(item, dict):
            raise ManagementAcceptanceError(f"{label} is invalid")
        _exact_keys(
            item,
            {
                "article_count",
                "catalog_id",
                "components",
                "description",
                "headword_count",
                "name",
                "source_language",
                "target_language",
            },
            label,
        )
        if item["catalog_id"] != expected["id"]:
            raise ManagementAcceptanceError(f"{label} does not match catalog order")
        components = item["components"]
        if not isinstance(components, list) or not 1 <= len(components) <= 16:
            raise ManagementAcceptanceError(f"{label} components are invalid")
        normalized_components = [
            _component_from_absolute(component, corpus_root, f"{label} component")
            for component in components
        ]
        if expected["primary_component"] not in normalized_components:
            raise ManagementAcceptanceError(f"{label} omits its primary component")
        counts: list[int] = []
        for field in ("article_count", "headword_count"):
            value = item[field]
            if not isinstance(value, int) or isinstance(value, bool) or value < 0:
                raise ManagementAcceptanceError(f"{label} {field} is invalid")
            counts.append(value)
        normalized_dictionaries.append(
            {
                "article_count": counts[0],
                "catalog_id": expected["id"],
                "components": normalized_components,
                "description": _signature(
                    _visible_description(item["description"], f"{label} description"),
                    f"{label} visible description",
                ),
                "headword_count": counts[1],
                "name": _text(item["name"], f"{label} name"),
                "source_language": _text(
                    item["source_language"],
                    f"{label} source language",
                    allow_empty=True,
                ),
                "target_language": _text(
                    item["target_language"],
                    f"{label} target language",
                    allow_empty=True,
                ),
            }
        )
    expected_groups = catalog["workflow"]["groups"]  # type: ignore[index]
    if raw["groups"] != expected_groups:
        raise ManagementAcceptanceError(
            "Persisted management groups do not match catalog"
        )
    expected_ids = [item["id"] for item in expected_dictionaries]  # type: ignore[index]
    if raw["rescan_dictionary_ids"] != expected_ids:
        raise ManagementAcceptanceError("Source rescan changed selected dictionaries")
    expected_browse = catalog["workflow"]["browse"]  # type: ignore[index]
    raw_browse = raw["browse"]
    if not isinstance(expected_browse, list) or not isinstance(raw_browse, list):
        raise ManagementAcceptanceError("Raw browse evidence is invalid")
    if len(expected_browse) != len(raw_browse):
        raise ManagementAcceptanceError("Raw browse evidence count mismatches")
    normalized_browse: list[dict[str, object]] = []
    for index, (expected, item) in enumerate(
        zip(expected_browse, raw_browse, strict=True)
    ):
        label = f"Raw browse evidence {index}"
        if not isinstance(expected, dict) or not isinstance(item, dict):
            raise ManagementAcceptanceError(f"{label} is invalid")
        _exact_keys(item, {"dictionary_id", "headwords"}, label)
        if item["dictionary_id"] != expected["dictionary_id"]:
            raise ManagementAcceptanceError(f"{label} dictionary mismatches")
        headwords = item["headwords"]
        expected_count = expected["page_size"] * 2
        if (
            not isinstance(headwords, list)
            or not headwords
            or len(headwords) > expected_count
        ):
            raise ManagementAcceptanceError(f"{label} headwords are invalid")
        normalized_browse.append(
            {
                "dictionary_id": expected["dictionary_id"],
                "headwords": [
                    _signature(headword, f"{label} headword") for headword in headwords
                ],
            }
        )
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": conditions_hash,
        "browse": normalized_browse,
        "dictionaries": normalized_dictionaries,
        "groups": raw["groups"],
        "manifest_sha256": manifest_hash,
        "pair_id": pair_id,
        "revision": revision,
        "scenario": scenario,
        "schema": OBSERVATION_SCHEMA,
        "version": version,
    }


def _atomic_json(path: Path, value: object) -> None:
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=path.parent, prefix=f".{path.name}.", delete=False
        ) as output:
            temporary = Path(output.name)
            output.write(_canonical_json(value))
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except OSError:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        raise


def _required_environment(name: str) -> str:
    value = os.environ.get(name, "")
    if not value:
        raise ManagementAcceptanceError(
            f"Required environment variable is missing: {name}"
        )
    return value


def observe(
    *,
    adapter: str,
    dictionary_root: Path,
    catalog_path: Path,
    scenario: str,
    output: Path,
    qt6_observer: Path | None = None,
    qt5_executable: Path | None = None,
    qt5_provenance: Path | None = None,
    qt5_runtime_bins: list[Path] | None = None,
    qt5_plugin_path: Path | None = None,
    timeout_seconds: int = 3600,
) -> Path:
    if adapter not in {"qt5", "qt6"} or scenario not in SCENARIOS:
        raise ManagementAcceptanceError("Unsupported management adapter or scenario")
    if _required_environment("GOLDENDICT_ACCEPTANCE_VERSION") != adapter:
        raise ManagementAcceptanceError("Management adapter does not match paired run")
    pair_id = _hash(_required_environment("GOLDENDICT_ACCEPTANCE_PAIR_ID"), "Pair id")
    revision = _required_environment("GOLDENDICT_ACCEPTANCE_REVISION")
    if REVISION_PATTERN.fullmatch(revision) is None:
        raise ManagementAcceptanceError("Pair revision is invalid")
    manifest_hash = _hash(
        _required_environment("GOLDENDICT_ACCEPTANCE_CORPUS_MANIFEST_SHA256"),
        "Pair manifest hash",
    )
    conditions_hash = _hash(
        _required_environment("GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256"),
        "Pair conditions hash",
    )
    try:
        corpus_root = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_CORPUS_ROOT")
        ).resolve(strict=True)
        supplied_root = dictionary_root.resolve(strict=True)
        evidence_root = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_EVIDENCE_ROOT")
        ).resolve(strict=True)
        index_root = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_INDEX_ROOT")
        ).resolve(strict=True)
        conditions_path = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_CONDITIONS_FILE")
        ).resolve(strict=True)
        acknowledgement_path = Path(
            _required_environment("GOLDENDICT_ACCEPTANCE_ACK_PATH")
        ).resolve(strict=False)
        config_root = Path(_required_environment("APPDATA")).resolve(strict=True)
    except OSError as error:
        raise ManagementAcceptanceError(
            "Paired management path cannot be resolved"
        ) from error
    if supplied_root != corpus_root:
        raise ManagementAcceptanceError("Dictionary root does not match paired corpus")
    conditions, _ = _read_json(conditions_path, "acceptance conditions")
    if _sha256(_canonical_json(conditions)) != conditions_hash:
        raise ManagementAcceptanceError("Acceptance conditions hash does not match")
    catalog, catalog_hash = read_catalog(
        catalog_path,
        corpus_root,
        expected_manifest_hash=manifest_hash,
        expected_conditions_hash=conditions_hash,
    )
    output = output.resolve(strict=False)
    if (
        output.parent != evidence_root
        or output.suffix.lower() != ".json"
        or output.exists()
    ):
        raise ManagementAcceptanceError(
            "Management output must be a new evidence-root JSON child"
        )
    with tempfile.TemporaryDirectory(
        dir=evidence_root, prefix=".raw-observation-management-"
    ) as temporary:
        raw_path = Path(temporary) / "raw.json"
        if adapter == "qt6":
            if qt6_observer is None:
                raise ManagementAcceptanceError("Qt 6 management observer is required")
            configuration_path = (
                config_root / "GoldenDict" / "management-acceptance.conf"
            )
            command = [
                str(qt6_observer.resolve(strict=True)),
                "--dictionary-root",
                str(corpus_root),
                "--index-root",
                str(index_root),
                "--configuration",
                str(configuration_path),
                "--conditions-sha256",
                conditions_hash,
                "--catalog",
                str(catalog_path.resolve(strict=True)),
                "--scenario",
                scenario,
                "--output",
                str(raw_path),
            ]
            try:
                completed = subprocess.run(
                    command, check=False, timeout=timeout_seconds
                )
            except (OSError, subprocess.TimeoutExpired) as error:
                raise ManagementAcceptanceError(
                    "Qt 6 management observer did not complete"
                ) from error
            if completed.returncode != 0:
                raise ManagementAcceptanceError(
                    f"Qt 6 management observer failed with exit code {completed.returncode}"
                )
        else:
            if qt5_executable is None or qt5_provenance is None:
                raise ManagementAcceptanceError(
                    "Qt 5 executable and provenance are required"
                )
            qt5_real_dictionary_observer.observe(
                qt5_executable,
                qt5_provenance,
                qt5_runtime_bins or [],
                qt5_plugin_path,
                corpus_root,
                index_root,
                str(conditions["locale"]),
                conditions_hash,
                scenario,
                raw_path,
                timeout_seconds,
                management_catalog=catalog_path,
                management_catalog_sha256=catalog_hash,
            )
        raw, _ = _read_json(raw_path, "raw management observation")
        normalized = _normalize_raw(
            raw,
            catalog,
            catalog_hash=catalog_hash,
            corpus_root=corpus_root,
            pair_id=pair_id,
            version=adapter,
            revision=revision,
            scenario=scenario,
            manifest_hash=manifest_hash,
            conditions_hash=conditions_hash,
        )
        acknowledgement = {
            "conditions_sha256": conditions_hash,
            "corpus_manifest_sha256": manifest_hash,
            "pair_id": pair_id,
            "revision": revision,
            "schema": acceptance_workspace.ACKNOWLEDGEMENT_SCHEMA,
            "version": adapter,
        }
        if acknowledgement_path.exists():
            raise ManagementAcceptanceError("Management acknowledgement already exists")
        try:
            _atomic_json(output, normalized)
            _atomic_json(acknowledgement_path, acknowledgement)
        except OSError as error:
            output.unlink(missing_ok=True)
            acknowledgement_path.unlink(missing_ok=True)
            raise ManagementAcceptanceError(
                "Management observation could not be published"
            ) from error
    return output


def _pair_contract(path: Path, catalog: dict[str, object]) -> dict[str, object]:
    pair, _ = _read_json(path, "acceptance pair")
    corpus = pair.get("corpus")
    runs = pair.get("runs")
    if (
        pair.get("schema") != acceptance_workspace.PAIR_SCHEMA
        or not isinstance(corpus, dict)
        or not isinstance(runs, dict)
    ):
        raise ManagementAcceptanceError("Acceptance pair is invalid")
    result = {
        "conditions_sha256": _hash(
            pair.get("conditions_sha256"), "Pair conditions hash"
        ),
        "manifest_sha256": _hash(corpus.get("manifest_sha256"), "Pair manifest hash"),
        "pair_id": _hash(pair.get("pair_id"), "Pair id"),
        "qt5_revision": (
            runs.get("qt5", {}).get("revision")
            if isinstance(runs.get("qt5"), dict)
            else None
        ),
        "qt6_revision": (
            runs.get("qt6", {}).get("revision")
            if isinstance(runs.get("qt6"), dict)
            else None
        ),
    }
    if (
        result["conditions_sha256"] != catalog["conditions_sha256"]
        or result["manifest_sha256"] != catalog["manifest_sha256"]
        or not isinstance(result["qt5_revision"], str)
        or not isinstance(result["qt6_revision"], str)
    ):
        raise ManagementAcceptanceError("Acceptance pair does not match catalog")
    return result


def _validate_signature(value: object, label: str, *, allow_empty: bool) -> None:
    if not isinstance(value, dict):
        raise ManagementAcceptanceError(f"{label} must be an object")
    _exact_keys(value, {"sha256", "utf8_bytes"}, label)
    _hash(value["sha256"], f"{label} hash")
    byte_count = value["utf8_bytes"]
    minimum = 0 if allow_empty else 1
    if (
        not isinstance(byte_count, int)
        or isinstance(byte_count, bool)
        or not minimum <= byte_count <= 16 * 1024
    ):
        raise ManagementAcceptanceError(f"{label} byte count is invalid")


def _validate_observation_payload(
    value: dict[str, object], catalog: dict[str, object]
) -> None:
    expected_dictionaries = catalog["dictionaries"]
    dictionaries = value["dictionaries"]
    if not isinstance(expected_dictionaries, list) or not isinstance(
        dictionaries, list
    ):
        raise ManagementAcceptanceError(
            "Management observation dictionaries are invalid"
        )
    if len(dictionaries) != len(expected_dictionaries):
        raise ManagementAcceptanceError(
            "Management observation dictionary count mismatches catalog"
        )

    dictionary_by_id: dict[str, dict[str, object]] = {}
    for index, (expected, item) in enumerate(
        zip(expected_dictionaries, dictionaries, strict=True)
    ):
        label = f"Management observation dictionary {index}"
        if not isinstance(expected, dict) or not isinstance(item, dict):
            raise ManagementAcceptanceError(f"{label} is invalid")
        _exact_keys(
            item,
            {
                "article_count",
                "catalog_id",
                "components",
                "description",
                "headword_count",
                "name",
                "source_language",
                "target_language",
            },
            label,
        )
        identifier = expected["id"]
        if item["catalog_id"] != identifier or not isinstance(identifier, str):
            raise ManagementAcceptanceError(f"{label} does not match catalog order")
        components = item["components"]
        if not isinstance(components, list) or not 1 <= len(components) <= 16:
            raise ManagementAcceptanceError(f"{label} components are invalid")
        normalized_components = [
            _relative_path(component, f"{label} component") for component in components
        ]
        if (
            len({component.casefold() for component in normalized_components})
            != len(normalized_components)
            or expected["primary_component"] not in normalized_components
        ):
            raise ManagementAcceptanceError(f"{label} components mismatch catalog")
        for field in ("article_count", "headword_count"):
            count = item[field]
            if not isinstance(count, int) or isinstance(count, bool) or count < 0:
                raise ManagementAcceptanceError(f"{label} {field} is invalid")
        _validate_signature(
            item["description"], f"{label} description", allow_empty=True
        )
        _text(item["name"], f"{label} name")
        _text(item["source_language"], f"{label} source language", allow_empty=True)
        _text(item["target_language"], f"{label} target language", allow_empty=True)
        dictionary_by_id[identifier] = item

    workflow = catalog["workflow"]
    if not isinstance(workflow, dict) or value["groups"] != workflow["groups"]:
        raise ManagementAcceptanceError(
            "Management observation groups do not match catalog"
        )
    expected_browse = workflow["browse"]
    browse = value["browse"]
    if not isinstance(expected_browse, list) or not isinstance(browse, list):
        raise ManagementAcceptanceError("Management observation browse is invalid")
    if len(browse) != len(expected_browse):
        raise ManagementAcceptanceError(
            "Management observation browse count mismatches catalog"
        )
    for index, (expected, item) in enumerate(zip(expected_browse, browse, strict=True)):
        label = f"Management observation browse {index}"
        if not isinstance(expected, dict) or not isinstance(item, dict):
            raise ManagementAcceptanceError(f"{label} is invalid")
        _exact_keys(item, {"dictionary_id", "headwords"}, label)
        identifier = expected["dictionary_id"]
        if item["dictionary_id"] != identifier or identifier not in dictionary_by_id:
            raise ManagementAcceptanceError(f"{label} does not match catalog order")
        page_size = expected["page_size"]
        headwords = item["headwords"]
        if (
            not isinstance(page_size, int)
            or not isinstance(headwords, list)
            or not page_size < len(headwords) <= page_size * 2
        ):
            raise ManagementAcceptanceError(
                f"{label} does not contain two bounded pages"
            )
        for headword_index, headword in enumerate(headwords):
            _validate_signature(
                headword, f"{label} headword {headword_index}", allow_empty=False
            )
        headword_count = dictionary_by_id[identifier]["headword_count"]
        if not isinstance(headword_count, int) or headword_count < len(headwords):
            raise ManagementAcceptanceError(
                f"{label} exceeds dictionary headword count"
            )


def _read_observation(
    path: Path,
    pair: dict[str, object],
    catalog: dict[str, object],
    catalog_hash: str,
    version: str,
    scenario: str,
) -> dict[str, object]:
    value, _ = _read_json(path, "management observation")
    required = {
        "browse",
        "catalog_sha256",
        "conditions_sha256",
        "dictionaries",
        "groups",
        "manifest_sha256",
        "pair_id",
        "revision",
        "scenario",
        "schema",
        "version",
    }
    _exact_keys(value, required, "Management observation")
    if (
        value["schema"] != OBSERVATION_SCHEMA
        or value["catalog_sha256"] != catalog_hash
        or value["conditions_sha256"] != pair["conditions_sha256"]
        or value["manifest_sha256"] != pair["manifest_sha256"]
        or value["pair_id"] != pair["pair_id"]
        or value["revision"] != pair[f"{version}_revision"]
        or value["version"] != version
        or value["scenario"] != scenario
    ):
        raise ManagementAcceptanceError("Management observation identity is invalid")
    _validate_observation_payload(value, catalog)
    return value


def _append_difference(
    output: list[dict[str, object]],
    *,
    actual: object,
    expected: object,
    field: str,
    kind: str,
    observation: str,
) -> None:
    if len(output) >= MAX_DIFFERENCES:
        raise ManagementAcceptanceError(
            "Management comparison exceeds its difference bound"
        )
    output.append(
        {
            "actual": actual,
            "expected": expected,
            "field": field,
            "kind": kind,
            "observation": observation,
        }
    )


def _collect_differences(
    expected: object,
    actual: object,
    field: str,
    observation: str,
    output: list[dict[str, object]],
) -> None:
    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(set(expected) | set(actual)):
            child = f"{field}/{str(key).replace('~', '~0').replace('/', '~1')}"
            if key not in expected:
                _append_difference(
                    output,
                    actual=actual[key],
                    expected=None,
                    field=child,
                    kind="extra-in-actual",
                    observation=observation,
                )
            elif key not in actual:
                _append_difference(
                    output,
                    actual=None,
                    expected=expected[key],
                    field=child,
                    kind="missing-in-actual",
                    observation=observation,
                )
            else:
                _collect_differences(
                    expected[key], actual[key], child, observation, output
                )
        return
    if isinstance(expected, list) and isinstance(actual, list):
        for index in range(max(len(expected), len(actual))):
            child = f"{field}/{index}"
            if index >= len(expected):
                _append_difference(
                    output,
                    actual=actual[index],
                    expected=None,
                    field=child,
                    kind="extra-in-actual",
                    observation=observation,
                )
            elif index >= len(actual):
                _append_difference(
                    output,
                    actual=None,
                    expected=expected[index],
                    field=child,
                    kind="missing-in-actual",
                    observation=observation,
                )
            else:
                _collect_differences(
                    expected[index], actual[index], child, observation, output
                )
        return
    if expected != actual:
        _append_difference(
            output,
            actual=actual,
            expected=expected,
            field=field,
            kind="value",
            observation=observation,
        )


def compare(
    qt5_clean: Path,
    qt5_warm: Path,
    qt6_clean: Path,
    qt6_warm: Path,
    *,
    catalog: dict[str, object],
    catalog_hash: str,
    pair: dict[str, object],
) -> dict[str, object]:
    identities = {
        "qt5-clean": (qt5_clean, "qt5", "clean-discovery"),
        "qt5-warm": (qt5_warm, "qt5", "warm-restart"),
        "qt6-clean": (qt6_clean, "qt6", "clean-discovery"),
        "qt6-warm": (qt6_warm, "qt6", "warm-restart"),
    }
    observations = {
        name: _read_observation(path, pair, catalog, catalog_hash, version, scenario)
        for name, (path, version, scenario) in identities.items()
    }
    reference = {
        field: observations["qt5-clean"][field]
        for field in ("browse", "dictionaries", "groups")
    }
    differences: list[dict[str, object]] = []
    for name in ("qt5-warm", "qt6-clean", "qt6-warm"):
        candidate = {
            field: observations[name][field]
            for field in ("browse", "dictionaries", "groups")
        }
        _collect_differences(reference, candidate, "management", name, differences)
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": pair["conditions_sha256"],
        "difference_count": len(differences),
        "differences": differences,
        "equivalent": not differences,
        "manifest_sha256": pair["manifest_sha256"],
        "pair_id": pair["pair_id"],
        "schema": COMPARISON_SCHEMA,
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    observe_parser = commands.add_parser("observe")
    observe_parser.add_argument("--adapter", choices=("qt5", "qt6"), required=True)
    observe_parser.add_argument("--dictionary-root", type=Path, required=True)
    observe_parser.add_argument("--catalog", type=Path, required=True)
    observe_parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    observe_parser.add_argument("--output", type=Path, required=True)
    observe_parser.add_argument("--qt6-observer", type=Path)
    observe_parser.add_argument("--qt5-executable", type=Path)
    observe_parser.add_argument("--qt5-provenance", type=Path)
    observe_parser.add_argument(
        "--qt5-runtime-bin", type=Path, action="append", default=[]
    )
    observe_parser.add_argument("--qt5-plugin-path", type=Path)
    observe_parser.add_argument("--timeout-seconds", type=int, default=3600)
    compare_parser = commands.add_parser("compare")
    compare_parser.add_argument("--qt5-clean", type=Path, required=True)
    compare_parser.add_argument("--qt5-warm", type=Path, required=True)
    compare_parser.add_argument("--qt6-clean", type=Path, required=True)
    compare_parser.add_argument("--qt6-warm", type=Path, required=True)
    compare_parser.add_argument("--catalog", type=Path, required=True)
    compare_parser.add_argument("--dictionary-root", type=Path, required=True)
    compare_parser.add_argument("--pair", type=Path, required=True)
    compare_parser.add_argument("--output", type=Path, required=True)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        if options.command == "observe":
            result = observe(
                adapter=options.adapter,
                dictionary_root=options.dictionary_root,
                catalog_path=options.catalog,
                scenario=options.scenario,
                output=options.output,
                qt6_observer=options.qt6_observer,
                qt5_executable=options.qt5_executable,
                qt5_provenance=options.qt5_provenance,
                qt5_runtime_bins=options.qt5_runtime_bin,
                qt5_plugin_path=options.qt5_plugin_path,
                timeout_seconds=options.timeout_seconds,
            )
            print(result)
        else:
            corpus_root = options.dictionary_root.resolve(strict=True)
            catalog, catalog_hash = read_catalog(options.catalog, corpus_root)
            pair = _pair_contract(options.pair, catalog)
            comparison = compare(
                options.qt5_clean,
                options.qt5_warm,
                options.qt6_clean,
                options.qt6_warm,
                catalog=catalog,
                catalog_hash=catalog_hash,
                pair=pair,
            )
            output = options.output.resolve(strict=False)
            if output.exists() or not output.parent.is_dir():
                raise ManagementAcceptanceError("Comparison output must be a new file")
            _atomic_json(output, comparison)
            print(output)
    except (ManagementAcceptanceError, OSError) as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
