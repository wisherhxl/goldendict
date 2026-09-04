#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Collect and compare bounded real-DSL article/resource evidence."""

from __future__ import annotations

import argparse
import base64
import binascii
import hashlib
import html.parser
import json
import os
import re
import subprocess
import tempfile
import unicodedata
import zipfile
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

import qt5_real_dictionary_observer
import real_dictionary_acceptance_workspace as acceptance_workspace

CATALOG_SCHEMA = "goldendict-dsl-query-resource-catalog-v1"
RAW_SCHEMA = "goldendict-real-dsl-raw-observation-v1"
OBSERVATION_SCHEMA = "goldendict-real-dsl-observation-v1"
COMPARISON_SCHEMA = "goldendict-real-dsl-comparison-v1"
SCENARIOS = ("clean-discovery", "warm-restart")
MAX_METADATA_BYTES = 16 * 1024 * 1024
MAX_ARTICLE_BYTES = 4 * 1024 * 1024
MAX_RESOURCE_BYTES = 16 * 1024 * 1024
MAX_ARCHIVE_MEMBERS = 500_000
HASH_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class DslAcceptanceError(RuntimeError):
    """Raised when real-DSL evidence is unsafe or inconsistent."""


def _canonical_json(value: object) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("utf-8")


def _sha256(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def _read_json(path: Path, label: str) -> tuple[dict[str, object], bytes]:
    try:
        if path.stat().st_size > MAX_METADATA_BYTES:
            raise DslAcceptanceError(f"{label} exceeds the metadata size bound")
        with path.open("rb") as source:
            content = source.read(MAX_METADATA_BYTES + 1)
    except OSError as error:
        raise DslAcceptanceError(f"Cannot read {label}: {path}") from error
    if len(content) > MAX_METADATA_BYTES:
        raise DslAcceptanceError(f"{label} exceeds the metadata size bound")
    try:
        value = json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise DslAcceptanceError(f"{label} is not valid UTF-8 JSON") from error
    if not isinstance(value, dict):
        raise DslAcceptanceError(f"{label} root must be an object")
    return value, content


def _text(value: object, label: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not value and not allow_empty):
        raise DslAcceptanceError(f"{label} must be a string")
    if len(value.encode("utf-8")) > MAX_ARTICLE_BYTES:
        raise DslAcceptanceError(f"{label} exceeds the text size bound")
    return value


def _hash(value: object, label: str) -> str:
    text = _text(value, label)
    if not HASH_PATTERN.fullmatch(text):
        raise DslAcceptanceError(f"{label} must be a lowercase SHA-256 digest")
    return text


def _relative_path(value: object, label: str) -> str:
    text = _text(value, label)
    path = PurePosixPath(text.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise DslAcceptanceError(f"{label} must be a confined relative path")
    return path.as_posix()


def _exact_keys(value: dict[str, object], expected: set[str], label: str) -> None:
    if set(value) != expected:
        raise DslAcceptanceError(f"{label} fields do not match the contract")


def _positive_integer(value: object, label: str, maximum: int | None = None) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 1:
        raise DslAcceptanceError(f"{label} must be a positive integer")
    if maximum is not None and value > maximum:
        raise DslAcceptanceError(f"{label} exceeds its bound")
    return value


def _confined_file(corpus_root: Path, component: str, label: str) -> Path:
    try:
        path = (corpus_root / Path(component)).resolve(strict=True)
        path.relative_to(corpus_root)
    except (OSError, ValueError) as error:
        raise DslAcceptanceError(f"{label} is outside the bound corpus") from error
    if not path.is_file():
        raise DslAcceptanceError(f"{label} is not a regular file")
    return path


def _validate_archive(
    value: object,
    corpus_root: Path,
    label: str,
    *,
    additional_keys: set[str] | None = None,
) -> tuple[dict[str, object], str]:
    if not isinstance(value, dict):
        raise DslAcceptanceError(f"{label} must be an object")
    archive_keys = {
        "aggregate_uncompressed_size",
        "component",
        "member_count",
        "size",
    }
    _exact_keys(value, archive_keys | (additional_keys or set()), label)
    component = _relative_path(value["component"], f"{label} component")
    member_count = _positive_integer(
        value["member_count"], f"{label} member count", MAX_ARCHIVE_MEMBERS
    )
    size = _positive_integer(value["size"], f"{label} size")
    aggregate = _positive_integer(
        value["aggregate_uncompressed_size"], f"{label} aggregate size"
    )
    archive_path = _confined_file(corpus_root, component, label)
    try:
        if archive_path.stat().st_size != size:
            raise DslAcceptanceError(f"{label} size does not match the corpus")
        with zipfile.ZipFile(archive_path) as archive:
            members = archive.infolist()
            if len(members) != member_count:
                raise DslAcceptanceError(f"{label} member count does not match")
            if sum(member.file_size for member in members) != aggregate:
                raise DslAcceptanceError(f"{label} aggregate size does not match")
    except (OSError, zipfile.BadZipFile) as error:
        raise DslAcceptanceError(f"Cannot validate {label}") from error
    return value, component


def read_catalog(
    path: Path,
    corpus_root: Path,
    *,
    expected_manifest_hash: str | None = None,
    expected_conditions_hash: str | None = None,
) -> tuple[dict[str, object], str]:
    catalog, content = _read_json(path, "DSL catalog")
    _exact_keys(
        catalog,
        {
            "conditions_sha256",
            "dictionaries",
            "manifest_sha256",
            "orphan_archives",
            "schema",
        },
        "DSL catalog",
    )
    if catalog["schema"] != CATALOG_SCHEMA:
        raise DslAcceptanceError(f"DSL catalog schema must be {CATALOG_SCHEMA}")
    manifest_hash = _hash(catalog["manifest_sha256"], "Catalog manifest hash")
    conditions_hash = _hash(catalog["conditions_sha256"], "Catalog conditions hash")
    if expected_manifest_hash is not None and manifest_hash != expected_manifest_hash:
        raise DslAcceptanceError("Catalog manifest hash does not match the pair")
    if (
        expected_conditions_hash is not None
        and conditions_hash != expected_conditions_hash
    ):
        raise DslAcceptanceError("Catalog conditions hash does not match the pair")
    corpus_root = corpus_root.resolve(strict=True)
    dictionaries = catalog["dictionaries"]
    if not isinstance(dictionaries, list) or len(dictionaries) != 5:
        raise DslAcceptanceError("Catalog must contain five DSL dictionaries")
    identifiers: set[str] = set()
    components: set[str] = set()
    for index, value in enumerate(dictionaries):
        if not isinstance(value, dict):
            raise DslAcceptanceError(f"Catalog dictionary {index} must be an object")
        _exact_keys(
            value,
            {"additional_components", "archive", "primary_component", "probe"},
            f"Catalog dictionary {index}",
        )
        primary = _relative_path(
            value["primary_component"], f"Catalog dictionary {index} primary"
        )
        if not primary.lower().endswith((".dsl", ".dsl.dz")):
            raise DslAcceptanceError("Catalog primary must be a DSL source")
        _confined_file(corpus_root, primary, f"Catalog dictionary {index} primary")
        additional = value["additional_components"]
        if not isinstance(additional, list):
            raise DslAcceptanceError("Catalog additional components must be an array")
        normalized_additional = [
            _relative_path(item, f"Catalog dictionary {index} additional component")
            for item in additional
        ]
        if len(set(normalized_additional)) != len(normalized_additional):
            raise DslAcceptanceError("Catalog additional components must be unique")
        for component in normalized_additional:
            _confined_file(
                corpus_root,
                component,
                f"Catalog dictionary {index} additional component",
            )
        _, archive_component = _validate_archive(
            value["archive"], corpus_root, f"Catalog dictionary {index} archive"
        )
        bound_components = {primary, archive_component, *normalized_additional}
        if components.intersection(bound_components):
            raise DslAcceptanceError("Catalog components must be unique")
        components.update(bound_components)
        probe = value["probe"]
        if not isinstance(probe, dict):
            raise DslAcceptanceError(
                f"Catalog dictionary {index} probe must be an object"
            )
        _exact_keys(
            probe, {"id", "query", "resource"}, f"Catalog dictionary {index} probe"
        )
        identifier = _text(probe["id"], f"Catalog dictionary {index} probe id")
        if identifier in identifiers:
            raise DslAcceptanceError("Catalog probe ids must be unique")
        identifiers.add(identifier)
        query = probe["query"]
        resource = probe["resource"]
        if not isinstance(query, dict) or not isinstance(resource, dict):
            raise DslAcceptanceError("Catalog probe payload is invalid")
        _exact_keys(query, {"match_mode", "text"}, "Catalog DSL query")
        if query["match_mode"] != "exact":
            raise DslAcceptanceError("R3.4 DSL probes must use exact lookup")
        _text(query["text"], "Catalog DSL query text")
        _exact_keys(resource, {"id", "sha256", "size"}, "Catalog DSL resource")
        _text(resource["id"], "Catalog DSL resource id")
        _hash(resource["sha256"], "Catalog DSL resource hash")
        _positive_integer(
            resource["size"], "Catalog DSL resource size", MAX_RESOURCE_BYTES
        )
    orphans = catalog["orphan_archives"]
    if not isinstance(orphans, list) or len(orphans) != 1:
        raise DslAcceptanceError("Catalog must bind exactly one orphan archive")
    orphan, orphan_component = _validate_archive(
        orphans[0],
        corpus_root,
        "Catalog orphan archive",
        additional_keys={"resource"},
    )
    if orphan_component in components:
        raise DslAcceptanceError("Orphan archive overlaps an owned component")
    orphan_resource = orphan["resource"]
    if not isinstance(orphan_resource, dict):
        raise DslAcceptanceError("Catalog orphan resource must be an object")
    _exact_keys(orphan_resource, {"id", "sha256", "size"}, "Catalog orphan resource")
    orphan_resource_id = _text(orphan_resource["id"], "Catalog orphan resource id")
    orphan_resource_hash = _hash(
        orphan_resource["sha256"], "Catalog orphan resource hash"
    )
    orphan_resource_size = _positive_integer(
        orphan_resource["size"],
        "Catalog orphan resource size",
        MAX_RESOURCE_BYTES,
    )
    orphan_archive = _confined_file(
        corpus_root, orphan_component, "Catalog orphan archive"
    )
    try:
        with zipfile.ZipFile(orphan_archive) as archive, archive.open(
            orphan_resource_id
        ) as source:
            orphan_resource_bytes = source.read(MAX_RESOURCE_BYTES + 1)
    except (KeyError, OSError, zipfile.BadZipFile) as error:
        raise DslAcceptanceError("Cannot validate catalog orphan resource") from error
    if (
        len(orphan_resource_bytes) != orphan_resource_size
        or _sha256(orphan_resource_bytes) != orphan_resource_hash
    ):
        raise DslAcceptanceError("Catalog orphan resource does not match the corpus")
    return catalog, _sha256(content)


class _VisibleTextParser(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.parts: list[str] = []
        self.suppressed_depth = 0

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        del attrs
        if tag.casefold() in {"script", "style"}:
            self.suppressed_depth += 1

    def handle_endtag(self, tag: str) -> None:
        if tag.casefold() in {"script", "style"} and self.suppressed_depth:
            self.suppressed_depth -= 1

    def handle_data(self, data: str) -> None:
        if not self.suppressed_depth:
            self.parts.append(data)


def _visible_text(markup: str) -> str:
    parser = _VisibleTextParser()
    parser.feed(markup)
    parser.close()
    return " ".join(unicodedata.normalize("NFC", "".join(parser.parts)).split())


def _decode_resource(value: object, label: str) -> bytes:
    encoded = _text(value, label, allow_empty=True)
    if len(encoded) > ((MAX_RESOURCE_BYTES + 2) // 3) * 4 + 4:
        raise DslAcceptanceError(f"{label} exceeds the resource size bound")
    try:
        content = base64.b64decode(encoded, validate=True)
    except (ValueError, binascii.Error) as error:
        raise DslAcceptanceError(f"{label} is not canonical base64") from error
    if (
        len(content) > MAX_RESOURCE_BYTES
        or base64.b64encode(content).decode() != encoded
    ):
        raise DslAcceptanceError(f"{label} is not bounded canonical base64")
    return content


def _component_from_absolute(path_text: object, corpus_root: Path, label: str) -> str:
    try:
        relative = (
            Path(_text(path_text, label)).resolve(strict=True).relative_to(corpus_root)
        )
    except (OSError, ValueError) as error:
        raise DslAcceptanceError(f"{label} is outside the bound corpus") from error
    return PurePosixPath(*relative.parts).as_posix()


def normalize_raw_observation(
    raw: object,
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
    if not isinstance(raw, dict):
        raise DslAcceptanceError("Raw DSL observation must be an object")
    _exact_keys(
        raw,
        {
            "catalog_sha256",
            "conditions_sha256",
            "dictionaries",
            "errors",
            "orphan_archive_owned",
            "scenario",
            "schema",
        },
        "Raw DSL observation",
    )
    if (
        raw["schema"] != RAW_SCHEMA
        or raw["catalog_sha256"] != catalog_hash
        or raw["conditions_sha256"] != conditions_hash
        or raw["scenario"] != scenario
        or raw["errors"] != []
    ):
        raise DslAcceptanceError("Raw DSL observation identity is invalid")
    if not isinstance(raw["orphan_archive_owned"], bool):
        raise DslAcceptanceError("Raw orphan ownership must be Boolean")
    raw_dictionaries = raw["dictionaries"]
    catalog_dictionaries = catalog["dictionaries"]
    assert isinstance(catalog_dictionaries, list)
    if not isinstance(raw_dictionaries, list) or len(raw_dictionaries) != len(
        catalog_dictionaries
    ):
        raise DslAcceptanceError("Raw DSL dictionary count does not match")
    corpus_root = corpus_root.resolve(strict=True)
    normalized: list[dict[str, object]] = []
    for index, (actual, expected) in enumerate(
        zip(raw_dictionaries, catalog_dictionaries, strict=True)
    ):
        if not isinstance(actual, dict) or not isinstance(expected, dict):
            raise DslAcceptanceError(f"Raw DSL dictionary {index} is invalid")
        _exact_keys(
            actual,
            {"archive_owned", "components", "id", "name", "probe"},
            f"Raw DSL dictionary {index}",
        )
        if not isinstance(actual["archive_owned"], bool) or not isinstance(
            actual["components"], list
        ):
            raise DslAcceptanceError("Raw DSL ownership payload is invalid")
        components = [
            _component_from_absolute(item, corpus_root, f"Raw DSL component {index}")
            for item in actual["components"]
        ]
        primary = str(expected["primary_component"])
        archive = expected["archive"]
        assert isinstance(archive, dict)
        expected_archive = str(archive["component"])
        additional_components = expected["additional_components"]
        assert isinstance(additional_components, list)
        expected_component_order = [primary, *additional_components, expected_archive]
        allowed_components = set(expected_component_order)
        if (
            not components
            or components[0] != primary
            or len(set(components)) != len(components)
            or any(component not in allowed_components for component in components)
            or components
            != [
                component
                for component in expected_component_order
                if component in components
            ]
            or bool(actual["archive_owned"]) != (expected_archive in components)
        ):
            raise DslAcceptanceError("Raw DSL components do not match the catalog")
        probe = actual["probe"]
        expected_probe = expected["probe"]
        if not isinstance(probe, dict) or not isinstance(expected_probe, dict):
            raise DslAcceptanceError("Raw DSL probe is invalid")
        _exact_keys(
            probe,
            {
                "article_markup",
                "headword",
                "id",
                "query",
                "resource_available",
                "resource_data_base64",
                "resource_id",
                "resource_media_type",
            },
            f"Raw DSL probe {index}",
        )
        expected_query = expected_probe["query"]
        expected_resource = expected_probe["resource"]
        assert isinstance(expected_query, dict) and isinstance(expected_resource, dict)
        if (
            probe["id"] != expected_probe["id"]
            or probe["query"] != expected_query["text"]
            or probe["resource_id"] != expected_resource["id"]
            or probe["headword"] != expected_query["text"]
        ):
            raise DslAcceptanceError("Raw DSL probe does not match the catalog")
        markup = _text(probe["article_markup"], f"Raw DSL article {index}")
        visible = _visible_text(markup)
        if (
            not visible
            or str(expected_resource["id"]).replace("\\", "/").casefold()
            not in markup.replace("\\", "/").casefold()
        ):
            raise DslAcceptanceError(
                "Raw DSL article is empty or lacks its resource reference"
            )
        available = probe["resource_available"]
        if not isinstance(available, bool):
            raise DslAcceptanceError("Raw DSL resource availability must be Boolean")
        resource_bytes = _decode_resource(
            probe["resource_data_base64"], f"Raw DSL resource {index}"
        )
        if available:
            if (
                len(resource_bytes) != expected_resource["size"]
                or _sha256(resource_bytes) != expected_resource["sha256"]
            ):
                raise DslAcceptanceError(
                    "Raw DSL resource bytes do not match the catalog"
                )
        elif resource_bytes:
            raise DslAcceptanceError("Unavailable DSL resource contains bytes")
        normalized.append(
            {
                "archive_component": expected_archive,
                "archive_owned": actual["archive_owned"],
                "dictionary": {
                    "components": components,
                    "id": _text(actual["id"], "Raw DSL id"),
                    "logical_key": primary,
                    "name": _text(actual["name"], "Raw DSL name"),
                },
                "probe": {
                    "article": {
                        "markup_sha256": _sha256(markup.encode()),
                        "markup_size": len(markup.encode()),
                        "visible_text_sha256": _sha256(visible.encode()),
                        "visible_text_size": len(visible.encode()),
                    },
                    "headword": probe["headword"],
                    "id": probe["id"],
                    "query": probe["query"],
                    "resource": {
                        "available": available,
                        "id": probe["resource_id"],
                        "media_type": _text(
                            probe["resource_media_type"], "Raw DSL media type"
                        ),
                        "sha256": _sha256(resource_bytes) if available else None,
                        "size": len(resource_bytes),
                    },
                },
            }
        )
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": conditions_hash,
        "dictionaries": normalized,
        "manifest_sha256": manifest_hash,
        "orphan_archive_owned": raw["orphan_archive_owned"],
        "pair_id": pair_id,
        "revision": revision,
        "scenario": scenario,
        "schema": OBSERVATION_SCHEMA,
        "version": version,
    }


def _atomic_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=path.parent, prefix=f".{path.name}.", delete=False
        ) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(
                (
                    json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True)
                    + "\n"
                ).encode()
            )
            temporary.flush()
            os.fsync(temporary.fileno())
        temporary_path.replace(path)
    except OSError:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
        raise


def _required_environment(name: str) -> str:
    value = os.environ.get(name, "")
    if not value:
        raise DslAcceptanceError(f"Required environment variable is missing: {name}")
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
    if (
        adapter not in {"qt5", "qt6"}
        or scenario not in SCENARIOS
        or _required_environment("GOLDENDICT_ACCEPTANCE_VERSION") != adapter
    ):
        raise DslAcceptanceError("Unsupported DSL observer adapter or scenario")
    pair_id = _required_environment("GOLDENDICT_ACCEPTANCE_PAIR_ID")
    revision = _required_environment("GOLDENDICT_ACCEPTANCE_REVISION")
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
    except OSError as error:
        raise DslAcceptanceError("Paired DSL path cannot be resolved") from error
    if supplied_root != corpus_root:
        raise DslAcceptanceError("Dictionary root does not match the paired corpus")
    conditions, _ = _read_json(conditions_path, "Acceptance conditions")
    if _sha256(_canonical_json(conditions)) != conditions_hash:
        raise DslAcceptanceError("Acceptance conditions hash does not match")
    catalog, catalog_hash = read_catalog(
        catalog_path,
        corpus_root,
        expected_manifest_hash=manifest_hash,
        expected_conditions_hash=conditions_hash,
    )
    catalog_queries = [{"id": item["probe"]["id"], **item["probe"]["query"]} for item in catalog["dictionaries"]]  # type: ignore[index]
    if conditions.get("queries") != catalog_queries:
        raise DslAcceptanceError("Catalog probes do not match paired queries")
    output = output.resolve(strict=False)
    if (
        output.parent != evidence_root
        or output.suffix.lower() != ".json"
        or output.exists()
    ):
        raise DslAcceptanceError("DSL output must be a new JSON evidence-root child")
    with tempfile.TemporaryDirectory(
        dir=evidence_root, prefix=".raw-observation-dsl-"
    ) as temporary:
        raw_path = Path(temporary) / "raw.json"
        if adapter == "qt6":
            if qt6_observer is None:
                raise DslAcceptanceError("Qt 6 observer executable is required")
            command = [
                str(qt6_observer.resolve(strict=True)),
                "--dictionary-root",
                str(corpus_root),
                "--index-root",
                str(index_root),
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
                raise DslAcceptanceError(
                    "Qt 6 DSL observer did not complete"
                ) from error
            if completed.returncode != 0:
                raise DslAcceptanceError(
                    f"Qt 6 DSL observer failed with exit code {completed.returncode}"
                )
        else:
            if qt5_executable is None or qt5_provenance is None:
                raise DslAcceptanceError("Qt 5 executable and provenance are required")
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
                dsl_catalog=catalog_path,
                dsl_catalog_sha256=catalog_hash,
            )
        raw, _ = _read_json(raw_path, "Raw DSL observation")
        normalized = normalize_raw_observation(
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
        if output.exists() or acknowledgement_path.exists():
            raise DslAcceptanceError("DSL publication target already exists")
        try:
            _atomic_json(output, normalized)
            _atomic_json(acknowledgement_path, acknowledgement)
        except OSError as error:
            output.unlink(missing_ok=True)
            acknowledgement_path.unlink(missing_ok=True)
            raise DslAcceptanceError(
                "DSL observation could not be published"
            ) from error
    return output


def read_pair_contract(path: Path, catalog: dict[str, object]) -> dict[str, object]:
    pair, _ = _read_json(path, "Acceptance pair")
    _exact_keys(
        pair,
        {"conditions", "conditions_sha256", "corpus", "pair_id", "runs", "schema"},
        "Acceptance pair",
    )
    if pair["schema"] != acceptance_workspace.PAIR_SCHEMA:
        raise DslAcceptanceError("Acceptance pair schema is invalid")
    pair_id = _hash(pair["pair_id"], "Acceptance pair id")
    conditions_hash = _hash(
        pair["conditions_sha256"], "Acceptance pair conditions hash"
    )
    if not isinstance(pair["conditions"], dict):
        raise DslAcceptanceError("Acceptance pair conditions are invalid")
    corpus = pair["corpus"]
    runs = pair["runs"]
    if not isinstance(corpus, dict) or not isinstance(runs, dict):
        raise DslAcceptanceError("Acceptance pair payload is invalid")
    _exact_keys(
        corpus,
        {"file_count", "manifest_sha256", "total_bytes"},
        "Acceptance pair corpus",
    )
    manifest_hash = _hash(corpus["manifest_sha256"], "Acceptance pair manifest hash")
    _positive_integer(corpus["file_count"], "Acceptance pair file count")
    _positive_integer(corpus["total_bytes"], "Acceptance pair total bytes")
    if (
        conditions_hash != catalog["conditions_sha256"]
        or manifest_hash != catalog["manifest_sha256"]
    ):
        raise DslAcceptanceError("Acceptance pair does not match the DSL catalog")
    _exact_keys(runs, {"qt5", "qt6"}, "Acceptance pair runs")
    revisions: dict[str, str] = {}
    for version in ("qt5", "qt6"):
        run = runs[version]
        if not isinstance(run, dict):
            raise DslAcceptanceError(f"Acceptance pair {version} run is invalid")
        _exact_keys(run, {"metadata", "revision"}, f"Acceptance pair {version} run")
        if run["metadata"] != f"{version}/run.json":
            raise DslAcceptanceError(f"Acceptance pair {version} metadata is invalid")
        revision = _text(run["revision"], f"Acceptance pair {version} revision")
        if not re.fullmatch(r"[0-9a-f]{40}", revision):
            raise DslAcceptanceError(f"Acceptance pair {version} revision is invalid")
        revisions[version] = revision
    return {
        "conditions_sha256": conditions_hash,
        "manifest_sha256": manifest_hash,
        "pair_id": pair_id,
        "revisions": revisions,
    }


def read_observation(
    path: Path,
    catalog: dict[str, object],
    *,
    catalog_hash: str,
    pair: dict[str, object],
    expected_version: str,
    expected_scenario: str,
) -> dict[str, object]:
    value, _ = _read_json(path, "DSL observation")
    _exact_keys(
        value,
        {
            "catalog_sha256",
            "conditions_sha256",
            "dictionaries",
            "manifest_sha256",
            "orphan_archive_owned",
            "pair_id",
            "revision",
            "scenario",
            "schema",
            "version",
        },
        "DSL observation",
    )
    if (
        value["schema"] != OBSERVATION_SCHEMA
        or value["version"] != expected_version
        or value["scenario"] != expected_scenario
    ):
        raise DslAcceptanceError("DSL observation identity is invalid")
    for name in ("catalog_sha256", "conditions_sha256", "manifest_sha256", "pair_id"):
        _hash(value[name], f"DSL observation {name}")
    if not re.fullmatch(r"[0-9a-f]{40}", _text(value["revision"], "DSL revision")):
        raise DslAcceptanceError("DSL observation revision is invalid")
    revisions = pair["revisions"]
    assert isinstance(revisions, dict)
    if (
        value["catalog_sha256"] != catalog_hash
        or value["conditions_sha256"] != pair["conditions_sha256"]
        or value["manifest_sha256"] != pair["manifest_sha256"]
        or value["pair_id"] != pair["pair_id"]
        or value["revision"] != revisions[expected_version]
    ):
        raise DslAcceptanceError("DSL observation does not match its pair or catalog")
    if (
        not isinstance(value["dictionaries"], list)
        or len(value["dictionaries"]) != 5
        or not isinstance(value["orphan_archive_owned"], bool)
    ):
        raise DslAcceptanceError("DSL observation payload is invalid")
    catalog_dictionaries = catalog["dictionaries"]
    assert isinstance(catalog_dictionaries, list)
    logical_keys: set[str] = set()
    probe_ids: set[str] = set()
    for index, (item, expected) in enumerate(
        zip(value["dictionaries"], catalog_dictionaries, strict=True)
    ):
        if not isinstance(item, dict):
            raise DslAcceptanceError(f"DSL observation dictionary {index} is invalid")
        assert isinstance(expected, dict)
        _exact_keys(
            item,
            {"archive_component", "archive_owned", "dictionary", "probe"},
            f"DSL observation dictionary {index}",
        )
        archive_component = _relative_path(
            item["archive_component"], f"DSL observation archive {index}"
        )
        if not isinstance(item["archive_owned"], bool):
            raise DslAcceptanceError("DSL observation archive ownership is invalid")
        dictionary = item["dictionary"]
        probe = item["probe"]
        if not isinstance(dictionary, dict) or not isinstance(probe, dict):
            raise DslAcceptanceError("DSL observation dictionary payload is invalid")
        _exact_keys(
            dictionary,
            {"components", "id", "logical_key", "name"},
            f"DSL observation dictionary identity {index}",
        )
        components_value = dictionary["components"]
        if not isinstance(components_value, list) or not components_value:
            raise DslAcceptanceError("DSL observation components are invalid")
        components = [
            _relative_path(component, f"DSL observation component {index}")
            for component in components_value
        ]
        logical_key = _relative_path(
            dictionary["logical_key"], f"DSL observation logical key {index}"
        )
        expected_archive = expected["archive"]
        expected_probe = expected["probe"]
        assert isinstance(expected_archive, dict) and isinstance(expected_probe, dict)
        expected_components = [
            str(expected["primary_component"]),
            *[str(component) for component in expected["additional_components"]],
            str(expected_archive["component"]),
        ]
        if (
            len(set(components)) != len(components)
            or components[0] != logical_key
            or logical_key != expected["primary_component"]
            or archive_component != expected_archive["component"]
            or any(component not in expected_components for component in components)
            or components
            != [
                component
                for component in expected_components
                if component in components
            ]
            or bool(item["archive_owned"]) != (archive_component in components)
            or logical_key in logical_keys
        ):
            raise DslAcceptanceError("DSL observation component ownership is invalid")
        logical_keys.add(logical_key)
        _text(dictionary["id"], f"DSL observation dictionary id {index}")
        _text(dictionary["name"], f"DSL observation dictionary name {index}")
        _exact_keys(
            probe,
            {"article", "headword", "id", "query", "resource"},
            f"DSL observation probe {index}",
        )
        probe_id = _text(probe["id"], f"DSL observation probe id {index}")
        expected_query = expected_probe["query"]
        expected_resource = expected_probe["resource"]
        assert isinstance(expected_query, dict) and isinstance(expected_resource, dict)
        if (
            probe_id in probe_ids
            or probe_id != expected_probe["id"]
            or probe["query"] != expected_query["text"]
            or probe["headword"] != expected_query["text"]
        ):
            raise DslAcceptanceError("DSL observation probe ids must be unique")
        probe_ids.add(probe_id)
        _text(probe["headword"], f"DSL observation headword {index}")
        _text(probe["query"], f"DSL observation query {index}")
        article = probe["article"]
        resource = probe["resource"]
        if not isinstance(article, dict) or not isinstance(resource, dict):
            raise DslAcceptanceError("DSL observation probe payload is invalid")
        _exact_keys(
            article,
            {
                "markup_sha256",
                "markup_size",
                "visible_text_sha256",
                "visible_text_size",
            },
            f"DSL observation article {index}",
        )
        _hash(article["markup_sha256"], f"DSL observation markup hash {index}")
        _positive_integer(
            article["markup_size"],
            f"DSL observation markup size {index}",
            MAX_ARTICLE_BYTES,
        )
        _hash(
            article["visible_text_sha256"],
            f"DSL observation visible-text hash {index}",
        )
        _positive_integer(
            article["visible_text_size"],
            f"DSL observation visible-text size {index}",
            MAX_ARTICLE_BYTES,
        )
        _exact_keys(
            resource,
            {"available", "id", "media_type", "sha256", "size"},
            f"DSL observation resource {index}",
        )
        available = resource["available"]
        if not isinstance(available, bool):
            raise DslAcceptanceError("DSL observation resource availability is invalid")
        resource_id = _text(resource["id"], f"DSL observation resource id {index}")
        _text(resource["media_type"], f"DSL observation media type {index}")
        if resource_id != expected_resource["id"]:
            raise DslAcceptanceError(
                "DSL observation resource does not match the catalog"
            )
        if available:
            resource_hash = _hash(
                resource["sha256"], f"DSL observation resource hash {index}"
            )
            resource_size = _positive_integer(
                resource["size"],
                f"DSL observation resource size {index}",
                MAX_RESOURCE_BYTES,
            )
            if (
                resource_hash != expected_resource["sha256"]
                or resource_size != expected_resource["size"]
            ):
                raise DslAcceptanceError(
                    "DSL observation resource does not match the catalog"
                )
        elif resource["sha256"] is not None or resource["size"] != 0:
            raise DslAcceptanceError("Unavailable DSL observation resource is invalid")
    return value


def _comparable(observation: dict[str, object]) -> dict[str, object]:
    dictionaries = observation["dictionaries"]
    assert isinstance(dictionaries, list)
    comparable = []
    for value in dictionaries:
        assert isinstance(value, dict)
        dictionary = value["dictionary"]
        probe = value["probe"]
        assert isinstance(dictionary, dict) and isinstance(probe, dict)
        article = probe["article"]
        assert isinstance(article, dict)
        comparable.append(
            {
                "archive_component": value["archive_component"],
                "archive_owned": value["archive_owned"],
                "dictionary": {
                    "components": dictionary["components"],
                    "logical_key": dictionary["logical_key"],
                    "name": dictionary["name"],
                },
                "probe": {
                    "article": {
                        "visible_text_sha256": article["visible_text_sha256"],
                        "visible_text_size": article["visible_text_size"],
                    },
                    "headword": probe["headword"],
                    "id": probe["id"],
                    "query": probe["query"],
                    "resource": probe["resource"],
                },
            }
        )
    return {
        "dictionaries": comparable,
        "orphan_archive_owned": observation["orphan_archive_owned"],
    }


def _field_differences(
    reference: dict[str, object], candidate: dict[str, object]
) -> list[dict[str, object]]:
    differences: list[dict[str, object]] = []

    def record(
        *,
        dictionary: str | None,
        probe: str | None,
        field: str,
        expected: object,
        actual: object,
    ) -> None:
        if actual != expected:
            differences.append(
                {
                    "actual": actual,
                    "dictionary": dictionary,
                    "expected": expected,
                    "field": field,
                    "probe": probe,
                }
            )

    record(
        dictionary=None,
        probe=None,
        field="orphan_archive_owned",
        expected=reference["orphan_archive_owned"],
        actual=candidate["orphan_archive_owned"],
    )
    reference_dictionaries = reference["dictionaries"]
    candidate_dictionaries = candidate["dictionaries"]
    assert isinstance(reference_dictionaries, list)
    assert isinstance(candidate_dictionaries, list)
    fields = (
        ("archive_component", ("archive_component",)),
        ("archive_owned", ("archive_owned",)),
        ("dictionary.components", ("dictionary", "components")),
        ("dictionary.name", ("dictionary", "name")),
        ("article.visible_text_sha256", ("probe", "article", "visible_text_sha256")),
        ("article.visible_text_size", ("probe", "article", "visible_text_size")),
        ("headword", ("probe", "headword")),
        ("query", ("probe", "query")),
        ("resource.available", ("probe", "resource", "available")),
        ("resource.id", ("probe", "resource", "id")),
        ("resource.media_type", ("probe", "resource", "media_type")),
        ("resource.sha256", ("probe", "resource", "sha256")),
        ("resource.size", ("probe", "resource", "size")),
    )
    for expected_item, actual_item in zip(
        reference_dictionaries, candidate_dictionaries, strict=True
    ):
        assert isinstance(expected_item, dict) and isinstance(actual_item, dict)
        expected_dictionary = expected_item["dictionary"]
        expected_probe = expected_item["probe"]
        assert isinstance(expected_dictionary, dict) and isinstance(
            expected_probe, dict
        )
        logical_key = str(expected_dictionary["logical_key"])
        probe_id = str(expected_probe["id"])
        for field, path in fields:
            expected_value: object = expected_item
            actual_value: object = actual_item
            for key in path:
                assert isinstance(expected_value, dict) and isinstance(
                    actual_value, dict
                )
                expected_value = expected_value[key]
                actual_value = actual_value[key]
            record(
                dictionary=logical_key,
                probe=probe_id,
                field=field,
                expected=expected_value,
                actual=actual_value,
            )
    return differences


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
    expected = {
        "qt5-clean": ("qt5", "clean-discovery"),
        "qt5-warm": ("qt5", "warm-restart"),
        "qt6-clean": ("qt6", "clean-discovery"),
        "qt6-warm": ("qt6", "warm-restart"),
    }
    paths = {
        "qt5-clean": qt5_clean,
        "qt5-warm": qt5_warm,
        "qt6-clean": qt6_clean,
        "qt6-warm": qt6_warm,
    }
    observations = {
        name: read_observation(
            paths[name],
            catalog,
            catalog_hash=catalog_hash,
            pair=pair,
            expected_version=identity[0],
            expected_scenario=identity[1],
        )
        for name, identity in expected.items()
    }
    reference = _comparable(observations["qt5-clean"])
    differences = []
    for name in ("qt5-warm", "qt6-clean", "qt6-warm"):
        candidate = _comparable(observations[name])
        if candidate != reference:
            differences.append(
                {
                    "actual_sha256": _sha256(_canonical_json(candidate)),
                    "expected_sha256": _sha256(_canonical_json(reference)),
                    "fields": _field_differences(reference, candidate),
                    "observation": name,
                }
            )
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": pair["conditions_sha256"],
        "differences": differences,
        "equivalent": not differences,
        "manifest_sha256": pair["manifest_sha256"],
        "pair_id": pair["pair_id"],
        "raw_markup_sha256": {
            name: [
                item["probe"]["article"]["markup_sha256"]
                for item in value["dictionaries"]
            ]
            for name, value in observations.items()
        },
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
    for name in ("qt5-clean", "qt5-warm", "qt6-clean", "qt6-warm"):
        compare_parser.add_argument(f"--{name}", type=Path, required=True)
    compare_parser.add_argument("--catalog", type=Path, required=True)
    compare_parser.add_argument("--dictionary-root", type=Path, required=True)
    compare_parser.add_argument("--pair", type=Path, required=True)
    compare_parser.add_argument("--output", type=Path, required=True)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        if options.command == "observe":
            observe(
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
            return 0
        catalog, catalog_hash = read_catalog(options.catalog, options.dictionary_root)
        pair = read_pair_contract(options.pair, catalog)
        result = compare(
            options.qt5_clean,
            options.qt5_warm,
            options.qt6_clean,
            options.qt6_warm,
            catalog=catalog,
            catalog_hash=catalog_hash,
            pair=pair,
        )
        if options.output.exists():
            raise DslAcceptanceError("DSL comparison output already exists")
        _atomic_json(options.output, result)
        return 0 if result["equivalent"] else 1
    except (DslAcceptanceError, OSError) as error:
        print(f"DSL acceptance failed: {error}", file=os.sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
