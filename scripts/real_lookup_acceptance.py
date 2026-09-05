#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Collect and compare bounded real-corpus lookup and article evidence."""

from __future__ import annotations

import argparse
import hashlib
import html.parser
import json
import os
import re
import subprocess
import tempfile
import unicodedata
import urllib.parse
from collections.abc import Iterable
from pathlib import Path, PurePosixPath

import qt5_real_dictionary_observer
import real_dictionary_acceptance_workspace as acceptance_workspace

CATALOG_SCHEMA = "goldendict-real-lookup-catalog-v1"
RAW_SCHEMA = "goldendict-real-lookup-raw-observation-v1"
OBSERVATION_SCHEMA = "goldendict-real-lookup-observation-v1"
COMPARISON_SCHEMA = "goldendict-real-lookup-comparison-v1"
SCENARIOS = ("clean-discovery", "warm-restart")
FORMATS = {"dsl", "mdict"}
OPERATIONS = {"lookup", "suggest"}
CATEGORIES = {
    "alias",
    "exact",
    "media",
    "missing",
    "multi-word",
    "punctuation",
    "suggestion",
    "unicode",
}
REQUIRED_CATEGORIES = CATEGORIES
MAX_METADATA_BYTES = 16 * 1024 * 1024
MAX_ARTICLE_BYTES = 4 * 1024 * 1024
MAX_DICTIONARIES = 32
MAX_PROBES_PER_DICTIONARY = 16
MAX_RESULTS = 32
MAX_DIFFERENCES = 4096
MAX_STRUCTURE_EVENTS = 8192
HASH_PATTERN = re.compile(r"^[0-9a-f]{64}$")
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")

MEDIA_TYPES_BY_EXTENSION = {
    ".aac": "audio/aac",
    ".css": "text/css",
    ".flac": "audio/flac",
    ".gif": "image/gif",
    ".jpeg": "image/jpeg",
    ".jpg": "image/jpeg",
    ".kar": "audio/midi",
    ".m4a": "audio/mp4",
    ".mid": "audio/midi",
    ".mp2": "audio/mpeg",
    ".mp3": "audio/mpeg",
    ".mpa": "audio/mpeg",
    ".oga": "audio/ogg",
    ".ogg": "audio/ogg",
    ".opus": "audio/opus",
    ".png": "image/png",
    ".spx": "audio/ogg",
    ".svg": "image/svg+xml",
    ".wav": "audio/wav",
}
SEMANTIC_CLASS_STYLES = {
    "dsl_b": "font-weight:bold",
    "dsl_i": "font-style:italic",
    "dsl_opt": "semantics:optional",
    "dsl_u": "text-decoration:underline",
    "gd-optional-part": "semantics:optional",
}
REFERENCE_ONLY_CLASSES = {"dsl_ref", "dsl_s_wav"}
PRESENTATION_CLASSES = {"mdict"}


class LookupAcceptanceError(RuntimeError):
    """Raised when lookup acceptance evidence is unsafe or inconsistent."""


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
            raise LookupAcceptanceError(f"{label} exceeds the metadata size bound")
        content = path.read_bytes()
    except OSError as error:
        raise LookupAcceptanceError(f"Cannot read {label}: {path}") from error
    if len(content) > MAX_METADATA_BYTES:
        raise LookupAcceptanceError(f"{label} exceeds the metadata size bound")
    try:
        value = json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise LookupAcceptanceError(f"{label} is not valid UTF-8 JSON") from error
    if not isinstance(value, dict):
        raise LookupAcceptanceError(f"{label} root must be an object")
    return value, content


def _exact_keys(value: dict[str, object], expected: set[str], label: str) -> None:
    if set(value) != expected:
        raise LookupAcceptanceError(f"{label} fields do not match the contract")


def _text(value: object, label: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not value and not allow_empty):
        raise LookupAcceptanceError(f"{label} must be a string")
    try:
        size = len(value.encode("utf-8"))
    except UnicodeEncodeError as error:
        raise LookupAcceptanceError(f"{label} must be valid UTF-8") from error
    if size > MAX_ARTICLE_BYTES:
        raise LookupAcceptanceError(f"{label} exceeds the text size bound")
    return value


def _hash(value: object, label: str) -> str:
    text = _text(value, label)
    if not HASH_PATTERN.fullmatch(text):
        raise LookupAcceptanceError(f"{label} must be a lowercase SHA-256 digest")
    return text


def _positive_integer(value: object, label: str, maximum: int) -> int:
    if (
        not isinstance(value, int)
        or isinstance(value, bool)
        or value < 1
        or value > maximum
    ):
        raise LookupAcceptanceError(f"{label} is outside its bound")
    return value


def _relative_path(value: object, label: str) -> str:
    text = _text(value, label)
    path = PurePosixPath(text.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise LookupAcceptanceError(f"{label} must be a confined relative path")
    return path.as_posix()


def _confined_file(root: Path, relative: str, label: str) -> Path:
    try:
        path = (root / Path(relative)).resolve(strict=True)
        path.relative_to(root)
    except (OSError, ValueError) as error:
        raise LookupAcceptanceError(f"{label} is outside the corpus") from error
    if not path.is_file():
        raise LookupAcceptanceError(f"{label} must be a regular file")
    return path


def read_catalog(
    path: Path,
    corpus_root: Path,
    *,
    expected_manifest_hash: str | None = None,
    expected_conditions_hash: str | None = None,
) -> tuple[dict[str, object], str]:
    catalog, content = _read_json(path, "Lookup catalog")
    _exact_keys(
        catalog,
        {"conditions_sha256", "dictionaries", "manifest_sha256", "schema"},
        "Lookup catalog",
    )
    if catalog["schema"] != CATALOG_SCHEMA:
        raise LookupAcceptanceError(f"Lookup catalog schema must be {CATALOG_SCHEMA}")
    manifest_hash = _hash(catalog["manifest_sha256"], "Catalog manifest hash")
    conditions_hash = _hash(catalog["conditions_sha256"], "Catalog conditions hash")
    if expected_manifest_hash is not None and manifest_hash != expected_manifest_hash:
        raise LookupAcceptanceError("Catalog manifest hash does not match the pair")
    if (
        expected_conditions_hash is not None
        and conditions_hash != expected_conditions_hash
    ):
        raise LookupAcceptanceError("Catalog conditions hash does not match the pair")
    dictionaries = catalog["dictionaries"]
    if (
        not isinstance(dictionaries, list)
        or not dictionaries
        or len(dictionaries) > MAX_DICTIONARIES
    ):
        raise LookupAcceptanceError("Catalog dictionaries are outside the bound")
    dictionary_ids: set[str] = set()
    probe_ids: set[str] = set()
    formats: set[str] = set()
    categories: set[str] = set()
    operations_by_format: dict[str, set[str]] = {item: set() for item in FORMATS}
    for dictionary_index, item in enumerate(dictionaries):
        label = f"Catalog dictionary {dictionary_index}"
        if not isinstance(item, dict):
            raise LookupAcceptanceError(f"{label} must be an object")
        _exact_keys(item, {"format", "id", "primary_component", "probes"}, label)
        dictionary_id = _text(item["id"], f"{label} id")
        if dictionary_id in dictionary_ids:
            raise LookupAcceptanceError("Catalog dictionary ids must be unique")
        dictionary_ids.add(dictionary_id)
        format_name = _text(item["format"], f"{label} format")
        if format_name not in FORMATS:
            raise LookupAcceptanceError(f"{label} format is unsupported")
        formats.add(format_name)
        primary = _relative_path(item["primary_component"], f"{label} component")
        _confined_file(corpus_root, primary, label)
        if (format_name == "dsl") != primary.casefold().endswith((".dsl", ".dsl.dz")):
            raise LookupAcceptanceError(f"{label} component does not match its format")
        if (format_name == "mdict") != primary.casefold().endswith(".mdx"):
            raise LookupAcceptanceError(f"{label} component does not match its format")
        probes = item["probes"]
        if (
            not isinstance(probes, list)
            or not probes
            or len(probes) > MAX_PROBES_PER_DICTIONARY
        ):
            raise LookupAcceptanceError(f"{label} probes are outside the bound")
        for probe_index, probe in enumerate(probes):
            probe_label = f"{label} probe {probe_index}"
            if not isinstance(probe, dict):
                raise LookupAcceptanceError(f"{probe_label} must be an object")
            _exact_keys(
                probe,
                {"category", "id", "operation", "query", "result_limit"},
                probe_label,
            )
            probe_id = _text(probe["id"], f"{probe_label} id")
            if probe_id in probe_ids:
                raise LookupAcceptanceError("Catalog probe ids must be unique")
            probe_ids.add(probe_id)
            operation = _text(probe["operation"], f"{probe_label} operation")
            category = _text(probe["category"], f"{probe_label} category")
            if operation not in OPERATIONS or category not in CATEGORIES:
                raise LookupAcceptanceError(f"{probe_label} classification is invalid")
            if (operation == "suggest") != (category == "suggestion"):
                raise LookupAcceptanceError(f"{probe_label} operation is inconsistent")
            _text(probe["query"], f"{probe_label} query")
            _positive_integer(
                probe["result_limit"], f"{probe_label} result limit", MAX_RESULTS
            )
            categories.add(category)
            operations_by_format[format_name].add(operation)
    if formats != FORMATS or categories != REQUIRED_CATEGORIES:
        raise LookupAcceptanceError(
            "Catalog does not cover the required families/cases"
        )
    if any(value != OPERATIONS for value in operations_by_format.values()):
        raise LookupAcceptanceError(
            "Each real dictionary family needs lookup and suggestion"
        )
    return catalog, _sha256(content)


def catalog_queries(catalog: dict[str, object]) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    dictionaries = catalog["dictionaries"]
    assert isinstance(dictionaries, list)
    for item in dictionaries:
        assert isinstance(item, dict)
        probes = item["probes"]
        assert isinstance(probes, list)
        for probe in probes:
            assert isinstance(probe, dict)
            result.append(
                {
                    "dictionary": item["id"],
                    "id": probe["id"],
                    "operation": probe["operation"],
                    "text": probe["query"],
                }
            )
    return result


def _normalize_reference(value: str) -> tuple[str, str] | None:
    parsed = urllib.parse.urlsplit(value)
    scheme = parsed.scheme.casefold()
    decoded_path = urllib.parse.unquote(parsed.path).lstrip("/")
    if scheme in {"bword", "gdlookup"}:
        target = urllib.parse.unquote(parsed.path or parsed.netloc).lstrip("/")
        return "link", unicodedata.normalize("NFC", target)
    if scheme == "goldendict" and parsed.netloc.casefold() == "lookup":
        return "link", unicodedata.normalize("NFC", decoded_path)
    if scheme in {"bres", "gdau"}:
        return "resource", unicodedata.normalize("NFC", decoded_path)
    if scheme == "goldendict" and parsed.netloc.casefold() == "resource":
        parts = decoded_path.split("/", 1)
        return "resource", unicodedata.normalize("NFC", parts[-1])
    if scheme in {"http", "https", "mailto"}:
        return "external", value
    return None


def _media_type_for_resource(resource_id: str) -> str:
    extension = PurePosixPath(resource_id).suffix.casefold()
    return MEDIA_TYPES_BY_EXTENSION.get(extension, "application/octet-stream")


def _collapse_html_whitespace(value: str) -> str:
    normalized = unicodedata.normalize("NFC", value)
    return re.sub(r"[\t\n\f\r ]+", " ", normalized).strip(" \t\n\f\r")


class _ArticleParser(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.text: list[str] = []
        self.links: set[str] = set()
        self.resources: set[str] = set()
        self.external_links: set[str] = set()
        self.styles: set[str] = set()
        self.reference_sequence: list[tuple[str, str]] = []
        self.content_events: list[
            tuple[str, tuple[str, ...], tuple[str, str] | None, list[str]]
        ] = []
        self.headwords: list[list[str]] = []
        self.stack: list[tuple[str, tuple[str, ...], bool, int | None, bool]] = []
        self.active_style_counts: dict[str, int] = {}
        self.active_references: list[tuple[str, str]] = []
        self.suppressed_depth = 0

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        tag = tag.casefold()
        suppressed_tag = tag in {"script", "style"}
        already_suppressed = self.suppressed_depth > 0
        if suppressed_tag:
            self.suppressed_depth += 1
        added_styles: set[str] = set()
        reference: tuple[str, str] | None = None
        starts_headword = False
        if not already_suppressed and not suppressed_tag:
            added_styles.update(
                {
                    "b": {"font-weight:bold"},
                    "strong": {"font-weight:bold"},
                    "i": {"font-style:italic"},
                    "em": {"font-style:italic"},
                    "u": {"text-decoration:underline"},
                    "sub": {"vertical-align:sub"},
                    "sup": {"vertical-align:super"},
                }.get(tag, set())
            )
        for name, value in attrs:
            if value is None or already_suppressed:
                continue
            name = name.casefold()
            if name in {"href", "src"}:
                normalized_reference = _normalize_reference(value)
                if normalized_reference is not None:
                    collection = {
                        "external": self.external_links,
                        "link": self.links,
                        "resource": self.resources,
                    }[normalized_reference[0]]
                    collection.add(normalized_reference[1])
                    self.reference_sequence.append(normalized_reference)
                    self.content_events.append(
                        ("reference", (), normalized_reference, [])
                    )
                    if name == "href":
                        reference = normalized_reference
            elif not suppressed_tag and name == "class":
                for token in value.split():
                    canonical_token = token.casefold()
                    starts_headword = (
                        starts_headword or canonical_token == "dsl_headwords"
                    )
                    if canonical_token in SEMANTIC_CLASS_STYLES:
                        added_styles.add(SEMANTIC_CLASS_STYLES[canonical_token])
                    elif canonical_token in PRESENTATION_CLASSES or (
                        canonical_token not in REFERENCE_ONLY_CLASSES
                        and canonical_token.startswith(("dsl_", "mdict_"))
                    ):
                        added_styles.add(canonical_token)
            elif not suppressed_tag and name == "style":
                for declaration in value.split(";"):
                    property_name, separator, property_value = declaration.partition(
                        ":"
                    )
                    property_name = property_name.strip().casefold()
                    if separator and property_name in {
                        "color",
                        "font-family",
                        "font-size",
                        "font-style",
                        "font-weight",
                        "text-decoration",
                    }:
                        added_styles.add(
                            f"{property_name}:{_collapse_html_whitespace(property_value)}"
                        )
        self.styles.update(added_styles)
        for style in added_styles:
            self.active_style_counts[style] = self.active_style_counts.get(style, 0) + 1
        if reference is not None:
            self.active_references.append(reference)
        headword_index: int | None = None
        if starts_headword:
            headword_index = len(self.headwords)
            self.headwords.append([])
        if tag not in {
            "area",
            "base",
            "br",
            "col",
            "embed",
            "hr",
            "img",
            "input",
            "link",
            "meta",
            "param",
            "source",
            "track",
            "wbr",
        }:
            self.stack.append(
                (
                    tag,
                    tuple(sorted(added_styles)),
                    reference is not None,
                    headword_index,
                    suppressed_tag,
                )
            )
        else:
            self._remove_styles(added_styles)
            if reference is not None:
                self.active_references.pop()

    def _remove_styles(self, styles: Iterable[str]) -> None:
        for style in styles:
            count = self.active_style_counts.get(style, 0)
            if count <= 1:
                self.active_style_counts.pop(style, None)
            else:
                self.active_style_counts[style] = count - 1

    def _pop_frame(self) -> None:
        _, styles, has_reference, _, suppressed_tag = self.stack.pop()
        self._remove_styles(styles)
        if has_reference and self.active_references:
            self.active_references.pop()
        if suppressed_tag and self.suppressed_depth:
            self.suppressed_depth -= 1

    def handle_endtag(self, tag: str) -> None:
        tag = tag.casefold()
        matching = next(
            (
                index
                for index in range(len(self.stack) - 1, -1, -1)
                if self.stack[index][0] == tag
            ),
            None,
        )
        if matching is None:
            return
        while len(self.stack) > matching:
            self._pop_frame()

    def handle_data(self, data: str) -> None:
        if not self.suppressed_depth:
            self.text.append(data)
            styles = tuple(sorted(self.active_style_counts))
            reference = self.active_references[-1] if self.active_references else None
            if (
                self.content_events
                and self.content_events[-1][0] == "text"
                and self.content_events[-1][1:3] == (styles, reference)
            ):
                self.content_events[-1][3].append(data)
            else:
                self.content_events.append(("text", styles, reference, [data]))
            for _, _, _, headword_index, _ in self.stack:
                if headword_index is not None:
                    self.headwords[headword_index].append(data)


def _exact_text_signature(value: str) -> dict[str, object]:
    normalized = unicodedata.normalize("NFC", value)
    encoded = normalized.encode("utf-8")
    return {"sha256": _sha256(encoded), "size": len(encoded)}


def _semantic_text_signature(value: str) -> dict[str, object]:
    normalized = _collapse_html_whitespace(value)
    encoded = normalized.encode("utf-8")
    return {"sha256": _sha256(encoded), "size": len(encoded)}


def _exact_text_signatures(values: Iterable[str]) -> list[dict[str, object]]:
    signatures = [_exact_text_signature(value) for value in values]
    return sorted(signatures, key=lambda item: (item["sha256"], item["size"]))


def _reference_evidence(reference: tuple[str, str]) -> dict[str, object]:
    kind, target = reference
    return {
        "kind": kind,
        "target": target if kind == "resource" else _exact_text_signature(target),
    }


def _article(
    markup: str, structured_resources: list[object], reported_headword: str = ""
) -> dict[str, object]:
    parser = _ArticleParser()
    parser.feed(markup)
    parser.close()
    media_types: dict[str, str] = {}
    for index, value in enumerate(structured_resources):
        if not isinstance(value, dict):
            raise LookupAcceptanceError(f"Raw resource {index} must be an object")
        _exact_keys(value, {"id", "media_type"}, f"Raw resource {index}")
        resource_id = _text(value["id"], f"Raw resource {index} id")
        media_types[resource_id] = _text(
            value["media_type"], f"Raw resource {index} media type"
        )
        parser.resources.add(resource_id)
    for resource_id in parser.resources:
        media_types.setdefault(resource_id, _media_type_for_resource(resource_id))
    visible = _collapse_html_whitespace("".join(parser.text))
    encoded = visible.encode("utf-8")
    displayed_headwords = [
        _semantic_text_signature("".join(value))
        for value in parser.headwords
        if _collapse_html_whitespace("".join(value))
    ]
    source_headwords = (
        [_exact_text_signature(reported_headword)] if reported_headword else []
    )
    styled_text_runs: list[dict[str, object]] = []
    content_sequence: list[dict[str, object]] = []
    for event_type, styles, reference, chunks in parser.content_events:
        if event_type == "reference":
            assert reference is not None
            content_sequence.append(
                {
                    "reference": _reference_evidence(reference),
                    "type": "reference",
                }
            )
            continue
        signature = _semantic_text_signature("".join(chunks))
        if signature["size"] == 0:
            continue
        run = {
            "reference": (
                _reference_evidence(reference) if reference is not None else None
            ),
            "styles": list(styles),
            "text_sha256": signature["sha256"],
            "text_size": signature["size"],
        }
        styled_text_runs.append(run)
        content_sequence.append({"type": "text", **run})
    if (
        len(displayed_headwords) > MAX_RESULTS
        or len(source_headwords) > MAX_RESULTS
        or len(parser.reference_sequence) > MAX_STRUCTURE_EVENTS
        or len(styled_text_runs) > MAX_STRUCTURE_EVENTS
        or len(content_sequence) > MAX_STRUCTURE_EVENTS
    ):
        raise LookupAcceptanceError("Article structure exceeds the evidence bound")
    return {
        "content_sequence": content_sequence,
        "displayed_headwords": displayed_headwords,
        "external_links": _exact_text_signatures(parser.external_links),
        "links": _exact_text_signatures(parser.links),
        "media_types": dict(sorted(media_types.items())),
        "reference_sequence": [
            _reference_evidence(reference) for reference in parser.reference_sequence
        ],
        "resources": sorted(parser.resources),
        "source_headwords": source_headwords,
        "styled_text_runs": styled_text_runs,
        "styles": sorted(parser.styles),
        "visible_text_sha256": _sha256(encoded),
        "visible_text_size": len(encoded),
    }


def _component_from_absolute(value: object, corpus_root: Path, label: str) -> str:
    try:
        path = Path(_text(value, label)).resolve(strict=True)
        relative = path.relative_to(corpus_root)
    except (OSError, ValueError) as error:
        raise LookupAcceptanceError(f"{label} is outside the corpus") from error
    return PurePosixPath(*relative.parts).as_posix()


def normalize_raw_observation(
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
            "catalog_sha256",
            "conditions_sha256",
            "dictionaries",
            "errors",
            "scenario",
            "schema",
        },
        "Raw lookup observation",
    )
    if (
        raw["schema"] != RAW_SCHEMA
        or raw["catalog_sha256"] != catalog_hash
        or raw["conditions_sha256"] != conditions_hash
        or raw["scenario"] != scenario
        or not isinstance(raw["errors"], list)
        or raw["errors"]
    ):
        raise LookupAcceptanceError("Raw lookup observation identity is invalid")
    raw_dictionaries = raw["dictionaries"]
    catalog_dictionaries = catalog["dictionaries"]
    if not isinstance(raw_dictionaries, list) or not isinstance(
        catalog_dictionaries, list
    ):
        raise LookupAcceptanceError("Raw lookup dictionaries are invalid")
    if len(raw_dictionaries) != len(catalog_dictionaries):
        raise LookupAcceptanceError("Raw lookup dictionary count is invalid")
    dictionaries: list[dict[str, object]] = []
    for index, (raw_item, expected) in enumerate(
        zip(raw_dictionaries, catalog_dictionaries, strict=True)
    ):
        label = f"Raw dictionary {index}"
        if not isinstance(raw_item, dict) or not isinstance(expected, dict):
            raise LookupAcceptanceError(f"{label} is invalid")
        _exact_keys(
            raw_item, {"catalog_id", "components", "id", "name", "probes"}, label
        )
        if raw_item["catalog_id"] != expected["id"]:
            raise LookupAcceptanceError(f"{label} does not match the catalog")
        components_value = raw_item["components"]
        if not isinstance(components_value, list) or not components_value:
            raise LookupAcceptanceError(f"{label} components are invalid")
        components = [
            _component_from_absolute(value, corpus_root, f"{label} component")
            for value in components_value
        ]
        if components[0] != expected["primary_component"]:
            raise LookupAcceptanceError(f"{label} primary component is invalid")
        raw_probes = raw_item["probes"]
        expected_probes = expected["probes"]
        if not isinstance(raw_probes, list) or not isinstance(expected_probes, list):
            raise LookupAcceptanceError(f"{label} probes are invalid")
        if len(raw_probes) != len(expected_probes):
            raise LookupAcceptanceError(f"{label} probe count is invalid")
        probes: list[dict[str, object]] = []
        for probe_index, (raw_probe, expected_probe) in enumerate(
            zip(raw_probes, expected_probes, strict=True)
        ):
            probe_label = f"{label} probe {probe_index}"
            if not isinstance(raw_probe, dict) or not isinstance(expected_probe, dict):
                raise LookupAcceptanceError(f"{probe_label} is invalid")
            _exact_keys(
                raw_probe,
                {"entries", "errors", "id", "operation", "suggestions"},
                probe_label,
            )
            if (
                raw_probe["id"] != expected_probe["id"]
                or raw_probe["operation"] != expected_probe["operation"]
            ):
                raise LookupAcceptanceError(f"{probe_label} does not match the catalog")
            entries_value = raw_probe["entries"]
            suggestions_value = raw_probe["suggestions"]
            errors_value = raw_probe["errors"]
            if (
                not isinstance(entries_value, list)
                or len(entries_value) > expected_probe["result_limit"]
                or not isinstance(suggestions_value, list)
                or len(suggestions_value) > expected_probe["result_limit"]
                or not isinstance(errors_value, list)
                or len(errors_value) > MAX_RESULTS
            ):
                raise LookupAcceptanceError(f"{probe_label} result is outside bounds")
            entries: list[dict[str, object]] = []
            for entry_index, entry in enumerate(entries_value):
                if not isinstance(entry, dict):
                    raise LookupAcceptanceError(f"{probe_label} entry is invalid")
                _exact_keys(
                    entry,
                    {"article_markup", "headword", "plain_text", "resources"},
                    f"{probe_label} entry {entry_index}",
                )
                markup = _text(
                    entry["article_markup"],
                    f"{probe_label} entry {entry_index} markup",
                    allow_empty=True,
                )
                headword = _text(
                    entry["headword"],
                    f"{probe_label} entry headword",
                )
                _text(
                    entry["plain_text"],
                    f"{probe_label} entry plain text",
                    allow_empty=True,
                )
                resources = entry["resources"]
                if not isinstance(resources, list) or len(resources) > 256:
                    raise LookupAcceptanceError(f"{probe_label} resources are invalid")
                entries.append(_article(markup, resources, headword))
            suggestions = [
                _exact_text_signature(_text(value, f"{probe_label} suggestion"))
                for value in suggestions_value
            ]
            probes.append(
                {
                    "category": expected_probe["category"],
                    "entries": entries,
                    "error_count": len(errors_value),
                    "id": expected_probe["id"],
                    "operation": expected_probe["operation"],
                    "query": expected_probe["query"],
                    "suggestions": suggestions,
                }
            )
        dictionaries.append(
            {
                "components": components,
                "format": expected["format"],
                "logical_key": expected["primary_component"],
                "name": _text(raw_item["name"], f"{label} name"),
                "probes": probes,
            }
        )
    return {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": conditions_hash,
        "dictionaries": dictionaries,
        "manifest_sha256": manifest_hash,
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
        raise LookupAcceptanceError(f"Required environment variable is missing: {name}")
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
        raise LookupAcceptanceError("Unsupported lookup adapter or scenario")
    if _required_environment("GOLDENDICT_ACCEPTANCE_VERSION") != adapter:
        raise LookupAcceptanceError("Lookup adapter does not match the paired run")
    pair_id = _hash(_required_environment("GOLDENDICT_ACCEPTANCE_PAIR_ID"), "Pair id")
    revision = _required_environment("GOLDENDICT_ACCEPTANCE_REVISION")
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise LookupAcceptanceError("Pair revision is invalid")
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
        raise LookupAcceptanceError("Paired lookup path cannot be resolved") from error
    if supplied_root != corpus_root:
        raise LookupAcceptanceError("Dictionary root does not match the paired corpus")
    conditions, conditions_content = _read_json(
        conditions_path, "Acceptance conditions"
    )
    if (
        _sha256(_canonical_json(conditions)) != conditions_hash
        and _sha256(conditions_content) != conditions_hash
    ):
        raise LookupAcceptanceError("Acceptance conditions hash does not match")
    catalog, catalog_hash = read_catalog(
        catalog_path,
        corpus_root,
        expected_manifest_hash=manifest_hash,
        expected_conditions_hash=conditions_hash,
    )
    if conditions.get("queries") != catalog_queries(catalog):
        raise LookupAcceptanceError("Catalog probes do not match paired queries")
    output = output.resolve(strict=False)
    if (
        output.parent != evidence_root
        or output.suffix.lower() != ".json"
        or output.exists()
    ):
        raise LookupAcceptanceError(
            "Lookup output must be a new JSON evidence-root child"
        )
    with tempfile.TemporaryDirectory(
        dir=evidence_root, prefix=".raw-observation-lookup-"
    ) as temporary:
        raw_path = Path(temporary) / "raw.json"
        if adapter == "qt6":
            if qt6_observer is None:
                raise LookupAcceptanceError("Qt 6 lookup observer is required")
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
                raise LookupAcceptanceError(
                    "Qt 6 lookup observer did not complete"
                ) from error
            if completed.returncode != 0:
                raise LookupAcceptanceError(
                    f"Qt 6 lookup observer failed with exit code {completed.returncode}"
                )
        else:
            if qt5_executable is None or qt5_provenance is None:
                raise LookupAcceptanceError(
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
                lookup_catalog=catalog_path,
                lookup_catalog_sha256=catalog_hash,
            )
        raw, _ = _read_json(raw_path, "Raw lookup observation")
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
        if acknowledgement_path.exists():
            raise LookupAcceptanceError("Lookup acknowledgement already exists")
        try:
            _atomic_json(output, normalized)
            _atomic_json(acknowledgement_path, acknowledgement)
        except OSError as error:
            output.unlink(missing_ok=True)
            acknowledgement_path.unlink(missing_ok=True)
            raise LookupAcceptanceError(
                "Lookup observation could not be published"
            ) from error
    return output


def _pair_contract(path: Path, catalog: dict[str, object]) -> dict[str, object]:
    pair, _ = _read_json(path, "Acceptance pair")
    _exact_keys(
        pair,
        {"conditions", "conditions_sha256", "corpus", "pair_id", "runs", "schema"},
        "Acceptance pair",
    )
    if pair.get("schema") != acceptance_workspace.PAIR_SCHEMA:
        raise LookupAcceptanceError("Acceptance pair schema is invalid")
    corpus = pair.get("corpus")
    runs = pair.get("runs")
    if not isinstance(corpus, dict) or not isinstance(runs, dict):
        raise LookupAcceptanceError("Acceptance pair payload is invalid")
    _exact_keys(
        corpus,
        {"file_count", "manifest_sha256", "total_bytes"},
        "Acceptance pair corpus",
    )
    if set(runs) != {"qt5", "qt6"}:
        raise LookupAcceptanceError("Acceptance pair runs are invalid")
    result = {
        "conditions_sha256": _hash(
            pair.get("conditions_sha256"), "Pair conditions hash"
        ),
        "manifest_sha256": _hash(corpus.get("manifest_sha256"), "Pair manifest hash"),
        "pair_id": _hash(pair.get("pair_id"), "Pair id"),
        "revisions": {},
    }
    if (
        result["conditions_sha256"] != catalog["conditions_sha256"]
        or result["manifest_sha256"] != catalog["manifest_sha256"]
    ):
        raise LookupAcceptanceError("Acceptance pair does not match the catalog")
    revisions = result["revisions"]
    assert isinstance(revisions, dict)
    for version in ("qt5", "qt6"):
        run = runs.get(version)
        if not isinstance(run, dict):
            raise LookupAcceptanceError(f"Acceptance pair {version} run is invalid")
        _exact_keys(run, {"metadata", "revision"}, f"Acceptance pair {version} run")
        revision = _text(run.get("revision"), f"Acceptance pair {version} revision")
        if not REVISION_PATTERN.fullmatch(revision):
            raise LookupAcceptanceError(
                f"Acceptance pair {version} revision is invalid"
            )
        revisions[version] = revision
    return result


def _nonnegative_integer(value: object, label: str, maximum: int) -> int:
    if (
        not isinstance(value, int)
        or isinstance(value, bool)
        or value < 0
        or value > maximum
    ):
        raise LookupAcceptanceError(f"{label} is outside its bound")
    return value


def _sorted_unique_texts(value: object, label: str, *, maximum: int = 256) -> list[str]:
    if not isinstance(value, list) or len(value) > maximum:
        raise LookupAcceptanceError(f"{label} is outside its bound")
    result = [_text(item, f"{label} item") for item in value]
    if result != sorted(set(result)):
        raise LookupAcceptanceError(f"{label} must be sorted and unique")
    return result


def _validate_text_signature(value: object, label: str) -> None:
    if not isinstance(value, dict):
        raise LookupAcceptanceError(f"{label} must be an object")
    _exact_keys(value, {"sha256", "size"}, label)
    _hash(value["sha256"], f"{label} hash")
    if _nonnegative_integer(value["size"], f"{label} size", MAX_ARTICLE_BYTES) == 0:
        raise LookupAcceptanceError(f"{label} must not be empty")


def _validate_text_signature_list(
    value: object, label: str, *, maximum: int, canonical: bool = False
) -> None:
    if not isinstance(value, list) or len(value) > maximum:
        raise LookupAcceptanceError(f"{label} are outside the bound")
    keys: list[tuple[str, int]] = []
    for index, signature in enumerate(value):
        _validate_text_signature(signature, f"{label} {index}")
        assert isinstance(signature, dict)
        keys.append((signature["sha256"], signature["size"]))
    if canonical and keys != sorted(set(keys)):
        raise LookupAcceptanceError(f"{label} must be sorted and unique")


def _validate_reference(value: object, label: str) -> None:
    if not isinstance(value, dict):
        raise LookupAcceptanceError(f"{label} must be an object")
    _exact_keys(value, {"kind", "target"}, label)
    if value["kind"] not in {"external", "link", "resource"}:
        raise LookupAcceptanceError(f"{label} kind is invalid")
    if value["kind"] == "resource":
        _text(value["target"], f"{label} target")
    else:
        _validate_text_signature(value["target"], f"{label} target")


def _validate_article(value: object, label: str) -> None:
    if not isinstance(value, dict):
        raise LookupAcceptanceError(f"{label} must be an object")
    _exact_keys(
        value,
        {
            "content_sequence",
            "displayed_headwords",
            "external_links",
            "links",
            "media_types",
            "reference_sequence",
            "resources",
            "source_headwords",
            "styled_text_runs",
            "styles",
            "visible_text_sha256",
            "visible_text_size",
        },
        label,
    )
    resources = _sorted_unique_texts(value["resources"], f"{label} resources")
    _validate_text_signature_list(
        value["links"], f"{label} links", maximum=256, canonical=True
    )
    _validate_text_signature_list(
        value["external_links"],
        f"{label} external links",
        maximum=256,
        canonical=True,
    )
    _sorted_unique_texts(value["styles"], f"{label} styles")
    _hash(value["visible_text_sha256"], f"{label} visible text hash")
    _nonnegative_integer(
        value["visible_text_size"], f"{label} visible text size", MAX_ARTICLE_BYTES
    )
    for field in ("displayed_headwords", "source_headwords"):
        headwords = value[field]
        if not isinstance(headwords, list) or len(headwords) > MAX_RESULTS:
            raise LookupAcceptanceError(f"{label} {field} are outside the bound")
        if field == "source_headwords" and len(headwords) != 1:
            raise LookupAcceptanceError(f"{label} must have one source headword")
        for index, headword in enumerate(headwords):
            _validate_text_signature(headword, f"{label} {field} headword {index}")
    references = value["reference_sequence"]
    if not isinstance(references, list) or len(references) > MAX_STRUCTURE_EVENTS:
        raise LookupAcceptanceError(f"{label} reference sequence is outside the bound")
    for index, reference in enumerate(references):
        _validate_reference(reference, f"{label} reference {index}")
    runs = value["styled_text_runs"]
    if not isinstance(runs, list) or len(runs) > MAX_STRUCTURE_EVENTS:
        raise LookupAcceptanceError(f"{label} styled text runs are outside the bound")
    total_run_size = 0
    for index, run in enumerate(runs):
        run_label = f"{label} styled text run {index}"
        if not isinstance(run, dict):
            raise LookupAcceptanceError(f"{run_label} must be an object")
        _exact_keys(
            run,
            {"reference", "styles", "text_sha256", "text_size"},
            run_label,
        )
        _sorted_unique_texts(run["styles"], f"{run_label} styles")
        _hash(run["text_sha256"], f"{run_label} text hash")
        text_size = _nonnegative_integer(
            run["text_size"], f"{run_label} text size", MAX_ARTICLE_BYTES
        )
        if text_size == 0:
            raise LookupAcceptanceError(f"{run_label} must not be empty")
        total_run_size += text_size
        reference = run["reference"]
        if reference is not None:
            _validate_reference(reference, f"{run_label} reference")
    if total_run_size > MAX_ARTICLE_BYTES:
        raise LookupAcceptanceError(f"{label} styled text exceeds the article bound")
    content = value["content_sequence"]
    if not isinstance(content, list) or len(content) > MAX_STRUCTURE_EVENTS:
        raise LookupAcceptanceError(f"{label} content sequence is outside the bound")
    total_content_size = 0
    for index, event in enumerate(content):
        event_label = f"{label} content event {index}"
        if not isinstance(event, dict):
            raise LookupAcceptanceError(f"{event_label} must be an object")
        if event.get("type") == "reference":
            _exact_keys(event, {"reference", "type"}, event_label)
            _validate_reference(event["reference"], f"{event_label} reference")
        elif event.get("type") == "text":
            _exact_keys(
                event,
                {"reference", "styles", "text_sha256", "text_size", "type"},
                event_label,
            )
            _sorted_unique_texts(event["styles"], f"{event_label} styles")
            _hash(event["text_sha256"], f"{event_label} text hash")
            text_size = _nonnegative_integer(
                event["text_size"], f"{event_label} text size", MAX_ARTICLE_BYTES
            )
            if text_size == 0:
                raise LookupAcceptanceError(f"{event_label} must not be empty")
            total_content_size += text_size
            if event["reference"] is not None:
                _validate_reference(event["reference"], f"{event_label} reference")
        else:
            raise LookupAcceptanceError(f"{event_label} type is invalid")
    if total_content_size > MAX_ARTICLE_BYTES:
        raise LookupAcceptanceError(f"{label} content exceeds the article bound")
    media_types = value["media_types"]
    if not isinstance(media_types, dict) or len(media_types) > 256:
        raise LookupAcceptanceError(f"{label} media types are outside the bound")
    if list(media_types) != sorted(media_types) or set(media_types) != set(resources):
        raise LookupAcceptanceError(
            f"{label} media types must exactly cover sorted resources"
        )
    for resource_id, media_type in media_types.items():
        _text(resource_id, f"{label} media resource")
        _text(media_type, f"{label} media type")


def _validate_observation(
    value: dict[str, object],
    *,
    catalog: dict[str, object],
    catalog_hash: str,
    pair: dict[str, object],
    version: str,
    scenario: str,
) -> dict[str, object]:
    required = {
        "catalog_sha256",
        "conditions_sha256",
        "dictionaries",
        "manifest_sha256",
        "pair_id",
        "revision",
        "scenario",
        "schema",
        "version",
    }
    _exact_keys(value, required, "Lookup observation")
    revisions = pair["revisions"]
    assert isinstance(revisions, dict)
    if (
        value["schema"] != OBSERVATION_SCHEMA
        or value["catalog_sha256"] != catalog_hash
        or value["conditions_sha256"] != pair["conditions_sha256"]
        or value["manifest_sha256"] != pair["manifest_sha256"]
        or value["pair_id"] != pair["pair_id"]
        or value["revision"] != revisions[version]
        or value["scenario"] != scenario
        or value["version"] != version
    ):
        raise LookupAcceptanceError("Lookup observation identity is invalid")
    dictionaries = value["dictionaries"]
    expected_dictionaries = catalog["dictionaries"]
    if (
        not isinstance(dictionaries, list)
        or not isinstance(expected_dictionaries, list)
        or len(dictionaries) != len(expected_dictionaries)
    ):
        raise LookupAcceptanceError("Lookup observation dictionary count is invalid")
    for dictionary_index, (dictionary, expected_dictionary) in enumerate(
        zip(dictionaries, expected_dictionaries, strict=True)
    ):
        label = f"Lookup dictionary {dictionary_index}"
        if not isinstance(dictionary, dict) or not isinstance(
            expected_dictionary, dict
        ):
            raise LookupAcceptanceError(f"{label} is invalid")
        _exact_keys(
            dictionary,
            {"components", "format", "logical_key", "name", "probes"},
            label,
        )
        if (
            dictionary["format"] != expected_dictionary["format"]
            or dictionary["logical_key"] != expected_dictionary["primary_component"]
        ):
            raise LookupAcceptanceError(f"{label} does not match the catalog")
        components = dictionary["components"]
        if not isinstance(components, list) or not components:
            raise LookupAcceptanceError(f"{label} components are invalid")
        normalized_components = [
            _relative_path(item, f"{label} component") for item in components
        ]
        if (
            normalized_components != components
            or len(set(normalized_components)) != len(normalized_components)
            or normalized_components[0] != expected_dictionary["primary_component"]
        ):
            raise LookupAcceptanceError(f"{label} components are not canonical")
        _text(dictionary["name"], f"{label} name")
        probes = dictionary["probes"]
        expected_probes = expected_dictionary["probes"]
        if (
            not isinstance(probes, list)
            or not isinstance(expected_probes, list)
            or len(probes) != len(expected_probes)
        ):
            raise LookupAcceptanceError(f"{label} probe count is invalid")
        for probe_index, (probe, expected_probe) in enumerate(
            zip(probes, expected_probes, strict=True)
        ):
            probe_label = f"{label} probe {probe_index}"
            if not isinstance(probe, dict) or not isinstance(expected_probe, dict):
                raise LookupAcceptanceError(f"{probe_label} is invalid")
            _exact_keys(
                probe,
                {
                    "category",
                    "entries",
                    "error_count",
                    "id",
                    "operation",
                    "query",
                    "suggestions",
                },
                probe_label,
            )
            for field in ("category", "id", "operation"):
                if probe[field] != expected_probe[field]:
                    raise LookupAcceptanceError(
                        f"{probe_label} {field} does not match the catalog"
                    )
            if probe["query"] != expected_probe["query"]:
                raise LookupAcceptanceError(f"{probe_label} query does not match")
            entries = probe["entries"]
            suggestions = probe["suggestions"]
            result_limit = expected_probe["result_limit"]
            assert isinstance(result_limit, int)
            if (
                not isinstance(entries, list)
                or len(entries) > result_limit
                or not isinstance(suggestions, list)
                or len(suggestions) > result_limit
            ):
                raise LookupAcceptanceError(f"{probe_label} results exceed the bound")
            _nonnegative_integer(
                probe["error_count"], f"{probe_label} error count", MAX_RESULTS
            )
            for entry_index, entry in enumerate(entries):
                _validate_article(entry, f"{probe_label} entry {entry_index}")
            _validate_text_signature_list(
                suggestions, f"{probe_label} suggestions", maximum=result_limit
            )
            if probe["operation"] == "lookup" and suggestions:
                raise LookupAcceptanceError(f"{probe_label} has unexpected suggestions")
            if probe["operation"] == "suggest" and entries:
                raise LookupAcceptanceError(f"{probe_label} has unexpected entries")
    return value


def _read_observation(
    path: Path,
    *,
    catalog: dict[str, object],
    catalog_hash: str,
    pair: dict[str, object],
    version: str,
    scenario: str,
) -> dict[str, object]:
    value, _ = _read_json(path, "Lookup observation")
    return _validate_observation(
        value,
        catalog=catalog,
        catalog_hash=catalog_hash,
        pair=pair,
        version=version,
        scenario=scenario,
    )


def _comparable(value: dict[str, object]) -> object:
    return value["dictionaries"]


def _path_component(value: object) -> str:
    return str(value).replace("~", "~0").replace("/", "~1")


def _append_difference(
    differences: list[dict[str, object]],
    *,
    observation: str,
    field: str,
    kind: str,
    expected: object,
    actual: object,
) -> None:
    if len(differences) >= MAX_DIFFERENCES:
        raise LookupAcceptanceError("Lookup comparison exceeds the difference bound")
    differences.append(
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
    *,
    observation: str,
    field: str,
    differences: list[dict[str, object]],
) -> None:
    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(set(expected) | set(actual)):
            child = f"{field}/{_path_component(key)}"
            if key not in expected:
                _append_difference(
                    differences,
                    observation=observation,
                    field=child,
                    kind="extra-in-actual",
                    expected=None,
                    actual=actual[key],
                )
            elif key not in actual:
                _append_difference(
                    differences,
                    observation=observation,
                    field=child,
                    kind="missing-in-actual",
                    expected=expected[key],
                    actual=None,
                )
            else:
                _collect_differences(
                    expected[key],
                    actual[key],
                    observation=observation,
                    field=child,
                    differences=differences,
                )
        return
    if isinstance(expected, list) and isinstance(actual, list):
        if (
            all(isinstance(item, str) for item in expected + actual)
            and expected == sorted(set(expected))
            and actual == sorted(set(actual))
        ):
            for item in sorted(set(expected) - set(actual)):
                _append_difference(
                    differences,
                    observation=observation,
                    field=f"{field}/{_path_component(item)}",
                    kind="missing-in-actual",
                    expected=item,
                    actual=None,
                )
            for item in sorted(set(actual) - set(expected)):
                _append_difference(
                    differences,
                    observation=observation,
                    field=f"{field}/{_path_component(item)}",
                    kind="extra-in-actual",
                    expected=None,
                    actual=item,
                )
            return
        for index in range(max(len(expected), len(actual))):
            child = f"{field}/{index}"
            if index >= len(expected):
                _append_difference(
                    differences,
                    observation=observation,
                    field=child,
                    kind="extra-in-actual",
                    expected=None,
                    actual=actual[index],
                )
            elif index >= len(actual):
                _append_difference(
                    differences,
                    observation=observation,
                    field=child,
                    kind="missing-in-actual",
                    expected=expected[index],
                    actual=None,
                )
            else:
                _collect_differences(
                    expected[index],
                    actual[index],
                    observation=observation,
                    field=child,
                    differences=differences,
                )
        return
    if expected != actual:
        _append_difference(
            differences,
            observation=observation,
            field=field,
            kind="value",
            expected=expected,
            actual=actual,
        )


def _validate_comparison(value: dict[str, object]) -> dict[str, object]:
    _exact_keys(
        value,
        {
            "catalog_sha256",
            "conditions_sha256",
            "difference_count",
            "differences",
            "equivalent",
            "manifest_sha256",
            "pair_id",
            "schema",
        },
        "Lookup comparison",
    )
    if value["schema"] != COMPARISON_SCHEMA:
        raise LookupAcceptanceError("Lookup comparison schema is invalid")
    for field in (
        "catalog_sha256",
        "conditions_sha256",
        "manifest_sha256",
        "pair_id",
    ):
        _hash(value[field], f"Lookup comparison {field}")
    differences = value["differences"]
    if not isinstance(differences, list) or len(differences) > MAX_DIFFERENCES:
        raise LookupAcceptanceError("Lookup comparison differences exceed the bound")
    if value["difference_count"] != len(differences):
        raise LookupAcceptanceError("Lookup comparison difference count is invalid")
    if value["equivalent"] is not (not differences):
        raise LookupAcceptanceError("Lookup comparison equivalent flag is invalid")
    for index, difference in enumerate(differences):
        if not isinstance(difference, dict):
            raise LookupAcceptanceError(f"Lookup difference {index} is invalid")
        _exact_keys(
            difference,
            {"actual", "expected", "field", "kind", "observation"},
            f"Lookup difference {index}",
        )
        _text(difference["field"], f"Lookup difference {index} field")
        if difference["kind"] not in {
            "extra-in-actual",
            "missing-in-actual",
            "value",
        }:
            raise LookupAcceptanceError(f"Lookup difference {index} kind is invalid")
        if difference["observation"] not in {
            "qt5-warm",
            "qt6-clean",
            "qt6-warm",
        }:
            raise LookupAcceptanceError(
                f"Lookup difference {index} observation is invalid"
            )
    return value


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
        name: _read_observation(
            path,
            catalog=catalog,
            catalog_hash=catalog_hash,
            pair=pair,
            version=version,
            scenario=scenario,
        )
        for name, (path, version, scenario) in identities.items()
    }
    reference = _comparable(observations["qt5-clean"])
    differences: list[dict[str, object]] = []
    for name in ("qt5-warm", "qt6-clean", "qt6-warm"):
        candidate = _comparable(observations[name])
        _collect_differences(
            reference,
            candidate,
            observation=name,
            field="dictionaries",
            differences=differences,
        )
    comparison = {
        "catalog_sha256": catalog_hash,
        "conditions_sha256": pair["conditions_sha256"],
        "difference_count": len(differences),
        "differences": differences,
        "equivalent": not differences,
        "manifest_sha256": pair["manifest_sha256"],
        "pair_id": pair["pair_id"],
        "schema": COMPARISON_SCHEMA,
    }
    return _validate_comparison(comparison)


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
            if output.exists():
                raise LookupAcceptanceError("Comparison output already exists")
            _atomic_json(output, comparison)
            result = output
    except (LookupAcceptanceError, OSError) as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
