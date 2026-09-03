#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Create a deterministic, payload-free manifest for a dictionary corpus."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import stat
import tempfile
from typing import BinaryIO, Iterable


SCHEMA = "goldendict-real-dictionary-manifest-v1"
HASH_ALGORITHM = "sha256"
HASH_CHUNK_SIZE = 4 * 1024 * 1024


class ManifestError(RuntimeError):
    """Raised when a safe, stable corpus manifest cannot be produced."""


def _normalized_path(path: Path) -> str:
    return os.path.normcase(os.path.abspath(os.fspath(path)))


def _is_within(candidate: Path, parent: Path) -> bool:
    try:
        return os.path.commonpath(
            (_normalized_path(candidate), _normalized_path(parent))
        ) == _normalized_path(parent)
    except ValueError:
        return False


def _resolve_corpus(corpus: Path) -> Path:
    try:
        requested_stat = corpus.lstat()
        if corpus.is_symlink() or _is_reparse_point(requested_stat):
            raise ManifestError(
                f"Corpus root links and reparse points are not allowed: {corpus}"
            )
        corpus_root = corpus.resolve(strict=True)
    except ManifestError:
        raise
    except OSError as error:
        raise ManifestError(f"Corpus directory does not exist: {corpus}") from error
    if not corpus_root.is_dir():
        raise ManifestError(f"Corpus path is not a directory: {corpus_root}")
    return corpus_root


def _is_reparse_point(file_stat: os.stat_result) -> bool:
    attributes = getattr(file_stat, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return bool(attributes & reparse_flag)


def _utf8_key(text: str) -> bytes:
    try:
        return text.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ManifestError("Corpus paths must be valid UTF-8") from error


def _stable_path_key(path: Path) -> bytes:
    return _utf8_key(path.as_posix())


def _walk_regular_files(corpus_root: Path) -> list[Path]:
    files: list[Path] = []
    pending = [corpus_root]

    while pending:
        directory = pending.pop()
        try:
            entries = list(os.scandir(directory))
        except OSError as error:
            raise ManifestError(f"Cannot enumerate corpus directory: {directory}") from error

        entries.sort(
            key=lambda entry: _utf8_key(entry.name),
            reverse=True,
        )
        for entry in entries:
            path = Path(entry.path)
            try:
                entry_stat = entry.stat(follow_symlinks=False)
            except OSError as error:
                raise ManifestError(f"Cannot inspect corpus entry: {path}") from error

            if entry.is_symlink() or _is_reparse_point(entry_stat):
                raise ManifestError(f"Corpus links and reparse points are not allowed: {path}")
            if stat.S_ISDIR(entry_stat.st_mode):
                pending.append(path)
            elif stat.S_ISREG(entry_stat.st_mode):
                files.append(path)
            else:
                raise ManifestError(f"Unsupported non-regular corpus entry: {path}")

    return sorted(
        files,
        key=lambda path: _stable_path_key(path.relative_to(corpus_root)),
    )


def classify_file(path: Path) -> str:
    name = path.name.casefold()
    if name.endswith(".dsl.dz"):
        return "dsl_dictionary"

    suffix = path.suffix.casefold()
    classifications = {
        ".mdx": "mdict_dictionary",
        ".mdd": "mdict_resource",
        ".dsl": "dsl_dictionary",
        ".dz": "compressed_data",
        ".zip": "resource_archive",
        ".aff": "hunspell_affix",
        ".dic": "hunspell_dictionary",
        ".ann": "annotation",
        ".css": "stylesheet",
        ".bmp": "image",
        ".gif": "image",
        ".ico": "image",
        ".jpeg": "image",
        ".jpg": "image",
        ".png": "image",
        ".svg": "image",
        ".webp": "image",
        ".otf": "font",
        ".ttc": "font",
        ".ttf": "font",
        ".woff": "font",
        ".woff2": "font",
        ".md": "text_metadata",
        ".txt": "text_metadata",
    }
    return classifications.get(suffix, "other")


def _same_file_identity(first: os.stat_result, second: os.stat_result) -> bool:
    return (
        first.st_dev,
        first.st_ino,
        first.st_size,
        first.st_mtime_ns,
    ) == (
        second.st_dev,
        second.st_ino,
        second.st_size,
        second.st_mtime_ns,
    )


def _file_identity(file_stat: os.stat_result) -> tuple[int, int, int, int]:
    return (
        file_stat.st_dev,
        file_stat.st_ino,
        file_stat.st_size,
        file_stat.st_mtime_ns,
    )


def _stream_sha256(source: BinaryIO) -> str:
    digest = hashlib.sha256()
    while chunk := source.read(HASH_CHUNK_SIZE):
        digest.update(chunk)
    return digest.hexdigest()


def _hash_regular_file(path: Path) -> tuple[int, str, tuple[int, int, int, int]]:
    try:
        before = path.stat(follow_symlinks=False)
        if _is_reparse_point(before) or not stat.S_ISREG(before.st_mode):
            raise ManifestError(f"Corpus entry changed type before hashing: {path}")

        flags = (
            os.O_RDONLY
            | getattr(os, "O_BINARY", 0)
            | getattr(os, "O_NOFOLLOW", 0)
        )
        descriptor = os.open(path, flags)
        with os.fdopen(descriptor, "rb") as source:
            opened = os.fstat(source.fileno())
            if not _same_file_identity(before, opened):
                raise ManifestError(f"Corpus entry changed before hashing: {path}")
            digest = _stream_sha256(source)
            after_read = os.fstat(source.fileno())

        after_path = path.stat(follow_symlinks=False)
    except ManifestError:
        raise
    except OSError as error:
        raise ManifestError(f"Cannot hash corpus file: {path}") from error

    if (
        _is_reparse_point(after_path)
        or not _same_file_identity(before, after_read)
        or not _same_file_identity(before, after_path)
    ):
        raise ManifestError(f"Corpus file changed while hashing: {path}")
    return before.st_size, digest, _file_identity(after_path)


def _revalidate_corpus(
    corpus_root: Path,
    original_files: list[Path],
    hashed_identities: dict[str, tuple[int, int, int, int]],
) -> None:
    final_files = _walk_regular_files(corpus_root)
    original_relative = [path.relative_to(corpus_root) for path in original_files]
    final_relative = [path.relative_to(corpus_root) for path in final_files]
    if original_relative != final_relative:
        raise ManifestError("Corpus inventory changed while generating the manifest")

    for path, relative_path in zip(final_files, final_relative, strict=True):
        try:
            final_stat = path.stat(follow_symlinks=False)
        except OSError as error:
            raise ManifestError(f"Cannot revalidate corpus file: {path}") from error
        if (
            _is_reparse_point(final_stat)
            or not stat.S_ISREG(final_stat.st_mode)
            or _file_identity(final_stat)
            != hashed_identities[relative_path.as_posix()]
        ):
            raise ManifestError(
                f"Corpus file changed while generating the manifest: {path}"
            )


def build_manifest(corpus: Path) -> dict[str, object]:
    corpus_root = _resolve_corpus(corpus)

    records: list[dict[str, object]] = []
    classifications: Counter[str] = Counter()
    total_bytes = 0
    files = _walk_regular_files(corpus_root)
    hashed_identities: dict[str, tuple[int, int, int, int]] = {}

    for path in files:
        relative_path = path.relative_to(corpus_root).as_posix()
        size, digest, identity = _hash_regular_file(path)
        classification = classify_file(path)
        records.append(
            {
                "classification": classification,
                "path": relative_path,
                "sha256": digest,
                "size": size,
            }
        )
        classifications[classification] += 1
        hashed_identities[relative_path] = identity
        total_bytes += size

    _revalidate_corpus(corpus_root, files, hashed_identities)

    return {
        "classification_counts": dict(sorted(classifications.items())),
        "file_count": len(records),
        "files": records,
        "hash_algorithm": HASH_ALGORITHM,
        "schema": SCHEMA,
        "total_bytes": total_bytes,
    }


def _serialized_manifest(manifest: dict[str, object]) -> bytes:
    return (
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    ).encode("utf-8")


def write_manifest(
    corpus: Path, output: Path, manifest: dict[str, object]
) -> Path:
    corpus_root = _resolve_corpus(corpus)
    output_path = output.resolve(strict=False)
    if _is_within(output_path, corpus_root):
        raise ManifestError("Manifest output must be outside the corpus directory")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            dir=output_path.parent,
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(_serialized_manifest(manifest))
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temporary_path, output_path)
    except OSError as error:
        raise ManifestError(f"Cannot write manifest: {output_path}") from error
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()
    return output_path


def create_manifest(corpus: Path, output: Path) -> Path:
    corpus_root = _resolve_corpus(corpus)
    output_path = output.resolve(strict=False)
    if _is_within(output_path, corpus_root):
        raise ManifestError("Manifest output must be outside the corpus directory")
    return write_manifest(corpus_root, output_path, build_manifest(corpus_root))


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a deterministic SHA-256 inventory without copying dictionary "
            "payloads or writing into the corpus."
        )
    )
    parser.add_argument("corpus", type=Path, help="Read-only dictionary corpus directory")
    parser.add_argument("output", type=Path, help="JSON output path outside the corpus")
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _argument_parser().parse_args(arguments)
    try:
        output = create_manifest(options.corpus, options.output)
    except ManifestError as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
