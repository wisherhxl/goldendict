# Dictd Title Metadata Parity

## Requirement and impact check

R3.7c is a minor correction under approved `CRD-DICT-003` and
`CRD-COMPAT-001` in the [product baseline](qt6-product-baseline-crd.md).
The parent [R3.7 gap](qt6-baseline-gap-audit.md) remains open.

Frozen Qt 5 commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` is the
authority. `dictdfiles.cc:712-741` considers each accepted physical index
row whose primary headword starts with `00databaseshort` or
`00-database-short`. Optional original-headword aliases do not choose the
title. The last extractable title wins, including an empty title. The initial
name is filename-derived (`dictdfiles.cc:652`); a body beginning with either
short-metadata prefix without a following newline leaves that name, or a
previous title, intact.

The body is a NUL-terminated byte string (`dictzip.c:601-611,650`). Qt 5
optionally skips a prefix-starting first line, skips the six ASCII whitespace
characters (`utf8.cc:161-176`), and retains bytes until the next LF or NUL.
Trailing spaces and CR within or at the end of that line remain. Unicode
whitespace is not stripped and UTF-8 title bytes are preserved.
`dictdfiles.cc:160-169` supports the stored empty-name case.

The approved design stays in Core's existing private Dictd format adapter:
a bounded `string_view` extraction helper returns `optional<string>` to
distinguish no replacement from an empty replacement. Accepted primary-row
processing applies it after existing validation. No interface, dependency,
module, setting, GUI responsibility, or architecture decision changes. The
existing adapter and local helpers suffice without another design pattern.
Description parsing, alias counts, metadata full-text exclusion, discovery,
rendering, ordinary-gzip disposition, complete RA validation, and other R3.7
variants remain outside this unit. Existing corruption and size checks stay
in force. This record does not assert full Dictd parity or real-corpus proof.

Full-text artifacts serialize dictionary names in
`dictionary/full_text_index.cc:100-133`. A new private Dictd title semantic
stamp therefore invalidates former names even when both original files are
unchanged. The existing content-detection stamp remains, as do original
two-file source snapshots and dictionary IDs. No shared cache format changes.

## Acceptance and verification

- Generated reader cases cover both prefixes and prefix suffixes, case,
  physical-row ordering and alias exclusion, empty and missing-header-newline
  distinctions, ASCII and Unicode whitespace, LF/CRLF/interior CR/trailing
  spaces, embedded NUL, and non-ASCII UTF-8 titles.
- Plain and genuine RA companions preserve exact articles, names, counts,
  IDs, source snapshots, and cold/warm full-text results.
- An artifact with the actual former first-title interpretation and the old
  content-detection source key rebuilds once and then reuses the corrected name.
- Mixed-service discovery and lookup publish the corrected title.

### Versioned smoke fixture integrity

The application smoke fixture's `.index` addresses LF-authored bytes. A
Windows checkout with `core.autocrlf=true` previously converted `fixture.dict`
to CRLF, changing both the title and following article ranges. Correct title
parsing exposed that fixture corruption in two existing GUI smoke assertions.
The exact path `apps/goldendict/tests/fixtures/dictd/fixture.dict` now has the
`-text` attribute. Its canonical payload is unchanged: Git blob
`6fc4d0a40745b04604d68132609434765c4f6764`. The focused Python regression
checks raw bytes and all three indexed article ranges. GUI assertions and
product behavior are unchanged by this fixture correction.

An existing worktree may retain converted CRLF bytes after an attribute-only
fast-forward because the blob itself did not change. Before verification,
check `git ls-files --eol` for this exact path and compare
`git hash-object --no-filters` with the canonical blob above. If materialization
is needed, first verify that the bytes are solely the LF-to-CRLF checkout
transform of the canonical blob and that no user edit is present. Restore this
exact fixture's canonical LF bytes using the approved file-edit workflow,
then repeat both checks: `i/lf w/lf attr/-text` and the canonical hash must
match. This is checkout setup for unchanged versioned fixture content; do not
change dictionary offsets, expected GUI names, or global Git conversion policy.
The canonical payload is 38 bytes. The known automatic CRLF transform is 40
bytes with raw Git hash `7b19254758630a27d4e0cc3a7ca8cce4df8f3a9e`; another
hash must be inspected rather than overwritten as a presumed checkout artifact.

Implementation base: `b071cebe796639dad5379c45a9961dd5748d78be`.
Task branch: `fix/qt6-dictd-title-metadata`.
The clean prebuilt `worktrees/test-qt6-inspector-focus-parity` worktree is
reused sequentially after explicit sole-writer transfer. Its previous branch
is preserved; no other writer shares the index or ignored build output.

Run from the owning checkout with fresh short `TEMP`/`TMP` directories:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release -j 12
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(dictd_discovery_test|dictd_reader_test|dictd_dictionary_test|application_service_test)$' --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
python -m unittest discover -s scripts/tests -p '*_test.py'
git diff --check
```

Status: **Implementation and Windows verification complete** (2026-09-09).
The complete Release build passed. Focused CTest passed 4/4 in 3.75 seconds;
the final focused run also included the two affected GUI smoke checks and
passed 6/6 in 5.31 seconds. Final cumulative serial CTest passed 139/139 in
57.12 seconds. Python regressions passed 172 tests with two Windows
symlink-capability skips, including the new fixture byte/range check.

Before the production correction, the new reader cases produced 19 failures
with 35 passing cases. The first cumulative run passed 137/139 and exposed the
CRLF fixture issue described above; its historical logs remain preserved.
Restoring canonical fixture bytes and protecting them required no further
product-code or GUI-assertion edits. The subsequent Release build, focused
checks, and cumulative run all passed. All 33 local Markdown targets in the
three affected documents exist, and `git diff --check` passed.

Local evidence belongs in the workspace's `evidence/r3.7c-20260909/` directory,
outside Git. Generated cases prove this bounded unit; no real Dictd corpus
acceptance is claimed. Linux execution and parent R3.7 remain open; shared
logic uses portable C++. Completion audit, commit, push, and independent
integration audit follow the project delivery policy. Their exact identities
and results are recorded in the external evidence directory; implementation
verification does not assert audit Pass or integration.
