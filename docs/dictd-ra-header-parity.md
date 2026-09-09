# Dictd RA Header Admission Parity

## Requirement and impact check

R3.7e is a minor correction under approved `CRD-DICT-003` and
`CRD-COMPAT-001` in the [product baseline](qt6-product-baseline-crd.md).
The parent [R3.7 gap](qt6-baseline-gap-audit.md) remains open.

Frozen Qt 5 commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` is the
authority. `dictzip.c:319-364` recognizes only a leading `RA` extra subfield,
requires version 1 and a positive chunk count, and consumes its complete
table. `dictzip.c:368-432` bounds each optional original filename and comment
to 10239 content bytes and requires the consumed header length to agree with
the declared extra length after optional fields. The `SUBLEN` field is read
but deliberately ignored. Qt 6 previously passed these bytes directly to
Zlib, which ignores RA semantics and can decode otherwise valid gzip whose
RA header the frozen loader rejects.

The approved correction adds bounded streaming header admission inside
Core's existing private Dictd Reader, before its unchanged Zlib decompression.
For a leading RA subfield, version, count, complete table, exact extra length
(`10 + 2 * chunk_count`), optional string bounds/termination, and presence of
optional header-CRC bytes are checked. Zlib continues to own compressed-stream
and checksum validation and the existing decoded-output bound remains.
The header checker does not read or copy the complete compressed payload.

The existing format adapter and local helper pattern suffice. No public
interface, dependency, module, setting, GUI responsibility, or architecture
decision changes. Non-RA extra fields, including a later RA subfield, retain
the existing gzip route. Ignored SUBLEN values remain accepted. Chunk length
and table entry values gain no new interpretation.

No full-text semantic stamp is needed: accepted inputs produce identical
documents and source snapshots. `Dictionary::Open` opens the Reader before
accessing a generated full-text artifact, so a previously valid artifact
cannot bypass admission of now-rejected sources. Tests seed such an artifact
against the actual rejected source revision and verify it remains untouched.

## Scope and acceptance

- Both supported companion suffixes preserve valid multi-chunk data, optional
  header flags, metadata, counts, IDs, source snapshots and cold/warm results.
- Valid gzip with an unsupported RA version, zero count or inconsistent
  declared table length is rejected with the existing typed invalid-data
  error. Structural truncation and invalid optional fields are also rejected.
- Filename and comment boundaries, missing terminators, ignored SUBLEN and
  a non-leading RA subfield are covered explicitly.
- An invalid selected `.dict` never falls back to a valid `.dict.dz`.
- Cached full-text data does not bypass loader admission; mixed-format
  discovery retains healthy dictionaries alongside a rejected Dictd source.

Complete chunk-table/random-access validation, zero-chunk-length failure
disposition, ordinary-gzip product disposition, rendering, description,
ordering, identity and remaining R3.7 variants remain separate work. Generated
fixtures provide bounded evidence, not real Dictd corpus acceptance. Linux
execution and parent R3.7 closure remain open.

## Delivery and verification

Implementation base: `7d516f5198e3930a9f96f5098b644d5e174a58e0`.
Task branch: `fix/qt6-dictd-ra-header-admission`.
The clean prebuilt `worktrees/test-qt6-inspector-focus-parity` worktree is
reused after explicit sole-writer transfer; prior branches are preserved.

Run from the owning checkout with fresh short `TEMP`/`TMP` directories:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release -j 12
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(dictd_discovery_test|dictd_reader_test|dictd_dictionary_test|application_service_test)$' --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
python -m unittest discover -s scripts/tests -p '*_test.py'
git diff --check
```

Status: **Implementation and Windows verification complete** (2026-09-09).
The full Release build passed. Focused CTest passed 4/4 in 3.60 seconds;
cumulative serial CTest passed 139/139 in 56.52 seconds. Python regressions
passed 172 tests with two Windows symlink-capability skips. All 38 local
Markdown targets in the three affected documents exist; `git diff --check`
passed.

Before the production correction, all 20 original semantic-only rejection
cases failed: the former reader admitted gzip with invalid RA versions,
counts, extra lengths and oversized optional strings under both suffixes.
The no-fallback test also failed. Independent fixture inflation succeeded,
while the accepted-header and structural-truncation tests passed. The final
suite additionally covers the maximum 16-bit chunk count. The corrected
Reader rejects all 22 semantic cases and retains accepted optional flags,
ignored SUBLEN and non-leading-RA behavior. Backend tests prove that a valid
former full-text artifact bound to unchanged rejected sources cannot bypass
admission, and that artifact bytes and modification time remain unchanged.
The service retains healthy StarDict lookup with its existing typed startup
error for the rejected Dictd source.

Local command logs and subsequent audit/delivery identities belong in the
workspace's `evidence/r3.7e-20260909/` directory outside Git. The detailed
pre-fix QTest log is retained separately because CTest captured no test stdout
on this host. Generated fixtures provide this unit's evidence; no frozen
Qt 5 files or real dictionary corpus were modified. Completion audit, commit,
push and independent integration audit follow the project delivery policy;
implementation verification does not assert those gates passed. Parent R3.7
and Linux execution remain open.
