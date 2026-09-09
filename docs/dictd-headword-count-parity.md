# Dictd Headword Count Parity

## Requirement and impact check

R3.7d is a minor correction under approved `CRD-DICT-003` and
`CRD-COMPAT-001` in the [product baseline](qt6-product-baseline-crd.md).
The parent [R3.7 gap](qt6-baseline-gap-audit.md) remains open.

Frozen Qt 5 commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` defines
the behavior. `dictdfiles.cc:701-710` increments the reported word count once
for each accepted primary row and once more whenever its fourth column exists,
including an empty column or one identical to the primary headword.
`dictdfiles.cc:114-115` returns that stored count directly. Qt 6 previously
counted retained searchable records, omitting those two fourth-column cases.

The approved correction separates reported headword accounting from searchable
record insertion inside Core's existing private Dictd Reader. Three-field rows
contribute one headword; four-field rows contribute two. Each accepted row still
contributes one article. Existing row validation, malformed-row recovery,
searchable records, lookup, suggestions, enumeration, full-text documents,
metadata, source snapshots, and dictionary IDs remain unchanged. No public
interface, dependency, setting, module, GUI ownership, or architecture decision
changes. The existing adapter and local accounting suffice without another
design pattern.

No full-text semantic stamp is needed. `dictionary/full_text_index.cc:100-133`
serializes only dictionary ID/name, headword, document ID, and plain text, not
reported counts. Those serialized values do not change. Every Dictionary open
publishes current counts from a freshly opened Reader, including when its
full-text artifact is reused. This correction does not change the separate
full-text result DTO serialization contract.

Description rendering/selection, other metadata, empty primary headwords,
ordinary-gzip disposition, complete RA validation, remaining indexing and
rendering variants, and complete Linux acceptance remain separate R3.7 work.
Generated fixtures prove this bounded correction, not real-corpus or full
Dictd parity.

## Acceptance and verification

- Raw generated indexes cover absent, empty, identical, distinct, and
  case-equivalent fourth columns; duplicate rows; skipped malformed rows;
  LF, CRLF, and a final row without a newline.
- Counts follow accepted physical rows and fourth-column presence while exact
  and prefix lookup, suggestions, enumeration, article bytes, source ordinals,
  and full-text document identity remain unchanged.
- Existing strict accepted-row errors and empty-index behavior remain covered.
- Backend cold/warm full-text opens publish corrected catalog identity counts,
  preserve original sources, and reuse unchanged generated artifacts.
- Mixed-service discovery publishes the corrected Dictd identity alongside
  StarDict, with working lookup and unchanged dictionary IDs.

Implementation base: `ea6dceb89a451a09a5122be4dbcf83fdeadb87ca`.
Task branch: `fix/qt6-dictd-headword-counts`.
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
The complete Release build passed. Focused CTest passed 4/4 in 2.88 seconds;
cumulative serial CTest passed 139/139 in 55.87 seconds. Python regressions
passed 172 tests with two Windows symlink-capability skips. All 36 local
Markdown targets in the three affected documents exist; `git diff --check`
passed.

Before the production correction, the new reader cases failed only for empty
and identical fourth columns: two physical rows reported two headwords instead
of four. Absent, distinct, and case-equivalent column cases passed. The
pre-fix CTest run also failed the backend and service test targets;
the post-fix focused and cumulative runs passed without changing those tests.

Local command logs and subsequent audit/delivery identities belong in the
workspace's `evidence/r3.7d-20260909/` directory outside Git. Generated fixtures
provide this unit's evidence; the external real corpus contains no Dictd
family, and no real Dictd acceptance is claimed. Completion audit, commit,
push, and independent integration audit follow the project delivery policy;
implementation verification does not assert those gates passed. Parent R3.7
and Linux execution remain open.
