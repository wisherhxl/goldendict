# Ordinary DSL Optional Text in Automatic Collapse

## Authority and status

Complete for this bounded unit on 2026-09-10 at
`739866e34ed5efd085bdc3e70b0fb02edb08d681`, after independent completion and
integration Passes, target push and primary rebuild/smokes. Workspace evidence:
`evidence/optional-collapse-counting-20260910/target-delivery.md`.
Minor correction under `CRD-LOOKUP-003`, `CRD-DICT-003`, `CRD-PREF-002/004`
and the product CRD's Section 3 precedence rule. The user confirmed frozen Qt 5
counting, including hidden ordinary DSL optional text, on 2026-09-10. This
supersedes the migration record's generic hidden-text exclusion; it adds no
visible-only mode or preference and approves no intentional Qt 5 divergence.
The workspace decision queue is
`evidence/article-collapse-decision-queue-20260910.md`.

Delivery: `fix/qt6-optional-collapse-counting`, isolated worktree
`worktrees/fix-qt6-optional-collapse-counting`, base
`5260b76de575680e79426235d89adee0a195ba2c`. A fresh fetch found the primary
Qt 6 checkout clean and equal to `origin/feature/tiger-qt6-migration`;
the new worktree's `cmake` submodule is initialized. The frozen reference
remains `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.

This unit removes one independently observable discrepancy: hiding ordinary
DSL optional spans must not subtract their text from automatic-collapse size.
An apparently short article can therefore start collapsed because its hidden
optional text counts. Enabling automatic expansion changes optional visibility
and controls, but not this span text's contribution to the size decision.
It does not restore the complete Qt 5 collapse metric.

## Evidence and remaining metric gaps

Frozen `dsl.cc:857-863` emits ordinary `[*]` zones as
`<span class="dsl_opt" ...>`. Its article wrapper and expand-image emission
are at `1792-1830`. `article_maker.cc:649-665` removes only exact
`<div class="dsl_opt"` matches at positions greater than zero when expansion
is disabled, using the limited recursive DIV matcher at `575-591`. It does
not remove the ordinary SPAN output. The outer `article-style.css:467-470`
display rule is not supplied to the fragment conversion.

The metric at `article_maker.cc:641-675` decodes the complete dictionary
response with explicit UTF-8 byte length, converts it through
`QTextDocumentFragment::fromHtml(...).toPlainText()`, and compares its UTF-16
length strictly greater than the configured limit. The workspace's
`evidence/collapse-metric-investigation-20260910.md` records the call path.
`evidence/collapse-metric-oracle-20260910/README.md` records 36 measured rows,
including a reconstructed ordinary DSL wrapper whose length is 17 for either
expansion setting and 9 after removing its eight optional letters. This is
source-based synthetic evidence, not an actual dictionary/backend capture.
Actual frozen backend verification is recorded in
`evidence/optional-collapse-backend-20260910/README.md`; its canonical runs are
`final-generated/` and `final-oald8/`. The real DSL backend and ArticleMaker
retain hidden ordinary optional spans with both expansion settings. The
generated 256-letter optional body measures 266 UTF-16 units and collapses at
20; the real OALD8 query `'em` measures 89 and collapses at 75. Each companion
short dictionary stays open. These are exact backend/maker checks, not browser
visibility or full metric equivalence.

Deleting the Qt 6 subtraction leaves these separate obligations open:

- Whole backend-response HTML input, rich-text whitespace/entities/blocks,
  inline styles, image objects and malformed-input conversion.
- UTF-16 code units instead of the present per-entry UTF-8 scalar estimate.
- Exact legacy optional DIV matching/removal quirks, including offset zero,
  nested bare DIVs, malformed closes and case/attribute spelling.
- Dictionary-request aggregation instead of individual DTO entries, initial
  active-dictionary exemption, returned/error-heading accounting and final
  sole-dictionary expansion.
- Manual disclosure, grouping/navigation, From-label translation, resources,
  full normal-article visual parity and Linux/macOS acceptance.

These are retained parity gaps, not exclusions from the product requirement.
No generic HTML equivalence or full R9.8 completion follows from this unit.

## Private design and impact check

The existing private Core article composer owns desktop composition. Remove
`OptionalTextCodePointCount` and the expansion-dependent subtraction in
`modules/core/src/article/article_composer.cc`; retain `Utf8CodePointCount`
and the current strict comparison, naming its local value `article_size`
instead of `visible_size`. Remove the now-unused vector include. Keep the
optional marker constant and `RenderOptionalControls` because they still own
presentation markup.

The call path remains DSL reader markup -> private article assembler -> one
structured entry per backend Article in `dictionary_service.cc:1888-1922` ->
`DesktopFacadeImpl::ComposeLookupPage` -> private composer. Both facade
constructors snapshot the existing preferences into the same composition
options. The direct private tests and the public facade therefore exercise
the same decision. The correction does not require another responsibility,
strategy, adapter, public interface or dependency; existing composition and
data-flow boundaries satisfy the SOLID rules without a new abstraction.

Preserve byte-identical structured/headless plain text, sanitized bodies,
resource references, entry order/identity and original optional-control IDs.
Keep hidden checkbox controls, expanded control omission, script-free CSS,
strict document-prefix admission, escaped fallback, CSP and the 16 MiB output
bound. Keep default off/2000, current validated limits, configuration format,
preference reconstruction, tab replay, cancellation and failure behavior.
No indexes or stored articles change; rollback is the prior delivery revision
and needs no data migration. The portable Core change applies to all platforms;
available Windows verification does not claim execution on Linux or macOS.

## Acceptance and file budget

One coherent functional unit, expected net production deletion of about 50
lines in the composer only. No header, CMake, application, renderer, sanitizer,
resource or dependency edits are planned. Reuse the two existing Core tests:

1. Update `article_composer_test.cpp`'s optional-policy case so the same
   complete plain text starts collapsed with both expansion settings. Keep
   control IDs and exact output text assertions. Add focused equality,
   one-above, collapse-disabled and sole-entry assertions, reusing the same
   entries; target at most 45 added/changed test lines.
2. Add one `application_service_test.cpp` case, using existing
   `WriteDslTextFixture` and public facade/service helpers, with two actual
   generated DSL dictionaries returning the same query. Verify both
   expansion settings, positive hidden-optional contribution, unmodified
   plain-text/entry/resource output, and cold/warm lookup through separate
   facades and the two isolated index roots described below. Target 60-100 test lines
   before the real-data supplement below; no independent harness or new
   fixture API.
3. Compare the same minimal source bytes with the real frozen Qt 5 backend
   and exact collapse conversion. Record the actual response bytes, counts,
   expansion states, relevant versions and hashes outside the repository.
   Choose a threshold between the no-optional control and optional response
   for which both engines demonstrate the same changed collapse outcome;
   record their actual lengths separately. Do not assert total-length equality
   where remaining normalization/object differences prevent it. Root owns
   this external oracle independently; it must be available before acceptance.
4. Supplement the generated check with one read-only external real DSL source,
   through an opt-in `GOLDENDICT_OPTIONAL_COLLAPSE_DSL` path in the same
   focused facade test. Follow the existing `article_selection_test` external
   input convention; ordinary CTest remains self-contained. Pair that source
   with a tiny generated second dictionary in the temporary directory to
   avoid the sole-entry exemption. The actual frozen oracle determines the
   matched query and common threshold before implementation acceptance.
   Keep source/companions read-only, indexes and outputs isolated, and all
   corpus payload external. Compare every participating source/companion's
   exact size and SHA-256 before/after, retain cold/warm index hashes and
   timestamps to prove reuse, and compare exact cold/warm response text,
   bodies, identities, resources and composition for both flags. Record only
   safe outcome/length/hash evidence.
   Reuse the test body with at most 35 extra lines. The approved focused budget
   audit-rework amendment permits 165 facade-test lines including the slot declaration:
   safe source size/hash evidence and an ownership-specific collapsed-container
   assertion strengthen acceptance without another harness or interface.
   Use two isolated index roots and six explicit states: cold(false) on root 0,
   cold(true) on root 1, warm(false) on root 0, warm(true) on root 1, then
   switched(true) on root 0 and switched(false) on root 1. Assert each cold
   root is absent before construction and preserve its own index snapshot.
   This is one real-query supplement under CRD
   Sections 10.4/11, not closure of the full corpus or visual matrix.

Reconcile this record, the CRD decision log, the existing migration text and
the architecture document's behavioral hidden-text exclusion sentence;
add short links in parity, porting, gap and testing records when implementation
is accepted. Retain explicit pending status before the implementation gate.

Configure the owning worktree using the documented Windows Conan Release
workflow and VS 2026 with MSVC 14.44, then build and run:

```powershell
./run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release
./run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(article_composer_test|article_assembler_test|application_service_test|dsl_reader_test|dsl_dictionary_test|goldendict_articles_preferences_smoke|goldendict_optional_parts_preferences_smoke)$' --output-on-failure
```

Run the cumulative serial Release suite with separate fresh short C/D TEMP
roots, existing Python script tests, local Markdown links and `git diff --check`.
The existing preference smokes prove configuration/application transactions;
they are not evidence of exact Qt 5 threshold extraction. Stage the complete
isolated unit and bind a fresh no-history completion audit to HEAD and staged
Tree ID before the original developer commits/pushes. Integration requires its
own verification and independent audit under the authorized contract.

## Bounded verification result

The composer removes 53 lines and adds three; the existing direct test changes
25 lines and the existing facade test adds 159 within its 165-line budget. The direct
strict-boundary pair is 15/16 for the 16-letter fixture. Windows uses the
owning Qt 6.11.1 shared Conan runtime and VS 2026/MSVC 14.44 Release build;
no dependency or primary build output was borrowed.

Workspace `evidence/optional-collapse-counting-20260910/verification-v2.md`
records the commands, hashes and full verification status. The focused seven
suites and rework results are recorded there. Both generated and real facade
cases require the six explicit cold/warm/switched states above. Their source
bytes exactly match the independent
Qt 5 inputs (generated first/second: 742/216 bytes; real second: 208 bytes).
The real source and all adjacent companions preserve size/SHA-256/timestamps,
and isolated generated-index hashes/timestamps and complete response content
remain unchanged. Qt 6 reports 265 ASCII text bytes for the generated optional
entry and 104 UTF-8 bytes for the real entry; the latter byte count is not its
Unicode-scalar metric or a claim of equality with Qt 5's UTF-16 length.

A separately labelled mixed-runtime diagnostic copies the old base Core DLL
and current test executable into an ignored isolated directory, activates only
the owning Conan runtime, and records the actual loaded old Core path/hash and
Qt/MSVC modules. It fails exactly at the missing collapsed-container assertion
for real `'em` at 75. The normal candidate passes that same check. This proves
the real supplement distinguishes the previous subtraction behavior; the
mixed runtime is not candidate acceptance evidence. No candidate or baseline
binary is replaced. All raw corpus content remains local and external.

Whole-HTML/UTF-16 conversion, exact optional DIV behavior, grouping/exemptions,
manual disclosure, complete corpus/visual parity and Linux/macOS execution
remain open. This delivery claims only ordinary optional-text contribution.

Completion audit v1 failed because a single index root supplied only
cold(false), warm(true), warm(false), warm(true); it did not prove the claimed
cold(true) state. The approved rework retains the full acceptance matrix and
adds a separate cold root for each flag, plus same-flag and switched-flag warm
reuse. The old audit and evidence remain preserved. The reworked tree subsequently
passed fresh completion and integration audits, as recorded above.
