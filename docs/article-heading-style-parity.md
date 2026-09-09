# Normal Dictionary Heading Box and Spacing

## Requirement and proposed boundary

Ready on 2026-09-10; workspace evidence `dictionary-headings-20260910/root-impact-review.md`.
Minor correction under CRD-SHELL-005,
CRD-LOOKUP-003, CRD-RES-002/004 and CRD Sections 3/6/10.2; bounded R9.8 work.
Base: `8adb1a735da9ee423eacd50e9dae0c8bd6b72158`; branch
`fix/qt6-dictionary-headings`, workspace `worktrees/fix-qt6-dictionary-headings`.
Remote fetch, clean primary/base equality and submodule setup passed. Qt 5 is
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.

Outcome: the default dotted heading box, 14px bold type, padding, heading
margins and enclosing result spacing, including a full-width collapsed summary.
Current wording, grouping and disclosure differences remain explicit gaps.

An enclosing one-heading-per-dictionary unit is not selected here. Frozen
`article_maker.cc:550-551,618-738` emits one container for the complete
dictionary request, including error-only responses. Its collapse metric is
complete HTML converted by QTextDocumentFragment to UTF-16 text length, with
optional-part removal, strict greater-than and sole-dictionary exemptions.
Current Core counts individual DTO Unicode scalars; summing them does not
restore HTML/block/object/supplementary-character semantics. Rich-text
extraction, a Core QtGui exception, grouping/navigation and translation need
a separately concrete successor design.

## Frozen evidence and private style mapping

Frozen `article_maker.cc:693-716` emits the From/name/arrow heading and body;
`article-style.css:35-69` defines the default box and outer geometry:

| Frozen owner | Existing Qt 6 target | Required declarations |
| --- | --- | --- |
| `.gddictname` | Direct expanded result `h2`; direct collapsed `summary` | Dotted black 1px border; padding 0.2em, left 0.5em; margin-top 1.2em, bottom 9px; bold 14px type |
| `.gdarticle` | `body > section.gd-dictionary-result` | Block display; top padding 1px; top margin -8px; bottom margin 8px; `#fefdeb` background; normal font style |
| `.gdarticle:after` | The same top-level result's `:after` | Empty block, zero height, clear both |

Only private `article_document.cc` changes. Keep its embedded-style convention;
packaged frozen `:/article-style.css` is a test oracle, never a Core dependency.
Retain GPL provenance and existing Core/ArticleView assembly/interaction owners;
no resource, dependency, module, public interface or pattern is added.

Replace the solid separator/first-child exception with the frozen outer rules.
Use direct top-level result selectors and these controlled heading paths:

```css
body > section.gd-dictionary-result > h2
body > section.gd-dictionary-result > details.gd-collapsed-article > summary
```

Apply box declarations to both, with zero horizontal margins and normal line
height. Summary's direct h2 stays inline, inherits font size/weight and has
zero margin/padding/border. Summary retains list-item/outside marker/pointer.
Remove the broad descendant h2 rule: nested body h2 must not gain the box.
Private-document fixtures can contain h2 although ordinary sanitization omits it.

No substitute arrow/spacer, forced line height, changed marker, click handler
or resource route is added. Qt 5's 16px arrow affects line height and wrapping;
retain those differences in raw evidence. Compare mapped declarations and box
edges separately from outstanding content/interaction differences.

## Geometry and compatibility

First/subsequent results share the rule. Top padding prevents heading-margin
collapse through the container; the -8px outer margin affects body-margin and
later-result positions. Measure adjacent margins, y positions and floats.
Existing inner `.gd-article{margin-bottom:.75rem}` remains
unchanged: at the current 16px root it contributes 12px per assembled entry.
Do not reuse the `.gd-article` class: it already names individual entries.
Inner spacing, repeated headings and body wrappers prevent whole-page equality.

The outside marker may extend left of the border. Measure marker/summary/h2/text
rectangles, hit ownership, clipping and overflow; click text/padding,
reopen and select. Baseline native evidence confirms the outside marker is
already inaccessible: x0/2/4/7 hit HTML while the summary begins at x8. Retain
that clipping and compare before/after; do not claim a successful marker click.
Qt 5 collapses only via its arrow and expands from anywhere
in its collapsed header. Retained native-summary behavior is an open product
gap, not a CRD native-rendering tolerance.

Unchanged composer/ArticleView compatibility (regression checks):

- Entry order, one result per entry, original navigation indices (including
  dictionary two at index 2), contiguous-ID selection, pointer/context state,
  invalid-index no-op, page/tab replacement and stale-context safeguards.
- Empty IDs as independent selection targets; noncontiguous IDs, conflicting
  metadata, empty names and name-or-ID fallback exactly as currently composed.
  Names and IDs remain escaped inert data; no new identity policy is inferred.
- Existing automatic collapse and sole-entry exemption, optional-part policy,
  original-index optional-control IDs, print expansion and persisted defaults.
  These invariants are regression checks, not proof of full Qt 5 collapse.
- Byte-identical plain-text DTOs; no From/translation consumption; unchanged
  app-less behavior, translator lifecycle and platform locale enablement.
- The 16 MiB composed bound, exact-prefix body extraction, escaped fallback,
  sanitizer policy, script-denying CSP and typed resource/navigation routes.

The prefix necessarily changes. As established in
[the base-style persistence trace](article-base-style-parity.md#composition-and-persistence-compatibility),
composition is same-runtime, tabs replay lookup, and caches do not persist
composed HTML. Retain prior/foreign-prefix rejection; no cache migration or
widened admission follows.

## Acceptance and file budget

Expected production change: about 20-30 lines in `article_document.cc`.
Reuse `article_base_style_test.cpp` and its existing MainWindow executable,
fixture helpers and measured publication path; do not duplicate its application
source list or introduce a general harness. The Ready amendment allows about
245 test lines plus native-control evidence fields. They cover real cold/warm
MainWindow publication and public-facade/ArticleView lookup of two generated
StarDict sources (three physical records). Retain body/pre, dark-media, source/cache
integrity and capture assertions.
Allow 20-40 Core test lines for prior-prefix/nested-body coverage as needed.
No CMake/public-header changes; update this record and short canonical links.

Portable assertions at actual 320, 800 and 1600 CSS-pixel viewports:

1. Compare mapped stylesheet declarations with frozen packaged selectors.
   Record border width/style/color, computed margins/padding, font size/weight,
   line-height, outer background/style/display and float-clearing pseudo-element.
2. Expanded heading and collapsed summary have the enclosing result's width
   (viewport minus 16px for normal body layout) and aligned left/right edges.
   Verify default first and later results, long wrapped names, escaped text,
   and the nested summary h2 has no independent box. No fixed glyph-height or
   whole-page y equality is asserted across engines.
3. Verify actual selection range stays inside the same result(s) after the
   spacing change. Navigate original indices 0, 1 and 2; exercise invalid
   indices, click/context selection, summary closing/reopening, tabs and page
   replacement with existing suites. Verify marker/text/padding hit ownership
   and the unchanged inaccessible marker region at the narrow viewport.
4. Verify nested body headings do not gain the trusted border or padding,
   optional IDs stay distinct, plain-text output is unchanged and initial
   collapse/print policy is unchanged. Use existing Core/selection suites for
   policy cases. MainWindow cold/warm source/cache identity, prefix and CSP
   remain required.

Native Windows paired evidence uses the unchanged Qt 5 parsers, ArticleMaker
and ArticleView through an explicitly identified external entry executable,
as in base-style acceptance. New external tools/evidence belong under workspace
`evidence/dictionary-headings-20260910`; prior evidence/sources/objects stay
unchanged. Qt 6 uses its own Conan launcher and actual MainWindow publication.
Record English/Fusion/fonts/hashes, DPR 1, zoom 1, no custom theme, matched
viewports/source bytes and full unmasked PNG/JSON/provenance. Compare box edges
separately from From/arrow/line-box/repeated-heading/body offsets. Include first,
subsequent, collapsed/reopened and long-name/pointer cases.

After Ready, configure/build with owning Conan generators and VS 2026 MSVC
14.44 as documented in `build.md`. The focused Release gate is:

```powershell
./run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release
./run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(article_base_style.*test|article_assembler_test|article_composer_test|application_service_test|article_selection_test|article_tabs_test|goldendict_article_tabs_smoke|goldendict_dictionary_context_navigation_smoke|goldendict_articles_preferences_smoke|goldendict_optional_parts_preferences_smoke|dictd_inline_display_test|article_page_.*routing_test)$' --output-on-failure
```

Then run cumulative serial Release CTest with separate fresh short C/D TEMP
roots, Python script tests, local Markdown links, formatting and `git diff
--check`. Stage only the coherent delivery for a fresh no-history completion
audit; original developer commit/push and separate integration verification
and audit remain required. No install, package or dependency change is planned.

Enclosing grouping, `From ` translation, exact automatic threshold extraction,
sole-dictionary expansion, manual arrows/tooltips/event order, icon provenance,
error headers, complete body spacing, theme/user/addon/print precedence,
real-corpus visual matrix, Linux/macOS acceptance and full R9.8 remain open.

## Bounded acceptance result

Implementation and Windows verification are complete; independent completion
and integration audits are separate gates. Workspace
`evidence/dictionary-headings-20260910/verification.md` records the owning
Conan/MSVC Release build, focused 15/15 tests, serial C/D 143/143 runs and
auxiliary checks. `final-native-manifest.json` records 18 paired focused rows
and 12 MainWindow cold/warm rows with exact source, PNG, tool, font and binary
hashes. The frozen default declarations and header box edges match; full-page
positions, word wrapping and missing controls do not claim parity.

The focused fixture reads identical generated StarDict bytes in both engines.
Qt 5 retains two enclosing headers and Qt 6 retains three entry headers.
Qt 6 text/padding clicks and existing exact selection/composer regression suites
pass. Qt 5 v3 reopened captures set state through gdExpandArticle; independent
`qt5-pointer-v2`/`qt5-pointer-manifest.json` additionally prove four real pointer
transitions with exit 0, using separate tools and unchanged frozen objects.
