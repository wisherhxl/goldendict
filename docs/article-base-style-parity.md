# Normal Article Base Canvas and Typography

## Requirement and impact check

Approved minor correction under `CRD-SHELL-005`, `CRD-LOOKUP-003`,
`CRD-RES-002/004` and the Qt 6 Product Baseline CRD Sections 3, 6 and 10.2.
This is one bounded R9.8 conformance unit. It introduces no requirement,
architecture, public API, configuration or dependency decision. Development
was approved after the coordinator's Ready impact review on 2026-09-10,
recorded in workspace
`evidence/article-base-style-20260910/root-impact-review.md`. Implementation
and completion acceptance are separate from this Ready result.

Implementation and bounded Windows verification are complete; independent
completion audit and target integration remain separate delivery gates.

Development branch: `fix/qt6-article-base-style`; isolated base:
`e013a4ddd7376508404be71cef4e8aa7a190b977`. Qt 5 evidence remains frozen at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.

Frozen `article_maker.cc:45-66` inserts the product `article-style.css` into
normal lookup documents. Its `body` rule at lines 4-9 sets `#fefdeb`,
`Tahoma, Verdana, "Lucida Sans Unicode", sans-serif`, and `13px`; its `pre`
rule at lines 29-32 sets `12px`. Neither rule supplies a width cap, padding,
line-height, auto dark scheme, word breaking, pre wrapping or pre scroll box.
Frozen `articleview.cc:361` and `mainwindow.cc:1340` configure WebKit security
and inspector attributes without replacing those base fonts.

The external design probe in workspace `evidence/article-base-style-20260910/`
uses actual Qt 5.15.19 WebKit, the exact legacy doctype and complete frozen CSS.
At 320, 800 and 1600 CSS-pixel viewports, measured browser DPR is 1. Body has
8px margins, no padding, content-box sizing, no max-width, normal wrapping,
black text and the pale-yellow background. Body width is viewport minus 16px.
On the recorded Windows fonts, body normal line height is 16px; monospace pre
is 12px with a 15px line box, 12px vertical margins, `white-space: pre` and
visible overflow. A 240-character token stays on one line and makes the page
scroll horizontally. A three-line pre stays three lines at every width; it
does not gain an independent scrolling box. These native line/glyph metrics
are evidence, not portable fixed-font assertions.

## Bounded implementation

Replace only the base declarations inside private Core
`modules/core/src/article/article_document.cc`:

- Remove `:root{color-scheme:light dark}`.
- Replace the demo body rule with the exact three frozen body declarations,
  plus explicit `color:#000` to preserve the measured frozen foreground when
  Chromium inherits a dark platform palette.
- Replace `pre{overflow:auto;white-space:pre-wrap}` with the frozen 12px rule.

The resulting browser defaults supply margins, box sizing, normal line-height,
normal word wrapping, pre whitespace and overflow. Do not hardcode the observed
Windows 16px/15px line heights or font metrics. Font fallback retains the frozen
ordered family list; Linux does not acquire a Windows-font dependency.

The explicit foreground is a bounded browser adaptation approved in the same
impact record. Removing the old auto scheme alone did not restore black text
under the diagnostic dark platform palette; adding root `color-scheme:light`
also did not. The body foreground restores inherited normal/pre/Dictd text
without changing scoped anchors. Tests compare the three frozen declarations
exactly against the packaged resource and check the additional black foreground
separately. This is not automatic dark-theme support.

Article document assembly already belongs to Core. Retain its private embedded
literal convention, and test the bounded rules against the packaged product
resource. Core will not read an application QRC, filesystem CSS, frozen source
or WebEngine state. No new library, strategy, parser, generalized style manager
or public testing seam is needed. This preserves cohesive ownership and the
headless boundary under the project design rules and SOLID policy.

Keep CSP, sanitizer admission, typed resource/navigation URLs, output bounds,
generated Dictd controls/phonetics/references/bidi rules and optional/collapsed
article behavior unchanged. Plain-text DTOs and full-text extraction do not
depend on these CSS declarations. Preserve exact-prefix body extraction.

The body inheritance correction necessarily changes descendants without their
own font/line-height, including normal Dictd text. Existing generic link,
table/media and result/heading/optional-part selectors remain as they are.
Their `rem` sizes retain browser-root sizing, while inherited typography uses
the new body size. In particular, the generic dark-mode link color still has
its own media rule; this unit restores the base canvas/text, and does not claim
theme or generic-link parity. Frozen Dictd outer spacing and dictionary heading
markup remain separate template/style gaps. Do not add those selectors here.

## Composition and persistence compatibility

`ExtractDocumentBody` accepts only the exact current Core prefix and suffix;
`ComposeLookupPage` falls back to escaped plain text for other documents. This
must remain a strict boundary. Merely changing a prefix is not evidence of a
reachable upgrade defect.

The production path was traced before this design: every
`DictionaryService::Lookup` result is assembled at `dictionary_service.cc:1888`
from a backend `Article`, and its HTML is then passed through the same runtime
facade/composer. `MainWindow::FinishLookup` at lines 14537-14614 publishes that
composition through `PublishArticleHtml` and ArticleView. Results retained in
the window are in-memory objects from that same process. Tab restoration stores
navigation state and triggers lookup; it does not deserialize assembled HTML.

Only StarDict records and full-text documents use `StoreGeneratedIndex` in
current Core. StarDict `stardict_reader.cc:1006-1079` serializes counts,
headwords and byte ranges; lookup reads source article bytes and decodes them.
`full_text_index.cc:99-135` serializes IDs, names, headwords, document IDs and
plain text. Dictd `dictd_dictionary.cc:36-50` renders raw reader data per lookup.
No persistent assembled-HTML read path was found. Network response caches hold
provider transport payloads upstream of assembly. Therefore no cache version
bump, invalidation, prefix migration or widened HTML admission is proposed.
Tests must preserve both current document extraction and rejection/fallback for
old/foreign prefix documents, including the previous demo-prefix signature.

## Acceptance and likely files

The expected product change is under 15 edited lines in `article_document.cc`.
Add a bounded `article_base_style_test.cpp` (target approximately 250-400 lines)
under app tests and about 20 lines of CMake registration. Reuse the existing
application source/link target list for a private MainWindow test executable;
do not enlarge the 1013-line Dictd harness or add another presentation library.
Use public MainWindow setup/lookup methods and existing child widgets where
possible; a public API or production behavior change is not planned.

The existing smoke-only `RunArticleTabsSmokeCheck` also gives its reopened
synthetic page the same 3000px spacer as its initial page. Previously only the
initial page had that fixture: with smaller restored typography the real
reopened page clamped a correctly requested CSS scroll y75 to13.6. Adding the
same spacer in its existing DocumentReady observer removes the incidental
typography dependency while retaining every actual/requested scroll, token,
search-generation and ownership assertion. Product scroll logic is unchanged.
This narrowly related fixture correction was approved in the impact record.

Add approximately 30-60 lines of focused composer coverage for current-prefix
round trips and old/foreign prefix escaped fallback, preserving existing
sanitizer and bounded-document suites. Keep documentation changes to this
record, testing guidance, the R9.8 gap/parity/porting ownership entry and the
closely related R3.7g.2 completed-status reconciliation. Reconcile g2 to exact
integrated commit `e013a4ddd7376508404be71cef4e8aa7a190b977` with workspace
`evidence/r3.7g2-20260909/target-delivery.md`, completion and integration reports;
do not reopen its already completed implementation or close its parent gaps.

Acceptance fixtures are generated locally: a tiny StarDict with plain HTML,
three-line pre including spaces/TAB, a long normal token and a long pre line;
and a tiny Dictd body with ordinary/phonetic/valid/inert reference text. Use
fresh profiles and generated indexes outside sources. The app test must run
real discovery and asynchronous lookup through MainWindow, not replace its
page with a fragment or injected full CSS. Its new captured normal pages must
contain the production Core prefix, rendered fixture content and actual scoped
Dictd effects. Cold and reopened warm profiles must produce equivalent body
content, style and provenance; generated StarDict/FTS cache bytes should remain
stable on a warm reopen when the cache is current.

At actual 320, 800 and 1600 CSS-pixel browser widths, measure computed body/pre
styles, body/paragraph/pre/token rectangles, line-count/wrap relationships,
horizontal document overflow and lack of a nested pre scroller. Portable CTest
asserts CSS values and relationships (including viewport minus margins), not
Windows glyph widths. Compare inherited Dictd body font/line-height with normal
text and preserve scoped green/italic pseudo-backslashes, blue underlined
active/inert anchors. Existing `dictd_inline_display_test` carries independent
controls/bidi and broader g2 regression coverage; the new tiny fixture does not
duplicate or claim that entire matrix. Verify the body stays the
frozen light canvas with automatic dark preference; generic link/theme
differences remain explicitly outside this unit.

For native Windows acceptance, pair frozen Qt 5 and actual Qt 6 page captures
with English locale, Fusion, font metadata/hashes, no user/addon theme, zoom 1,
matched logical viewport and measured browser/widget DPR. Record engine default
font settings before any capture controls. Qt 5 uses its isolated MSYS runtime;
Qt 6 uses this checkout's Conan launcher. Raw screenshots and JSON remain
external. Compare base geometry/styles and wrapping separately from known
heading/template offsets; retain full images without hiding those differences.
The design-only Qt 5 CSS probe is an oracle input, not proof of actual Qt 6
MainWindow publication or completion. Obtain product-page paired evidence
during implementation, supplementing source/CSS isolation when useful.

Run the owning Release build and focused suite first:

```powershell
./run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release
./run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(article_base_style.*test|article_assembler_test|article_composer_test|application_service_test|stardict_reader_test|stardict_dictionary_test|dictd_dictionary_test|dictd_inline_display_test|article_selection_test|article_page_.*routing_test|article_tabs_test|goldendict_article_tabs_smoke)$' --output-on-failure
```

Then full serial CTest with separate fresh short C/D TEMP/TMP roots, Python
tests, Markdown link validation and `git diff --check`. No install/package or
dependency behavior changes are intended. Coordinate Conan/build/native GUI
work with the primary environment-refresh owner; never borrow its activation
or build directory. Stage only the complete functional unit for a fresh
independent completion audit, then audited commit/task push and separate
integration verification/audit under the existing contract.

## Recorded acceptance and limits

Workspace `evidence/article-base-style-20260910/verification.md` identifies the
build/test commands, paired native measurements, provenance and immutable
failure diagnostics. Final `v5` evidence passes the owning Release build,
15 focused tests, full serial C/D suites (143 tests each), 172 Python tests
(two existing skips), 95 local Markdown paths/anchors and formatting checks
(only the touched range in legacy MainWindow). The frozen product capture uses an explicitly labelled
external entry with unchanged parsers, ArticleMaker and ArticleView objects;
only the entry object is replaced in a separate executable. It does not claim
an untouched MainWindow launch. Qt 6 uses actual Core-to-MainWindow publication.

Native captures retain full pages at DPR 1 and zoom 1. Both engines measure
body widths 304/784/1584 and normal paragraph line counts 2/1/1 at the three
viewports; pre stays three lines and tokens stay unbroken. They do not have
identical native glyph metrics: WebKit pre line advance is 15px and long-line
width 1680px, versus Chromium 14px and 1728.28125px, with the same declared 12px
monospace and recorded Courier New default. The wide normal paragraph measures
402px versus 403.9375px, and the unbroken normal token 2880px versus
2813.796875px. These renderer differences are recorded separately
from matching CSS and wrapping contracts; no fixed native line-height override
is introduced. Frozen template margin collapse places body y at 0, and its
From/header markup and Dictd fallback path title differ visibly from Qt 6.
Whole-page positions, heights and pixels are not asserted equal or masked.

Light/dark tests use process-local Blink `preferredColorScheme=1/0` and assert
actual media preference on every cold/warm page. Do not use `--force-dark-mode`:
Qt maps that switch to the distinct ForceDarkMode paint setting, which can
invert pixels while computed styles remain unchanged. Native acceptance also
asserts a blank canvas pixel is `#fefdeb` and measured plain-paragraph raster
contains black text; raw images and counts are retained. This tests dark media
preference, not algorithmic page inversion or a new product theme policy.

R9.8, dictionary headings/templates, complete dictionary-specific CSS, themes,
user/addon/print precedence, generic table/media/link styles, real-corpus visual
matrix, Linux/macOS acceptance and formal cutover stay open. Compilation and
this bounded Windows correction cannot close them.
