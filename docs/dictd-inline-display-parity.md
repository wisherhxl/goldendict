# Dictd Coupled Inline Display

## Requirement and design

R3.7g.2 implements the approved `CRD-DICT-003`, `CRD-LOOKUP-003`,
`CRD-COMPAT-001`, [EL-01](empty-lookup-target-decision.md) and
[N3](malformed-reference-decision.md) and [N4](dot-only-reference-decision.md)
requirements. The Ready decomposition
places this unit after [independent extraction](dictd-full-text-extraction-parity.md).
The resumed implementation base is `72822d014a7fca2ba109539067dab81d2f98f01b`.
No requirement, installed interface, navigation policy or architecture changes.

Frozen `dictdfiles.cc:349-394` at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` defines sequential preformat,
phonetic, brace-reference and complete-opening-tag href rewrites. Source markup
is escaped before those transformations. Display and pre-href full-text
extraction have different contracts and independent oracles.

Private Core `dictd_article_renderer.cc` retains its concrete transformation
functions. Display requests an optional bounded reference sidecar from brace
conversion: generated opening offset, capture range and complete-tag rewrite
eligibility. The original capture's generated quotes determine eligibility;
resulting browser URLs or unusual source characters do not. Normalization
replaces generated tags and `&nbsp;` with spaces, applies frozen whitespace
simplification, then uses the existing typed lookup URL constructor. A successful
empty normalized target follows EL-01; successful exact `.` or `..` targets
follow N4 before encoding. Failed origins follow N3 separately. N4 changes
neither other dot/slash/percent/entity targets nor independent full text.

A private generated-grammar normalizer emits balanced div/span/a markup using
a bounded open-element stack and one active-anchor record. Anchor reconstruction
retains the originating record, href eligibility and target. Closing a span
cannot cross a div boundary; spans are never reconstructed. Failed opening tags
are tokenized with their actual quoted-attribute boundary, preserving leaked
quote text. Source text is not reinterpreted as arbitrary HTML. Every failed
origin and clone emits an anchor with the private inert class and no href.

The strict article sanitizer admits only `span.dictd_phonetic` and
`a.dictd_inert_reference` in addition to its existing classes. Product resources
already own frozen phonetic color, italics and pseudo-element backslashes;
scoped inert-reference CSS restores blue and underlining. Scoped Dictd div
rules restore WebKit's `unicode-bidi: normal` for ordinary blocks and `embed`
for explicit `dir` blocks; Chromium's default `isolate` is a real behavior
difference, not a serialization tolerance. Core retains all
format logic; app code only presents content and forwards valid intent.

Actual frozen 69-case control-glyph evidence establishes zero visual advance
for retained U+0001..0008, U+000B..000C, U+000E..001F and U+007F. Only final
display text emission wraps consecutive bytes from that exact set in
`span.dictd_control`, whose scoped zero font size suppresses Chromium's extra
glyph/advance. The original bytes remain in DOM text; target normalization and
independent FTS precede this adaptation. TAB, CR, LF, C1 and other Unicode are
unchanged. The strict sanitizer admits only this exact span class. The original
DOM comparison projects away only this wrapper, retaining all original
elements, boundaries and text; raw measurements retain the wrapper itself.
Frozen plain/reference, consecutive/whitespace, phonetic and reconstruction
contexts must demonstrate no extra inline spacing or changed line boxes.

Normal lookup documents use Core's `article_document.cc` embedded stylesheet;
the full legacy resource was previously used only for welcome/help documents.
The same narrow Dictd phonetic/pseudo, active/inert link, control and bidi rules
therefore also belong in that existing document prefix. A private app test
compares those exact rules with the resource and observes their computed
effects in real MainWindow pages. CSP, general body styles and other formats
are unchanged. Scoped anchor `text-decoration-thickness: auto` also overrides
the generic document's measured 1.28px thickness without changing other formats;
controlled and actual-page computed values must agree for active/inert anchors.
The broader normal-document system-ui/1rem/1.55 typography and
template differ from frozen product CSS and remain R9.8. Controlled full-CSS
format captures plus actual scoped-style/state proof do not close whole normal
article visual parity, R9, Dictd parity or cutover.

All growing buffers, duplicated captures, sidecars, stack and URL expansion
are checked before allocation against 16 MiB. Forward scans and chunked appends
retain checkpoints at most 4096 work units apart. No general parser, new
dependency, public test API or speculative design pattern is introduced.

## Acceptance and delivery

Core tests cover original 48 probe cases, exact active targets, every failed
origin/clone, normalized-empty references, literal braces, unmatched markers,
Unicode/entities/controls, expansion and cancellation. Preserve all 111 actual
Qt 5 full-text oracle rows and extraction/cache migration regressions.

Actual Qt 5 WebKit and Qt 6 WebEngine comparisons load their product CSS and
control viewport, DPI, font, locale and widget style. External evidence retains
paired screenshots, DOM text/anchor/block boundaries, computed styles and
phonetic pseudo-elements for all original 48 bodies. Earlier unstyled probe
outputs remain input evidence only.

Matched native measurements retain raw glyph/antialias differences under CRD
Sections 5, 6 and 10.2. The controlled Windows matrix has exact root rectangles,
line heights, direction and semantic styles; the maximum original-element
text-geometry delta is 2.453125px on a long run. Same-engine control comparisons
permit only the measured 1/64 CSS px horizontal letter x/width quantization at
the zero-width span boundary. Control advance, vertical coordinates/heights
and root geometry remain exact; surrounding whitespace runs and original bytes
remain mandatory. This is neither a general pixel threshold nor permission for
layout changes. Raw screenshot differences are retained without masks, and
representative pairs require visual review. These Windows measurements do not
define fixed glyph-width assumptions for portable CTest or close R9.8.

A narrow app-private test exercises real MainWindow rendering and pointer
activation on every inert origin and clone. A private friend accessor may
inspect existing state and install the existing AudioPlaybackService sinks;
it adds no product behavior or general testing framework. Each normal and
supported new-tab attempt proves its DOM hit and compares page/generation,
query/navigation dispatch, genuinely active sink lifetime, open article search,
history and all tab/session state. Explicit snapshots include every tab's
widget/view/page identity, title, URL and browser history. The playback sink
owns a live timer lease with actual resource bytes, stable identity and ongoing
activity; this proves sink lifecycle rather than physical audio playback.
Test-only toolbar wrapping and a window width derived from its size hint
establish visible article search even with offscreen font fallback.
Active controls prove exact dispatch and DOM pointer hits in both dispositions.
Original zero-area anchors in cases 12/23 and the separately measured empty
first fragment of the newline/indentation fixture are recorded as unclickable;
every visible clone and the supplemental actual-NBSP empty anchor require hits.

Verification uses the owning Conan Release build and runtime launcher, focused
Core/assembler/ArticleView/MainWindow checks, full serial CTest with fresh short
C-drive and D-drive TEMP/TMP roots, Python tests, Markdown links/anchors and
`git diff --check`. External evidence belongs to workspace
`evidence/r3.7g2-20260909/`. Stage the complete unit and bind fresh independent
completion audit to HEAD and staged Tree ID before commit/task push. Separate
integration verification and audit precede authorized target advancement.

R3.7g.2 bounded Windows inline display and inert activation is Complete.
Independent completion and integration audits passed, and the delivery reached
`feature/tiger-qt6-migration` at
`e013a4ddd7376508404be71cef4e8aa7a190b977` (tree
`b8ea35ac69cc46d407f206ad268b936869383186`). Workspace
`evidence/r3.7g2-20260909/target-delivery.md`, `completion-audit-pass.md` and
`integration-audit-pass.md` record the audited delivery.
Completed EL-01/N2/N3/N4 decisions
remain distinct from this runtime unit. Windows results do not close
Linux/macOS, real Dictd corpus coverage, full Dictd parity, R9.8 or cutover.

Release build, all 203 Dictd Core cases (including the independent 111-row FTS
oracle/cache/bounds suite), 50 assembler cases, native app QTest 6/6 and full
serial CTest 141/141 under each fresh C/D TEMP root pass. Python has 172 tests
with two existing platform skips. Native evidence includes 137 screenshots,
130 frozen paired comparisons, 90 inert normal/middle-pointer hits, 48 exact
positive hits and four required zero-area records. All 71 actual lookup-page
anchors have blue/underlined/auto-thickness styling. External reproduction,
raw measurements/hashes, pixel reports, state snapshots and prior diagnostic
classification are in workspace
`evidence/r3.7g2-20260909/verification-20260909.md`; the accepted final rerun is
`native-final-v2`, `final-native-v2.txt`, `final-styled-comparison-v2.json`,
`final-pixels-v2.json`, `final-ctest-c-v2.txt` and `final-ctest-d-v2.txt`.
The large activation JSON should be inspected programmatically with selected
fields/counts/digests, not dumped into a review context.
