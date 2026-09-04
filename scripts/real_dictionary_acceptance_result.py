#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Validate and compare bounded real-dictionary acceptance observations."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import tempfile
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

OBSERVATION_SCHEMA = "goldendict-real-dictionary-observation-v1"
COMPARISON_SCHEMA = "goldendict-real-dictionary-comparison-v1"
MAX_RESULT_BYTES = 16 * 1024 * 1024
MAX_DICTIONARIES = 10_000
MAX_DIAGNOSTICS = 10_000
MAX_PHASES = 100_000
MAX_TEXT_BYTES = 4096
HASH_PATTERN = re.compile(r"^[0-9a-f]{64}$")
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")
VERSIONS = ("qt5", "qt6")
SCENARIOS = (
    "clean-discovery",
    "warm-restart",
    "explicit-rescan",
    "changed-source",
    "cancellation",
    "unavailable-companion",
    "companion-recovery",
)
OUTCOMES = ("completed", "cancelled", "failed")
PHASE_STATUSES = ("started", "completed", "cancelled", "failed")
DIAGNOSTIC_SEVERITIES = ("info", "warning", "error")
INDEX_DISPOSITIONS = ("created", "reused", "rebuilt", "present", "removed")
DIFFERENCE_KINDS = ("value", "missing-in-qt5", "missing-in-qt6")
TOKEN_PATTERN = re.compile(r"^[a-z0-9][a-z0-9._-]*$")


class ResultError(RuntimeError):
    """Raised when an observation or comparison is unsafe or inconsistent."""


def canonical_json(value: object) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("utf-8")


def _text(value: object, label: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not allow_empty and not value):
        raise ResultError(
            f"{label} must be a {'possibly empty ' if allow_empty else ''}string"
        )
    try:
        encoded = value.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ResultError(f"{label} must be valid UTF-8 text") from error
    if len(encoded) > MAX_TEXT_BYTES or "\x00" in value:
        raise ResultError(f"{label} exceeds the text bound")
    return value


def _hash(value: object, label: str) -> str:
    text = _text(value, label)
    if not HASH_PATTERN.fullmatch(text):
        raise ResultError(f"{label} must be a lowercase SHA-256")
    return text


def _revision(value: object, label: str) -> str:
    text = _text(value, label)
    if not REVISION_PATTERN.fullmatch(text):
        raise ResultError(f"{label} must be a lowercase 40-digit commit ID")
    return text


def _token(value: object, label: str) -> str:
    token = _text(value, label)
    if not TOKEN_PATTERN.fullmatch(token):
        raise ResultError(f"{label} must be a canonical token")
    return token


def _bounded_count(value: object, label: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ResultError(f"{label} must be a non-negative integer")
    if value > (1 << 63) - 1:
        raise ResultError(f"{label} exceeds the supported bound")
    return value


def _relative_component(value: object, label: str) -> str:
    component = _text(value, label)
    if "\\" in component or ":" in component:
        raise ResultError(f"{label} must be a normalized relative path")
    path = PurePosixPath(component)
    if path.is_absolute() or component.startswith("//"):
        raise ResultError(f"{label} must be corpus-relative")
    if (
        component == "."
        or path.as_posix() != component
        or any(part == ".." for part in path.parts)
    ):
        raise ResultError(f"{label} must be normalized and confined")
    return component


def _logical_key(value: object, label: str) -> str:
    logical_key = _text(value, label)
    kind, separator, component = logical_key.partition(":")
    if separator != ":" or not re.fullmatch(r"[a-z0-9][a-z0-9-]*", kind):
        raise ResultError(f"{label} must have a normalized format prefix")
    _relative_component(component, f"{label} source component")
    return logical_key


def _exact_keys(value: dict[str, object], expected: set[str], label: str) -> None:
    if set(value) != expected:
        missing = sorted(expected - set(value))
        extra = sorted(set(value) - expected)
        raise ResultError(f"{label} keys differ (missing={missing}, extra={extra})")


def _validate_dictionary(value: object, index: int) -> dict[str, object]:
    label = f"dictionaries[{index}]"
    if not isinstance(value, dict):
        raise ResultError(f"{label} must be an object")
    _exact_keys(
        value,
        {
            "article_count",
            "edition",
            "enabled",
            "headword_count",
            "id",
            "logical_key",
            "name",
            "order",
            "source_components",
            "source_language",
            "target_language",
        },
        label,
    )
    _logical_key(value["logical_key"], f"{label}.logical_key")
    _text(value["id"], f"{label}.id")
    _text(value["name"], f"{label}.name", allow_empty=True)
    _text(value["edition"], f"{label}.edition", allow_empty=True)
    _text(value["source_language"], f"{label}.source_language", allow_empty=True)
    _text(value["target_language"], f"{label}.target_language", allow_empty=True)
    if not isinstance(value["enabled"], bool):
        raise ResultError(f"{label}.enabled must be a boolean")
    order = value["order"]
    if order is not None:
        _bounded_count(order, f"{label}.order")
    _bounded_count(value["article_count"], f"{label}.article_count")
    _bounded_count(value["headword_count"], f"{label}.headword_count")
    components = value["source_components"]
    if not isinstance(components, list) or not components:
        raise ResultError(f"{label}.source_components must be a non-empty array")
    normalized = [
        _relative_component(component, f"{label}.source_components[{offset}]")
        for offset, component in enumerate(components)
    ]
    if len(set(normalized)) != len(normalized) or normalized != sorted(
        normalized, key=lambda item: (item.casefold(), item)
    ):
        raise ResultError(f"{label}.source_components must be unique and sorted")
    return value


def _validate_diagnostic(value: object, index: int) -> dict[str, object]:
    label = f"diagnostics[{index}]"
    if not isinstance(value, dict):
        raise ResultError(f"{label} must be an object")
    _exact_keys(
        value,
        {"category", "dictionary_key", "message_code", "severity", "source_component"},
        label,
    )
    _token(value["category"], f"{label}.category")
    _token(value["message_code"], f"{label}.message_code")
    if value["severity"] not in DIAGNOSTIC_SEVERITIES:
        raise ResultError(f"{label}.severity is unsupported")
    if value["dictionary_key"] is not None:
        _logical_key(value["dictionary_key"], f"{label}.dictionary_key")
    if value["source_component"] is not None:
        _relative_component(value["source_component"], f"{label}.source_component")
    return value


def _validate_comparable_dictionary(value: object, label: str) -> None:
    if not isinstance(value, dict):
        raise ResultError(f"{label} must be a dictionary object")
    _exact_keys(
        value,
        {
            "article_count",
            "edition",
            "enabled",
            "headword_count",
            "logical_key",
            "name",
            "order",
            "source_components",
            "source_language",
            "target_language",
        },
        label,
    )
    candidate = dict(value)
    candidate["id"] = "comparison-placeholder"
    _validate_dictionary(candidate, 0)


def _validate_dictionary_field_value(field: str, value: object, label: str) -> None:
    if field in ("article_count", "headword_count"):
        _bounded_count(value, label)
    elif field == "enabled":
        if not isinstance(value, bool):
            raise ResultError(f"{label} must be a boolean")
    elif field == "order":
        if value is not None:
            _bounded_count(value, label)
    elif field == "logical_key":
        _logical_key(value, label)
    elif field == "source_components":
        if not isinstance(value, list) or not value:
            raise ResultError(f"{label} must be a non-empty path array")
        components = [
            _relative_component(item, f"{label}[{index}]")
            for index, item in enumerate(value)
        ]
        if len(set(components)) != len(components) or components != sorted(
            components, key=lambda item: (item.casefold(), item)
        ):
            raise ResultError(f"{label} must be unique and sorted")
    elif field in ("edition", "name", "source_language", "target_language"):
        _text(value, label, allow_empty=True)
    else:
        raise ResultError(f"{label} has an unsupported dictionary field")


def _validate_diagnostic_comparison(value: object, label: str) -> None:
    if not isinstance(value, list) or len(value) > MAX_DIAGNOSTICS:
        raise ResultError(f"{label} must be a bounded diagnostic array")
    for index, item in enumerate(value):
        item_label = f"{label}[{index}]"
        if not isinstance(item, list) or len(item) != 5:
            raise ResultError(f"{item_label} must be a five-field array")
        severity, category, dictionary_key, source_component, message_code = item
        if severity not in DIAGNOSTIC_SEVERITIES:
            raise ResultError(f"{item_label} severity is unsupported")
        _token(category, f"{item_label} category")
        if dictionary_key is not None:
            _logical_key(dictionary_key, f"{item_label} dictionary key")
        if source_component is not None:
            _relative_component(source_component, f"{item_label} source component")
        _token(message_code, f"{item_label} message code")
    if value != sorted(value, key=canonical_json):
        raise ResultError(f"{label} must be sorted")


def _validate_phase_comparison(value: object, label: str) -> None:
    if not isinstance(value, list) or len(value) > MAX_PHASES:
        raise ResultError(f"{label} must be a bounded phase array")
    for index, item in enumerate(value):
        item_label = f"{label}[{index}]"
        if not isinstance(item, dict):
            raise ResultError(f"{item_label} must be an object")
        _exact_keys(item, {"dictionary_key", "name", "status"}, item_label)
        _token(item["name"], f"{item_label}.name")
        if item["status"] not in PHASE_STATUSES:
            raise ResultError(f"{item_label}.status is unsupported")
        if item["dictionary_key"] is not None:
            _logical_key(item["dictionary_key"], f"{item_label}.dictionary_key")


def _validate_difference(value: object, index: int) -> None:
    label = f"comparison.differences[{index}]"
    if not isinstance(value, dict):
        raise ResultError(f"{label} must be an object")
    _exact_keys(value, {"field", "kind", "qt5", "qt6"}, label)
    field = _text(value["field"], f"{label}.field")
    kind = value["kind"]
    if kind not in DIFFERENCE_KINDS:
        raise ResultError(f"{label}.kind is unsupported")
    qt5 = value["qt5"]
    qt6 = value["qt6"]
    if field == "outcome":
        if kind != "value" or qt5 not in OUTCOMES or qt6 not in OUTCOMES:
            raise ResultError(f"{label} has invalid outcome values")
        if qt5 == qt6:
            raise ResultError(f"{label} does not describe a difference")
        return
    if field == "diagnostics":
        if kind != "value":
            raise ResultError(f"{label}.kind must be value")
        _validate_diagnostic_comparison(qt5, f"{label}.qt5")
        _validate_diagnostic_comparison(qt6, f"{label}.qt6")
        if qt5 == qt6:
            raise ResultError(f"{label} does not describe a difference")
        return
    if field == "phases":
        if kind != "value":
            raise ResultError(f"{label}.kind must be value")
        _validate_phase_comparison(qt5, f"{label}.qt5")
        _validate_phase_comparison(qt6, f"{label}.qt6")
        if qt5 == qt6:
            raise ResultError(f"{label} does not describe a difference")
        return
    if field.startswith("indexes/"):
        index_path = field[len("indexes/") :]
        if "/" not in index_path:
            raise ResultError(f"{label}.field is malformed")
        dictionary_key, role = index_path.rsplit("/", 1)
        _logical_key(dictionary_key, f"{label}.field dictionary key")
        _token(role, f"{label}.field role")
        if kind != "value" or any(
            item is not None and item not in INDEX_DISPOSITIONS for item in (qt5, qt6)
        ):
            raise ResultError(f"{label} has invalid index dispositions")
        if qt5 == qt6:
            raise ResultError(f"{label} does not describe a difference")
        return
    if not field.startswith("dictionaries/"):
        raise ResultError(f"{label}.field is unsupported")
    dictionary_path = field[len("dictionaries/") :]
    if kind in ("missing-in-qt5", "missing-in-qt6"):
        _logical_key(dictionary_path, f"{label}.field dictionary key")
        missing, present = (qt5, qt6) if kind == "missing-in-qt5" else (qt6, qt5)
        if missing is not None:
            raise ResultError(f"{label} missing side must be null")
        _validate_comparable_dictionary(present, f"{label} present side")
        if present["logical_key"] != dictionary_path:
            raise ResultError(f"{label} present dictionary key does not match field")
        return
    fields = (
        "source_components",
        "source_language",
        "target_language",
        "article_count",
        "headword_count",
        "edition",
        "enabled",
        "order",
        "name",
    )
    for dictionary_field in fields:
        suffix = f"/{dictionary_field}"
        if dictionary_path.endswith(suffix):
            logical_key = dictionary_path[: -len(suffix)]
            _logical_key(logical_key, f"{label}.field dictionary key")
            _validate_dictionary_field_value(dictionary_field, qt5, f"{label}.qt5")
            _validate_dictionary_field_value(dictionary_field, qt6, f"{label}.qt6")
            if qt5 == qt6:
                raise ResultError(f"{label} does not describe a difference")
            return
    raise ResultError(f"{label}.field has an unsupported dictionary field")


def _validate_phase(value: object, index: int) -> dict[str, object]:
    label = f"phases[{index}]"
    if not isinstance(value, dict):
        raise ResultError(f"{label} must be an object")
    _exact_keys(value, {"dictionary_key", "name", "sequence", "status"}, label)
    sequence = _bounded_count(value["sequence"], f"{label}.sequence")
    if sequence != index:
        raise ResultError("phases must use a contiguous zero-based sequence")
    _token(value["name"], f"{label}.name")
    if value["status"] not in PHASE_STATUSES:
        raise ResultError(f"{label}.status is unsupported")
    if value["dictionary_key"] is not None:
        _logical_key(value["dictionary_key"], f"{label}.dictionary_key")
    return value


def _validate_index(value: object, index: int) -> dict[str, object]:
    label = f"indexes[{index}]"
    if not isinstance(value, dict):
        raise ResultError(f"{label} must be an object")
    _exact_keys(
        value,
        {
            "dictionary_key",
            "disposition",
            "elapsed_milliseconds",
            "file_name",
            "role",
            "sha256",
            "size",
        },
        label,
    )
    _logical_key(value["dictionary_key"], f"{label}.dictionary_key")
    _token(value["role"], f"{label}.role")
    file_name = _text(value["file_name"], f"{label}.file_name")
    if (
        "/" in file_name
        or "\\" in file_name
        or ":" in file_name
        or file_name in (".", "..")
    ):
        raise ResultError(f"{label}.file_name must be a single relative name")
    elapsed = value["elapsed_milliseconds"]
    if elapsed is not None:
        _bounded_count(elapsed, f"{label}.elapsed_milliseconds")
    if value["disposition"] not in INDEX_DISPOSITIONS:
        raise ResultError(f"{label}.disposition is unsupported")
    _hash(value["sha256"], f"{label}.sha256")
    _bounded_count(value["size"], f"{label}.size")
    return value


def validate_observation(
    value: object,
    *,
    expected_pair_id: str | None = None,
    expected_version: str | None = None,
    expected_revision: str | None = None,
    expected_manifest_hash: str | None = None,
    expected_conditions_hash: str | None = None,
) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ResultError("Observation root must be an object")
    _exact_keys(
        value,
        {
            "conditions_sha256",
            "corpus_manifest_sha256",
            "diagnostics",
            "dictionaries",
            "indexes",
            "outcome",
            "pair_id",
            "phases",
            "revision",
            "scenario",
            "schema",
            "version",
        },
        "observation",
    )
    if value["schema"] != OBSERVATION_SCHEMA:
        raise ResultError(f"Observation schema must be {OBSERVATION_SCHEMA}")
    pair_id = _hash(value["pair_id"], "pair_id")
    manifest_hash = _hash(value["corpus_manifest_sha256"], "corpus_manifest_sha256")
    conditions_hash = _hash(value["conditions_sha256"], "conditions_sha256")
    revision = _revision(value["revision"], "revision")
    version = value["version"]
    if version not in VERSIONS:
        raise ResultError("version must be qt5 or qt6")
    if value["scenario"] not in SCENARIOS:
        raise ResultError("scenario is unsupported")
    if value["outcome"] not in OUTCOMES:
        raise ResultError("outcome is unsupported")
    for actual, expected, label in (
        (pair_id, expected_pair_id, "pair_id"),
        (version, expected_version, "version"),
        (revision, expected_revision, "revision"),
        (manifest_hash, expected_manifest_hash, "corpus_manifest_sha256"),
        (conditions_hash, expected_conditions_hash, "conditions_sha256"),
    ):
        if expected is not None and actual != expected:
            raise ResultError(f"Observation {label} does not match the expected run")

    dictionaries = value["dictionaries"]
    diagnostics = value["diagnostics"]
    phases = value["phases"]
    indexes = value["indexes"]
    for collection, maximum, label in (
        (dictionaries, MAX_DICTIONARIES, "dictionaries"),
        (diagnostics, MAX_DIAGNOSTICS, "diagnostics"),
        (phases, MAX_PHASES, "phases"),
        (indexes, MAX_DICTIONARIES * 4, "indexes"),
    ):
        if not isinstance(collection, list) or len(collection) > maximum:
            raise ResultError(f"{label} must be a bounded array")
    validated_dictionaries = [
        _validate_dictionary(item, index) for index, item in enumerate(dictionaries)
    ]
    logical_keys = [item["logical_key"] for item in validated_dictionaries]
    if len(set(logical_keys)) != len(logical_keys):
        raise ResultError("dictionary logical keys must be unique")
    orders = [
        item["order"] for item in validated_dictionaries if item["order"] is not None
    ]
    if len(set(orders)) != len(orders):
        raise ResultError("dictionary order values must be unique")
    [_validate_diagnostic(item, index) for index, item in enumerate(diagnostics)]
    [_validate_phase(item, index) for index, item in enumerate(phases)]
    validated_indexes = [
        _validate_index(item, index) for index, item in enumerate(indexes)
    ]
    known_keys = set(logical_keys)
    for label, entries in (
        ("diagnostic", diagnostics),
        ("phase", phases),
        ("index", validated_indexes),
    ):
        for item in entries:
            key = item.get("dictionary_key")
            if key is not None and key not in known_keys:
                raise ResultError(f"{label} references an unknown dictionary key")
    index_keys = [(item["dictionary_key"], item["role"]) for item in validated_indexes]
    if len(set(index_keys)) != len(index_keys):
        raise ResultError("index dictionary/role pairs must be unique")
    return value


def read_observation(path: Path, **expected: str | None) -> dict[str, object]:
    try:
        with path.open("rb") as stream:
            content = stream.read(MAX_RESULT_BYTES + 1)
    except OSError as error:
        raise ResultError(f"Cannot read observation: {path}") from error
    if len(content) > MAX_RESULT_BYTES:
        raise ResultError("Observation exceeds the file-size bound")
    try:
        value = json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ResultError(f"Invalid UTF-8 JSON observation: {path}") from error
    observation = validate_observation(value, **expected)
    if content != canonical_json(observation):
        raise ResultError("Observation is not canonical JSON")
    return observation


def write_observation(path: Path, value: object) -> None:
    observation = validate_observation(value)
    content = canonical_json(observation)
    if len(content) > MAX_RESULT_BYTES:
        raise ResultError("Observation exceeds the file-size bound")
    try:
        parent = path.parent.resolve(strict=True)
    except OSError as error:
        raise ResultError(
            f"Observation parent does not exist: {path.parent}"
        ) from error
    temporary: Path | None = None
    try:
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{path.name}.", dir=parent
        )
        temporary = Path(temporary_name)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except OSError as error:
        raise ResultError(f"Cannot publish observation: {path}") from error
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _dictionary_comparable(item: dict[str, object]) -> dict[str, object]:
    return {key: value for key, value in item.items() if key != "id"}


def _diagnostic_key(item: dict[str, object]) -> tuple[object, ...]:
    return tuple(
        item[key]
        for key in (
            "severity",
            "category",
            "dictionary_key",
            "source_component",
            "message_code",
        )
    )


def _sorted_diagnostics(items: list[dict[str, object]]) -> list[list[object]]:
    values = [list(_diagnostic_key(item)) for item in items]
    return sorted(values, key=lambda item: canonical_json(item))


def compare_observations(qt5: object, qt6: object) -> dict[str, object]:
    left = validate_observation(qt5, expected_version="qt5")
    right = validate_observation(qt6, expected_version="qt6")
    for key in ("pair_id", "corpus_manifest_sha256", "conditions_sha256", "scenario"):
        if left[key] != right[key]:
            raise ResultError(f"Paired observations disagree on {key}")

    differences: list[dict[str, object]] = []
    if left["outcome"] != right["outcome"]:
        differences.append(
            {
                "field": "outcome",
                "kind": "value",
                "qt5": left["outcome"],
                "qt6": right["outcome"],
            }
        )
    left_by_key = {item["logical_key"]: item for item in left["dictionaries"]}
    right_by_key = {item["logical_key"]: item for item in right["dictionaries"]}
    for key in sorted(set(left_by_key) | set(right_by_key)):
        if key not in left_by_key:
            differences.append(
                {
                    "field": f"dictionaries/{key}",
                    "kind": "missing-in-qt5",
                    "qt5": None,
                    "qt6": _dictionary_comparable(right_by_key[key]),
                }
            )
        elif key not in right_by_key:
            differences.append(
                {
                    "field": f"dictionaries/{key}",
                    "kind": "missing-in-qt6",
                    "qt5": _dictionary_comparable(left_by_key[key]),
                    "qt6": None,
                }
            )
        else:
            left_dictionary = left_by_key[key]
            right_dictionary = right_by_key[key]
            for field in sorted(_dictionary_comparable(left_dictionary)):
                if left_dictionary[field] != right_dictionary[field]:
                    differences.append(
                        {
                            "field": f"dictionaries/{key}/{field}",
                            "kind": "value",
                            "qt5": left_dictionary[field],
                            "qt6": right_dictionary[field],
                        }
                    )
    left_diagnostics = _sorted_diagnostics(left["diagnostics"])
    right_diagnostics = _sorted_diagnostics(right["diagnostics"])
    if left_diagnostics != right_diagnostics:
        differences.append(
            {
                "field": "diagnostics",
                "kind": "value",
                "qt5": left_diagnostics,
                "qt6": right_diagnostics,
            }
        )
    left_phases = [
        {key: value for key, value in item.items() if key != "sequence"}
        for item in left["phases"]
    ]
    right_phases = [
        {key: value for key, value in item.items() if key != "sequence"}
        for item in right["phases"]
    ]
    if left_phases != right_phases:
        differences.append(
            {
                "field": "phases",
                "kind": "value",
                "qt5": left_phases,
                "qt6": right_phases,
            }
        )
    left_index_dispositions = {
        (item["dictionary_key"], item["role"]): item["disposition"]
        for item in left["indexes"]
    }
    right_index_dispositions = {
        (item["dictionary_key"], item["role"]): item["disposition"]
        for item in right["indexes"]
    }
    comparable_indexes = set(left_index_dispositions) & set(right_index_dispositions)
    for key in sorted(comparable_indexes):
        left_disposition = left_index_dispositions[key]
        right_disposition = right_index_dispositions[key]
        if left_disposition != right_disposition:
            differences.append(
                {
                    "field": f"indexes/{key[0]}/{key[1]}",
                    "kind": "value",
                    "qt5": left_disposition,
                    "qt6": right_disposition,
                }
            )

    differences.sort(key=lambda item: (item["field"], canonical_json(item)))
    comparison = {
        "conditions_sha256": left["conditions_sha256"],
        "corpus_manifest_sha256": left["corpus_manifest_sha256"],
        "difference_count": len(differences),
        "differences": differences,
        "equivalent": not differences,
        "pair_id": left["pair_id"],
        "qt5_revision": left["revision"],
        "qt6_revision": right["revision"],
        "scenario": left["scenario"],
        "schema": COMPARISON_SCHEMA,
        "version_specific": {
            "qt5_dictionary_ids": {
                key: item["id"] for key, item in sorted(left_by_key.items())
            },
            "qt5_indexes": sorted(
                left["indexes"], key=lambda item: (item["dictionary_key"], item["role"])
            ),
            "qt6_dictionary_ids": {
                key: item["id"] for key, item in sorted(right_by_key.items())
            },
            "qt6_indexes": sorted(
                right["indexes"],
                key=lambda item: (item["dictionary_key"], item["role"]),
            ),
        },
    }
    return validate_comparison(comparison)


def validate_comparison(value: object) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ResultError("Comparison root must be an object")
    _exact_keys(
        value,
        {
            "conditions_sha256",
            "corpus_manifest_sha256",
            "difference_count",
            "differences",
            "equivalent",
            "pair_id",
            "qt5_revision",
            "qt6_revision",
            "scenario",
            "schema",
            "version_specific",
        },
        "comparison",
    )
    if value["schema"] != COMPARISON_SCHEMA:
        raise ResultError(f"Comparison schema must be {COMPARISON_SCHEMA}")
    _hash(value["pair_id"], "comparison.pair_id")
    _hash(value["conditions_sha256"], "comparison.conditions_sha256")
    _hash(value["corpus_manifest_sha256"], "comparison.corpus_manifest_sha256")
    _revision(value["qt5_revision"], "comparison.qt5_revision")
    _revision(value["qt6_revision"], "comparison.qt6_revision")
    if value["scenario"] not in SCENARIOS:
        raise ResultError("comparison.scenario is unsupported")
    differences = value["differences"]
    if not isinstance(differences, list) or len(differences) > MAX_DICTIONARIES * 16:
        raise ResultError("comparison.differences must be a bounded array")
    difference_count = _bounded_count(
        value["difference_count"], "comparison.difference_count"
    )
    if difference_count != len(differences):
        raise ResultError("comparison.difference_count does not match")
    if not isinstance(value["equivalent"], bool) or value["equivalent"] != (
        not differences
    ):
        raise ResultError("comparison.equivalent does not match its differences")
    for index, difference in enumerate(differences):
        _validate_difference(difference, index)
    fields = [difference["field"] for difference in differences]
    if len(set(fields)) != len(fields):
        raise ResultError("comparison.differences fields must be unique")
    if differences != sorted(
        differences, key=lambda item: (item["field"], canonical_json(item))
    ):
        raise ResultError("comparison.differences must be sorted")
    version_specific = value["version_specific"]
    if not isinstance(version_specific, dict):
        raise ResultError("comparison.version_specific must be an object")
    _exact_keys(
        version_specific,
        {"qt5_dictionary_ids", "qt5_indexes", "qt6_dictionary_ids", "qt6_indexes"},
        "comparison.version_specific",
    )
    for key in ("qt5_dictionary_ids", "qt6_dictionary_ids"):
        if not isinstance(version_specific[key], dict):
            raise ResultError(f"comparison.version_specific.{key} must be an object")
        if len(version_specific[key]) > MAX_DICTIONARIES:
            raise ResultError(f"comparison.version_specific.{key} exceeds the bound")
        for logical_key, generated_id in version_specific[key].items():
            _logical_key(logical_key, f"comparison.version_specific.{key} key")
            _text(generated_id, f"comparison.version_specific.{key}[{logical_key}]")
    validated_indexes_by_version: dict[str, list[dict[str, object]]] = {}
    for key in ("qt5_indexes", "qt6_indexes"):
        if not isinstance(version_specific[key], list):
            raise ResultError(f"comparison.version_specific.{key} must be an array")
        if len(version_specific[key]) > MAX_DICTIONARIES * 4:
            raise ResultError(f"comparison.version_specific.{key} exceeds the bound")
        validated = [
            _validate_index(item, index)
            for index, item in enumerate(version_specific[key])
        ]
        if validated != sorted(
            validated, key=lambda item: (item["dictionary_key"], item["role"])
        ):
            raise ResultError(f"comparison.version_specific.{key} must be sorted")
        identities = [(item["dictionary_key"], item["role"]) for item in validated]
        if len(set(identities)) != len(identities):
            raise ResultError(
                f"comparison.version_specific.{key} contains duplicate indexes"
            )
        validated_indexes_by_version[key] = validated
    disposition_maps = {
        version: {
            (item["dictionary_key"], item["role"]): item["disposition"]
            for item in validated_indexes_by_version[f"{version}_indexes"]
        }
        for version in ("qt5", "qt6")
    }
    expected_index_differences: list[dict[str, object]] = []
    comparable_indexes = set(disposition_maps["qt5"]) & set(disposition_maps["qt6"])
    for identity in sorted(comparable_indexes):
        qt5_disposition = disposition_maps["qt5"][identity]
        qt6_disposition = disposition_maps["qt6"][identity]
        if qt5_disposition != qt6_disposition:
            expected_index_differences.append(
                {
                    "field": f"indexes/{identity[0]}/{identity[1]}",
                    "kind": "value",
                    "qt5": qt5_disposition,
                    "qt6": qt6_disposition,
                }
            )
    actual_index_differences = [
        difference
        for difference in differences
        if difference["field"].startswith("indexes/")
    ]
    if actual_index_differences != expected_index_differences:
        raise ResultError(
            "comparison index differences do not match version-specific evidence"
        )
    return value


def write_comparison(path: Path, value: object) -> None:
    content = canonical_json(validate_comparison(value))
    if len(content) > MAX_RESULT_BYTES:
        raise ResultError("Comparison exceeds the file-size bound")
    temporary: Path | None = None
    try:
        parent = path.parent.resolve(strict=True)
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{path.name}.", dir=parent
        )
        temporary = Path(temporary_name)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except OSError as error:
        raise ResultError(f"Cannot publish comparison: {path}") from error
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate = subparsers.add_parser("validate")
    validate.add_argument("--result", type=Path, required=True)
    compare = subparsers.add_parser("compare")
    compare.add_argument("--qt5", type=Path, required=True)
    compare.add_argument("--qt6", type=Path, required=True)
    compare.add_argument("--output", type=Path, required=True)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        if options.command == "validate":
            result = read_observation(options.result)
            print(hashlib.sha256(canonical_json(result)).hexdigest())
        else:
            comparison = compare_observations(
                read_observation(options.qt5), read_observation(options.qt6)
            )
            write_comparison(options.output, comparison)
            print(comparison["difference_count"])
    except ResultError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
