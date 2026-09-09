# Dictd Independent Full-Text Extraction

## Requirement and design

R3.7g.1 is a minor correction to `CRD-DICT-003`, `CRD-FTS-001/002/003`
and `CRD-COMPAT-001/004` in the [product baseline](qt6-product-baseline-crd.md).
The approved Ready decomposition follows the
[N3 decision](malformed-reference-decision.md). This unit restores extraction
and cache migration; inline display, CSS and activation remain R3.7g.2.

Frozen commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`,
`dictdfiles.cc:573-589` and `htmlescape.cc:149-175`, defines the oracle:
preformat lines, substitute nonempty phonetics, substitute nonempty brace
references, replace block tags with spaces, remove remaining tags, trim the
intermediate text and convert HTML entities/whitespace. Href cleanup is absent.
The Qt 5 browser display is not the full-text oracle.

Private `dictd_article_renderer.cc` owns these concrete transformations.
`PreformatLines` is shared with the unchanged literal display renderer.
`ExtractArticleText` independently converts the generated div/span/anchor
grammar and the five emitted entities, without QtGui or article assembly.
There is no new public interface, dependency, module or speculative pattern.
The Reader owns source decoding and ranges; the adapter retains metadata
exclusion, range deduplication and `dictd-index:<ordinal>:<offset>:<size>` IDs.
A private semantic source stamp invalidates prior generated artifacts while
retaining every physical source snapshot and previous semantic stamp.

Every growing intermediate and duplicated capture is bounded at 16 MiB before
growth. Forward scans and chunked appends observe private checkpoints at most
4096 work units apart. `Dictionary::Open` still eagerly ingests documents and
has no cancellation/deadline parameter; private extraction checkpoint tests do
not establish cancellable production ingestion. Search cancellation is separate.

## Acceptance

Compare all 48 existing frozen probe FTS rows and 63 additional Qt 5.15.19
whitespace/entity observations. Retain compact input/output fixtures with
provenance. Cover malformed UTF-8, CR/NUL, indentation, Unicode whitespace,
overlap, output expansion and early/middle/late cancellation. U+2029 produces
LF per occurrence; U+2028 collapses; generated NBSP survives intermediate trim;
literal entity spellings decode only once. Final global trimming is incorrect.

Seed a full R3.7f artifact and verify stale replacement, exact text/excerpts,
stable IDs, warm restart, corrupt recovery, unchanged sources and preservation
of a prior artifact on failed publication. Existing exact/prefix and display
checks protect the deliberately unchanged rendering behavior.

Run the owning Conan Release build (`-j 12`), focused serial CTest
`^(dictd_reader_test|dictd_dictionary_test|article_assembler_test|application_service_test)$`,
full serial CTest, Python script tests, Markdown links/anchors and whitespace.
Use fresh short process-local `TEMP`/`TMP`. External observations and logs live
in workspace `evidence/r3.7g1-20260909/`; oracle inputs remain read-only in
`evidence/r3.7g-design-probe/`, `r3.7g-design-probe-extra/`, and
`r3.7g1-fts-oracle-20260909/`.

The [generated-index publication prerequisite](generated-index-publication.md)
is integrated at `480939d0968f190bb613d3712c2d937bfd11d03f`. It removes
destructive delete/retry behavior and independently
tests preservation of a deletable prior file when the prepared replacement is
locked. This unit additionally verifies Dictd's stale-cache replacement failure
is contained and retains the old artifact, then succeeds after the lock closes.
No shared persistence code is changed by this unit.

The first completion audit failed because the required dictionary-bar smoke
timed out while saving configuration. Its results and diagnostic evidence are
retained externally; earlier passing checks do not supersede that Fail.
The separate [configuration publication correction](configuration-file-publication.md)
is integrated at `1c2615414bb93cde15c87abfb282e963d962ec82`, the updated
delivery base for this unit. Neither configuration changes nor display R3.7g.2
changes belong here.

Final Windows Release verification on the updated base passed: the full build,
118 named extraction/bounds/migration cases, focused CTest 4/4, and full serial
CTest 140/140 with each of fresh C-drive and D-drive temporary roots. The
dictionary-bar smoke passes in both full runs. All 111 oracle fixture rows
match the original Qt 5 observation bytes, outputs and direction fields.
Python reports 172 tests with two existing Windows capability skips. Markdown
paths/anchors, read-only formatting validation and whitespace checks pass.
External `second-*` logs retain this verification; the first audit Fail and
earlier `final-*` results remain historical evidence.

Independent completion audit precedes commit/task push; separate integration
audit precedes target advancement. These verification results do not claim
either audit has passed.

Windows evidence does not close Linux/macOS, R3.7g.2, full Dictd parity or cutover.
