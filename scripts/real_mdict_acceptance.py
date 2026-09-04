#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Collect and compare bounded real-MDict lookup/resource evidence."""

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
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

import qt5_real_dictionary_observer
import real_dictionary_acceptance_workspace as acceptance_workspace

CATALOG_SCHEMA = "goldendict-mdict-query-resource-catalog-v1"
RAW_SCHEMA = "goldendict-real-mdict-raw-observation-v1"
OBSERVATION_SCHEMA = "goldendict-real-mdict-observation-v1"
COMPARISON_SCHEMA = "goldendict-real-mdict-comparison-v1"
SCENARIOS = ("clean-discovery", "warm-restart")
MAX_METADATA_BYTES = 16 * 1024 * 1024
MAX_ARTICLE_BYTES = 4 * 1024 * 1024
MAX_RESOURCE_BYTES = 8 * 1024 * 1024
HASH_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class MdictAcceptanceError(RuntimeError):
    """Raised when real-MDict evidence is unsafe or inconsistent."""


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
        raise MdictAcceptanceError(f"Cannot read {label}: {path}") from error
    if len(content) > MAX_METADATA_BYTES:
        raise MdictAcceptanceError(f"{label} exceeds the metadata size bound")
    try:
        value = json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise MdictAcceptanceError(f"{label} is not valid UTF-8 JSON") from error
    if not isinstance(value, dict):
        raise MdictAcceptanceError(f"{label} root must be an object")
    return value, content


def _text(value: object, label: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not value and not allow_empty):
        raise MdictAcceptanceError(f"{label} must be a non-empty string")
    if len(value.encode("utf-8")) > MAX_ARTICLE_BYTES:
        raise MdictAcceptanceError(f"{label} exceeds the text size bound")
    return value


def _hash(value: object, label: str) -> str:
    text = _text(value, label)
    if not HASH_PATTERN.fullmatch(text):
        raise MdictAcceptanceError(f"{label} must be a lowercase SHA-256 digest")
    return text


def _relative_path(value: object, label: str) -> str:
    text = _text(value, label)
    path = PurePosixPath(text.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise MdictAcceptanceError(f"{label} must be a confined relative path")
    return path.as_posix()


def _exact_keys(value: dict[str, object], expected: set[str], label: str) -> None:
    if set(value) != expected:
        raise MdictAcceptanceError(f"{label} fields do not match the contract")


def read_catalog(
    path: Path,
    *,
    expected_manifest_hash: str | None = None,
    expected_conditions_hash: str | None = None,
) -> tuple[dict[str, object], str]:
    catalog, content = _read_json(path, "MDict catalog")
    _exact_keys(
        catalog,
        {
            "conditions_sha256",
            "dictionary",
            "manifest_sha256",
            "probes",
            "schema",
        },
        "MDict catalog",
    )
    if catalog["schema"] != CATALOG_SCHEMA:
        raise MdictAcceptanceError(f"MDict catalog schema must be {CATALOG_SCHEMA}")
    manifest_hash = _hash(catalog["manifest_sha256"], "Catalog manifest hash")
    conditions_hash = _hash(catalog["conditions_sha256"], "Catalog conditions hash")
    if expected_manifest_hash is not None and manifest_hash != expected_manifest_hash:
        raise MdictAcceptanceError("Catalog manifest hash does not match the pair")
    if (
        expected_conditions_hash is not None
        and conditions_hash != expected_conditions_hash
    ):
        raise MdictAcceptanceError("Catalog conditions hash does not match the pair")
    dictionary = catalog["dictionary"]
    if not isinstance(dictionary, dict):
        raise MdictAcceptanceError("Catalog dictionary must be an object")
    _exact_keys(
        dictionary, {"ordered_components", "primary_component"}, "Catalog dictionary"
    )
    components = dictionary["ordered_components"]
    if not isinstance(components, list) or len(components) != 4:
        raise MdictAcceptanceError("Catalog must bind exactly four MDict components")
    normalized_components = [
        _relative_path(value, f"Catalog component {index}")
        for index, value in enumerate(components)
    ]
    if len(set(normalized_components)) != 4:
        raise MdictAcceptanceError("Catalog MDict components must be unique")
    primary = _relative_path(dictionary["primary_component"], "Primary component")
    if normalized_components[0] != primary or not primary.lower().endswith(".mdx"):
        raise MdictAcceptanceError("Primary component must be the first MDX component")
    expected_suffixes = (".mdx", ".mdd", ".1.mdd", ".2.mdd")
    if tuple(
        value.lower().endswith(suffix)
        for value, suffix in zip(normalized_components, expected_suffixes, strict=True)
    ) != (True, True, True, True):
        raise MdictAcceptanceError("Catalog MDict component order is invalid")
    probes = catalog["probes"]
    if not isinstance(probes, list) or len(probes) != 3:
        raise MdictAcceptanceError("Catalog must contain exactly three MDict probes")
    identifiers: set[str] = set()
    resources: set[str] = set()
    source_components: set[str] = set()
    for index, value in enumerate(probes):
        if not isinstance(value, dict):
            raise MdictAcceptanceError(f"Catalog probe {index} must be an object")
        _exact_keys(value, {"id", "query", "resource"}, f"Catalog probe {index}")
        identifier = _text(value["id"], f"Catalog probe {index} id")
        if identifier in identifiers:
            raise MdictAcceptanceError("Catalog probe ids must be unique")
        identifiers.add(identifier)
        query = value["query"]
        if not isinstance(query, dict):
            raise MdictAcceptanceError(f"Catalog probe {index} query must be an object")
        _exact_keys(query, {"match_mode", "text"}, f"Catalog probe {index} query")
        if query["match_mode"] != "exact":
            raise MdictAcceptanceError("R3.3 MDict probes must use exact lookup")
        _text(query["text"], f"Catalog probe {index} query text")
        resource = value["resource"]
        if not isinstance(resource, dict):
            raise MdictAcceptanceError(
                f"Catalog probe {index} resource must be an object"
            )
        _exact_keys(
            resource,
            {"id", "sha256", "size", "source_component"},
            f"Catalog probe {index} resource",
        )
        resource_id = _text(resource["id"], f"Catalog probe {index} resource id")
        if resource_id in resources:
            raise MdictAcceptanceError("Catalog resource ids must be unique")
        resources.add(resource_id)
        _hash(resource["sha256"], f"Catalog probe {index} resource hash")
        if (
            not isinstance(resource["size"], int)
            or isinstance(resource["size"], bool)
            or resource["size"] < 1
            or resource["size"] > MAX_RESOURCE_BYTES
        ):
            raise MdictAcceptanceError("Catalog resource size is outside the bound")
        source = _relative_path(
            resource["source_component"],
            f"Catalog probe {index} source component",
        )
        if source not in normalized_components[1:]:
            raise MdictAcceptanceError(
                "Catalog resource is not bound to an MDD component"
            )
        source_components.add(source)
    if source_components != set(normalized_components[1:]):
        raise MdictAcceptanceError("Catalog must probe every MDD volume exactly once")
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
    try:
        parser.feed(markup)
        parser.close()
    except html.parser.HTMLParseError as error:
        raise MdictAcceptanceError("Article markup could not be parsed") from error
    text = unicodedata.normalize("NFC", "".join(parser.parts))
    return " ".join(text.split())


def _decode_resource(value: object, label: str) -> bytes:
    encoded = _text(value, label)
    if len(encoded) > ((MAX_RESOURCE_BYTES + 2) // 3) * 4 + 4:
        raise MdictAcceptanceError(f"{label} exceeds the resource size bound")
    try:
        content = base64.b64decode(encoded, validate=True)
    except (ValueError, binascii.Error) as error:
        raise MdictAcceptanceError(f"{label} is not canonical base64") from error
    if len(content) > MAX_RESOURCE_BYTES:
        raise MdictAcceptanceError(f"{label} exceeds the resource size bound")
    if base64.b64encode(content).decode("ascii") != encoded:
        raise MdictAcceptanceError(f"{label} is not canonical base64")
    return content


def _component_from_absolute(path_text: object, corpus_root: Path, label: str) -> str:
    path = Path(_text(path_text, label))
    try:
        resolved = path.resolve(strict=True)
        relative = resolved.relative_to(corpus_root)
    except (OSError, ValueError) as error:
        raise MdictAcceptanceError(f"{label} is outside the bound corpus") from error
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
        raise MdictAcceptanceError("Raw observation root must be an object")
    _exact_keys(
        raw,
        {
            "catalog_sha256",
            "conditions_sha256",
            "dictionary",
            "errors",
            "probes",
            "scenario",
            "schema",
        },
        "Raw observation",
    )
    if raw["schema"] != RAW_SCHEMA:
        raise MdictAcceptanceError(f"Raw observation schema must be {RAW_SCHEMA}")
    if raw["catalog_sha256"] != catalog_hash:
        raise MdictAcceptanceError("Raw observation catalog hash does not match")
    if raw["conditions_sha256"] != conditions_hash:
        raise MdictAcceptanceError("Raw observation conditions hash does not match")
    if raw["scenario"] != scenario or scenario not in SCENARIOS:
        raise MdictAcceptanceError("Raw observation scenario does not match")
    if raw["errors"] != []:
        raise MdictAcceptanceError("Raw observation contains product errors")
    dictionary = raw["dictionary"]
    if not isinstance(dictionary, dict):
        raise MdictAcceptanceError("Raw dictionary must be an object")
    _exact_keys(dictionary, {"components", "id", "name"}, "Raw dictionary")
    raw_components = dictionary["components"]
    if not isinstance(raw_components, list):
        raise MdictAcceptanceError("Raw dictionary components must be an array")
    corpus_root = corpus_root.resolve(strict=True)
    components = [
        _component_from_absolute(value, corpus_root, f"Raw component {index}")
        for index, value in enumerate(raw_components)
    ]
    catalog_dictionary = catalog["dictionary"]
    assert isinstance(catalog_dictionary, dict)
    expected_components = [
        str(value) for value in catalog_dictionary["ordered_components"]
    ]
    if components != expected_components:
        raise MdictAcceptanceError(
            "Observed MDict components do not match catalog order"
        )
    raw_probes = raw["probes"]
    catalog_probes = catalog["probes"]
    assert isinstance(catalog_probes, list)
    if not isinstance(raw_probes, list) or len(raw_probes) != len(catalog_probes):
        raise MdictAcceptanceError("Raw probe count does not match the catalog")
    normalized_probes: list[dict[str, object]] = []
    for index, (raw_probe, expected_probe) in enumerate(
        zip(raw_probes, catalog_probes, strict=True)
    ):
        if not isinstance(raw_probe, dict) or not isinstance(expected_probe, dict):
            raise MdictAcceptanceError(f"Raw probe {index} must be an object")
        _exact_keys(
            raw_probe,
            {
                "article_markup",
                "headword",
                "id",
                "query",
                "resource_data_base64",
                "resource_id",
                "resource_media_type",
            },
            f"Raw probe {index}",
        )
        expected_query = expected_probe["query"]
        expected_resource = expected_probe["resource"]
        assert isinstance(expected_query, dict) and isinstance(expected_resource, dict)
        identifier = _text(raw_probe["id"], f"Raw probe {index} id")
        query = _text(raw_probe["query"], f"Raw probe {index} query")
        headword = _text(raw_probe["headword"], f"Raw probe {index} headword")
        resource_id = _text(raw_probe["resource_id"], f"Raw probe {index} resource id")
        if (
            identifier != expected_probe["id"]
            or query != expected_query["text"]
            or resource_id != expected_resource["id"]
        ):
            raise MdictAcceptanceError(f"Raw probe {index} does not match the catalog")
        if headword.casefold() != query.casefold():
            raise MdictAcceptanceError(
                f"Raw probe {index} did not resolve the exact headword"
            )
        markup = _text(raw_probe["article_markup"], f"Raw probe {index} article")
        markup_bytes = markup.encode("utf-8")
        visible_text = _visible_text(markup)
        if not visible_text:
            raise MdictAcceptanceError(f"Raw probe {index} article has no visible text")
        resource_bytes = _decode_resource(
            raw_probe["resource_data_base64"], f"Raw probe {index} resource"
        )
        if (
            len(resource_bytes) != expected_resource["size"]
            or _sha256(resource_bytes) != expected_resource["sha256"]
        ):
            raise MdictAcceptanceError(f"Raw probe {index} resource bytes do not match")
        resource_token = resource_id.replace("\\", "/").casefold()
        markup_token = markup.replace("\\", "/").casefold()
        if resource_token not in markup_token:
            raise MdictAcceptanceError(
                f"Raw probe {index} article does not reference its resource"
            )
        normalized_probes.append(
            {
                "article": {
                    "markup_sha256": _sha256(markup_bytes),
                    "markup_size": len(markup_bytes),
                    "visible_text_sha256": _sha256(visible_text.encode("utf-8")),
                    "visible_text_size": len(visible_text.encode("utf-8")),
                },
                "headword": headword,
                "id": identifier,
                "query": query,
                "resource": {
                    "id": resource_id,
                    "media_type": _text(
                        raw_probe["resource_media_type"],
                        f"Raw probe {index} resource media type",
                    ),
                    "sha256": _sha256(resource_bytes),
                    "size": len(resource_bytes),
                    "source_component": expected_resource["source_component"],
                },
            }
        )
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": conditions_hash,
        "dictionary": {
            "components": components,
            "id": _text(dictionary["id"], "Raw dictionary id"),
            "logical_key": expected_components[0],
            "name": _text(dictionary["name"], "Raw dictionary name"),
        },
        "manifest_sha256": manifest_hash,
        "pair_id": pair_id,
        "probes": normalized_probes,
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
                ).encode("utf-8")
            )
            temporary.flush()
            os.fsync(temporary.fileno())
        temporary_path.replace(path)
    except OSError:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
        raise


def _publish_observation_pair(
    output: Path,
    observation: dict[str, object],
    acknowledgement_path: Path,
    acknowledgement: dict[str, object],
) -> None:
    if output.exists() or acknowledgement_path.exists():
        raise MdictAcceptanceError("MDict publication target already exists")
    try:
        _atomic_json(output, observation)
        _atomic_json(acknowledgement_path, acknowledgement)
    except OSError as error:
        cleanup_failed = False
        for path in (output, acknowledgement_path):
            try:
                path.unlink(missing_ok=True)
            except OSError:
                cleanup_failed = True
        if cleanup_failed:
            raise MdictAcceptanceError(
                "MDict publication failed and incomplete evidence could not be removed"
            ) from error
        raise MdictAcceptanceError(
            "MDict observation and acknowledgement could not be published"
        ) from error


def _required_environment(name: str) -> str:
    value = os.environ.get(name, "")
    if not value:
        raise MdictAcceptanceError(f"Required environment variable is missing: {name}")
    return value


def _bound_output(path: Path, evidence_root: Path) -> Path:
    resolved = path.resolve(strict=False)
    if resolved.parent != evidence_root or resolved.suffix.lower() != ".json":
        raise MdictAcceptanceError(
            "MDict observation must be a JSON evidence-root child"
        )
    return resolved


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
        raise MdictAcceptanceError("Unsupported MDict observer adapter or scenario")
    if _required_environment("GOLDENDICT_ACCEPTANCE_VERSION") != adapter:
        raise MdictAcceptanceError("MDict adapter does not match the paired version")
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
        raise MdictAcceptanceError("Paired MDict path cannot be resolved") from error
    if supplied_root != corpus_root:
        raise MdictAcceptanceError("Dictionary root does not match the paired corpus")
    conditions, _ = _read_json(conditions_path, "Acceptance conditions")
    if _sha256(_canonical_json(conditions)) != conditions_hash:
        raise MdictAcceptanceError("Acceptance conditions hash does not match the pair")
    catalog, catalog_hash = read_catalog(
        catalog_path,
        expected_manifest_hash=manifest_hash,
        expected_conditions_hash=conditions_hash,
    )
    catalog_queries = [
        {"id": item["id"], **item["query"]} for item in catalog["probes"]  # type: ignore[index]
    ]
    if conditions.get("queries") != catalog_queries:
        raise MdictAcceptanceError("Catalog probes do not match paired queries")
    output = _bound_output(output, evidence_root)
    if output.exists():
        raise MdictAcceptanceError("MDict observation output already exists")
    with tempfile.TemporaryDirectory(
        dir=evidence_root, prefix=".raw-observation-mdict-"
    ) as temporary:
        raw_path = Path(temporary) / "raw.json"
        if adapter == "qt6":
            if qt6_observer is None:
                raise MdictAcceptanceError("Qt 6 observer executable is required")
            try:
                executable = qt6_observer.resolve(strict=True)
            except OSError as error:
                raise MdictAcceptanceError(
                    "Qt 6 observer executable is missing"
                ) from error
            command = [
                str(executable),
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
                raise MdictAcceptanceError(
                    "Qt 6 MDict observer did not complete"
                ) from error
            if completed.returncode != 0:
                raise MdictAcceptanceError(
                    f"Qt 6 MDict observer failed with exit code {completed.returncode}"
                )
        else:
            if qt5_executable is None or qt5_provenance is None:
                raise MdictAcceptanceError(
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
                mdict_catalog=catalog_path,
                mdict_catalog_sha256=catalog_hash,
            )
        raw, _ = _read_json(raw_path, "Raw MDict observation")
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
        _publish_observation_pair(
            output,
            normalized,
            acknowledgement_path,
            acknowledgement,
        )
    return output


def read_observation(path: Path) -> dict[str, object]:
    value, _ = _read_json(path, "MDict observation")
    _exact_keys(
        value,
        {
            "catalog_sha256",
            "conditions_sha256",
            "dictionary",
            "manifest_sha256",
            "pair_id",
            "probes",
            "revision",
            "scenario",
            "schema",
            "version",
        },
        "MDict observation",
    )
    if value["schema"] != OBSERVATION_SCHEMA:
        raise MdictAcceptanceError("MDict observation schema is invalid")
    for name in (
        "catalog_sha256",
        "conditions_sha256",
        "manifest_sha256",
        "pair_id",
    ):
        _hash(value[name], f"MDict observation {name}")
    revision = _text(value["revision"], "MDict observation revision")
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise MdictAcceptanceError("MDict observation revision is invalid")
    if value["version"] not in {"qt5", "qt6"} or value["scenario"] not in SCENARIOS:
        raise MdictAcceptanceError("MDict observation version or scenario is invalid")
    dictionary = value["dictionary"]
    if not isinstance(dictionary, dict):
        raise MdictAcceptanceError("MDict observation dictionary must be an object")
    _exact_keys(
        dictionary,
        {"components", "id", "logical_key", "name"},
        "MDict observation dictionary",
    )
    components = dictionary["components"]
    if not isinstance(components, list) or len(components) != 4:
        raise MdictAcceptanceError("MDict observation must contain four components")
    for index, component in enumerate(components):
        _relative_path(component, f"MDict observation component {index}")
    _text(dictionary["id"], "MDict observation dictionary id")
    _text(dictionary["logical_key"], "MDict observation logical key")
    _text(dictionary["name"], "MDict observation dictionary name")
    probes = value["probes"]
    if not isinstance(probes, list) or len(probes) != 3:
        raise MdictAcceptanceError("MDict observation must contain three probes")
    for index, probe in enumerate(probes):
        if not isinstance(probe, dict):
            raise MdictAcceptanceError(f"MDict observation probe {index} is invalid")
        _exact_keys(
            probe,
            {"article", "headword", "id", "query", "resource"},
            f"MDict observation probe {index}",
        )
        for name in ("headword", "id", "query"):
            _text(probe[name], f"MDict observation probe {index} {name}")
        article = probe["article"]
        resource = probe["resource"]
        if not isinstance(article, dict) or not isinstance(resource, dict):
            raise MdictAcceptanceError(
                f"MDict observation probe {index} payload is invalid"
            )
        _exact_keys(
            article,
            {
                "markup_sha256",
                "markup_size",
                "visible_text_sha256",
                "visible_text_size",
            },
            f"MDict observation probe {index} article",
        )
        _exact_keys(
            resource,
            {"id", "media_type", "sha256", "size", "source_component"},
            f"MDict observation probe {index} resource",
        )
        _hash(article["markup_sha256"], "Article markup hash")
        _hash(article["visible_text_sha256"], "Article visible-text hash")
        _hash(resource["sha256"], "Resource hash")
        for item, label, maximum in (
            (article["markup_size"], "Article markup size", MAX_ARTICLE_BYTES),
            (
                article["visible_text_size"],
                "Article visible-text size",
                MAX_ARTICLE_BYTES,
            ),
            (resource["size"], "Resource size", MAX_RESOURCE_BYTES),
        ):
            if (
                not isinstance(item, int)
                or isinstance(item, bool)
                or item < 1
                or item > maximum
            ):
                raise MdictAcceptanceError(f"{label} is outside the bound")
        for name in ("id", "media_type", "source_component"):
            _text(resource[name], f"MDict observation resource {name}")
    return value


def _comparable(observation: dict[str, object]) -> dict[str, object]:
    dictionary = observation.get("dictionary")
    probes = observation.get("probes")
    if not isinstance(dictionary, dict) or not isinstance(probes, list):
        raise MdictAcceptanceError("MDict observation payload is incomplete")
    comparable_probes: list[dict[str, object]] = []
    for probe in probes:
        if not isinstance(probe, dict):
            raise MdictAcceptanceError("MDict observation probe is invalid")
        article = probe.get("article")
        resource = probe.get("resource")
        if not isinstance(article, dict) or not isinstance(resource, dict):
            raise MdictAcceptanceError("MDict observation probe payload is incomplete")
        comparable_probes.append(
            {
                "article": {
                    "visible_text_sha256": article.get("visible_text_sha256"),
                    "visible_text_size": article.get("visible_text_size"),
                },
                "headword": probe.get("headword"),
                "id": probe.get("id"),
                "query": probe.get("query"),
                "resource": resource,
            }
        )
    return {
        "dictionary": {
            "components": dictionary.get("components"),
            "logical_key": dictionary.get("logical_key"),
            "name": dictionary.get("name"),
        },
        "probes": comparable_probes,
    }


def compare(
    qt5_clean: Path,
    qt5_warm: Path,
    qt6_clean: Path,
    qt6_warm: Path,
) -> dict[str, object]:
    observations = {
        "qt5-clean": read_observation(qt5_clean),
        "qt5-warm": read_observation(qt5_warm),
        "qt6-clean": read_observation(qt6_clean),
        "qt6-warm": read_observation(qt6_warm),
    }
    expected = {
        "qt5-clean": ("qt5", "clean-discovery"),
        "qt5-warm": ("qt5", "warm-restart"),
        "qt6-clean": ("qt6", "clean-discovery"),
        "qt6-warm": ("qt6", "warm-restart"),
    }
    identities = {
        (
            value.get("pair_id"),
            value.get("manifest_sha256"),
            value.get("conditions_sha256"),
            value.get("catalog_sha256"),
        )
        for value in observations.values()
    }
    if len(identities) != 1:
        raise MdictAcceptanceError("MDict observations are not from one evidence pair")
    for name, value in observations.items():
        if (value.get("version"), value.get("scenario")) != expected[name]:
            raise MdictAcceptanceError(f"MDict observation identity is invalid: {name}")
    differences: list[dict[str, object]] = []
    reference = _comparable(observations["qt5-clean"])
    for name in ("qt5-warm", "qt6-clean", "qt6-warm"):
        candidate = _comparable(observations[name])
        if candidate != reference:
            differences.append(
                {
                    "actual_sha256": _sha256(_canonical_json(candidate)),
                    "expected_sha256": _sha256(_canonical_json(reference)),
                    "observation": name,
                }
            )
    pair_id, manifest_hash, conditions_hash, catalog_hash = identities.pop()
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": conditions_hash,
        "differences": differences,
        "equivalent": not differences,
        "manifest_sha256": manifest_hash,
        "pair_id": pair_id,
        "raw_markup_sha256": {
            name: [probe["article"]["markup_sha256"] for probe in value["probes"]]  # type: ignore[index]
            for name, value in observations.items()
        },
        "schema": COMPARISON_SCHEMA,
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    observe_parser = subparsers.add_parser("observe")
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
    compare_parser = subparsers.add_parser("compare")
    compare_parser.add_argument("--qt5-clean", type=Path, required=True)
    compare_parser.add_argument("--qt5-warm", type=Path, required=True)
    compare_parser.add_argument("--qt6-clean", type=Path, required=True)
    compare_parser.add_argument("--qt6-warm", type=Path, required=True)
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
            comparison = compare(
                options.qt5_clean,
                options.qt5_warm,
                options.qt6_clean,
                options.qt6_warm,
            )
            _atomic_json(options.output, comparison)
            print(options.output)
            if not comparison["equivalent"]:
                return 1
    except MdictAcceptanceError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
