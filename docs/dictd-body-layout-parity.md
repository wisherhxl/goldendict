# Dictd Body Layout Parity

## Requirement and impact check

R3.7f is a minor correction under approved `CRD-DICT-003`,
`CRD-LOOKUP-003`, and `CRD-COMPAT-001` in the
[product baseline](qt6-product-baseline-crd.md). The parent
[R3.7 gap](qt6-baseline-gap-audit.md) remains open.

Frozen Qt 5 commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` defines
the layout in `dictdfiles.cc:343-373` and `htmlescape.cc:49-109`:
escaped text, leading nonbreaking-space indentation (four spaces per tab),
line divs, ignored carriage returns, first-NUL termination, and per-line
direction relative to the filename-derived target language. Qt 5 converts
the resulting bytes through `QString::fromUtf8`; malformed UTF-8 replacement
must be observed before choosing the compatible bounded implementation.

The approved unit restores this independently useful body layout through a
private Dictd renderer and Core's existing article assembly. Backslash and
brace markers remain literal. Their coupled phonetic and reference conversion,
including overlap and cross-line behavior, belongs to R3.7g. This unit does
not claim complete Dictd article, description, language-identity, ordering,
compression, platform or parent R3.7 parity.

## Design and acceptance

- A private Dictd renderer owns escaping, line layout, UTF-8 presentation and
  direction. Conversion checks cancellation and the existing 16 MiB article
  output ceiling while scanning and appending, before large intermediate
  allocations. The Reader continues to own source decoding and record ranges.
- A private shared language-pair primitive moves the already verified
  StarDict token inference and complete three-letter mapping without changing
  its behavior. This second actual consumer justifies the extraction;
  no module, installed interface or dependency is added. StarDict retains
  filename/book-name fallback. Dictd considers only the index basename,
  never its parent directory or metadata title, matching
  `dictdfiles.cc:784-797` and `langcoder.cc:321-346`.
- The Dictd adapter supplies private base direction, returns `text/html` for
  exact and prefix lookup, and ingests the same assembled body into full-text
  documents. A private semantic source stamp invalidates earlier plain-text
  artifacts while preserving physical source snapshots and document identity.
  Dictd full-text ingestion alone converts nonbreaking spaces to ASCII spaces,
  matching Qt 5's text conversion and preserving whole-word searches with the
  existing shared matcher. Article presentation retains its nonbreaking spaces;
  literal source entities remain literal text. This does not claim complete
  legacy full-text excerpt equivalence or change the shared matching policy.
- The sanitizer admits exactly `div.dictd_article`; existing `dir` handling
  and imported product CSS provide presentation. URL handling and broader
  sanitizer recovery are unchanged.
- Generated fixtures cover indentation, CR/LF/blank/final lines, escaping,
  NUL and malformed UTF-8, mixed and neutral direction, filename inference,
  exact/prefix agreement, cancellation, output bounds and seeded-old-cache
  migration. Existing StarDict tests protect the extracted behavior.
- Actual frozen Qt 5 observation uses disposable generated fixtures and
  isolated profiles outside both reference checkouts. It supplements focused
  headless HTML/plain-text tests. No real dictionary payload is modified.

## Delivery and verification

Actual frozen Qt 5 clean and warm observation covers 40 generated probes per
run: ten bodies across neutral, Arabic-target, Armenian-target and misleading
parent-directory names. Body tag structure, text, indentation and direction
match the Qt 6 sanitized output in both runs, with HTML entity spelling
normalized. These are semantic body comparisons, not product-shell captures or
pixel-parity evidence. The observer's matching historical harness and verified
source provenance were used without changing either reference checkout.

The observations pin one U+FFFD per invalid UTF-8 byte, including incomplete,
overlong, surrogate and out-of-range sequences; retained BOMs; removal of CR
before UTF-8 interpretation; supplementary right-to-left characters; nested
and unmatched isolates; and ignored isolated text during first-strong direction
selection. Focused tests retain these results.

An actual Qt 5 `QTextDocumentFragment::toPlainText` probe confirms conversion
of generated and literal nonbreaking spaces to ASCII spaces, while escaped
literal `&nbsp;` remains that literal text. Frozen `getArticleText` reaches this
conversion through `Html::unescape` after block/tag processing. The local
full-text correction prevents a new indentation-induced whole-word regression;
it does not reproduce or redefine that broader historical block processing.

Implementation base: `8a54fed635aef1896ea665e8ab91275fa2f33270`.
Task branch: `fix/qt6-dictd-body-layout`.
The prebuilt `worktrees/test-qt6-inspector-focus-parity` worktree is reused
after explicit sole-writer transfer. Evidence belongs in the workspace's
`evidence/r3.7f-20260909/` directory outside Git.

Status: **Implementation and Windows verification complete** (2026-09-09).
The final full Release build passed; focused CTest passed 7/7 in 5.98 seconds,
and cumulative serial CTest passed 139/139 in 58.47 seconds. Python regressions
passed 172 tests with two Windows symlink-capability skips. All 40 local
Markdown targets in the three changed documents exist and `git diff --check`
passes. The relocated language table retains its exact original Git blob.

The first focused run exposed historical raw-plaintext assertions and a real
new whole-word-search regression caused by indentation becoming nonbreaking
spaces. Historical tests now check assembled text; the Dictd-only full-text
normalization restores the original positive whole-word expectation. Cold/warm
tests prove seeded-old-artifact replacement, unchanged source stamps/document
IDs, hidden-after-NUL exclusion, literal entity preservation and separate exact
presentation/full-text excerpts. Cancellation tests interrupt early, middle
and late conversion work, including normalized-line scanning.

External `README.md`, observer scripts, raw observations, comparison and
initial/final verification logs record reproducible evidence. Completion and
integration audit Pass results are not claimed by implementation verification.

Run from the owning checkout with fresh short `TEMP`/`TMP` directories:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release -j 12
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(dictd_discovery_test|dictd_reader_test|dictd_dictionary_test|stardict_reader_test|stardict_dictionary_test|article_assembler_test|application_service_test)$' --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
python -m unittest discover -s scripts/tests -p '*_test.py'
git diff --check
```

Completion audit, commit, push and independent integration audit follow the
project delivery policy. Linux execution and parent R3.7 closure remain open.
