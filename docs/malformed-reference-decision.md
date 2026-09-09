# Malformed Generated Dictd Reference Decision

## Authority and scope

N3 is an approved CRD decision amendment dated 2026-09-09 under the
[Qt 6 product baseline](qt6-product-baseline-crd.md), specifically
`CRD-LOOKUP-007`, `CRD-LOOKUP-003`, `CRD-DICT-003` and `CRD-COMPAT-001`.
The product owner selected alternative 1: preserve the observed Qt 5 text,
layout and styles of malformed generated Dictd references, but disable their
activation. This resolves R3.7g issue N3 without creating a new baseline.

The exception applies only when the frozen href-cleanup expression cannot
rewrite a reference's complete generated opening tag after the sequential
phonetic and brace-reference substitutions. Preserve private origin attribution
for each generated reference, including every anchor reconstructed from that
origin across span or div boundaries. Omit actionable hrefs from all affected
anchors and provide no alternative activation action. Do not infer this class
from unusual characters, the parsed URL, or visible text alone.

Preserve the Qt 5 browser-observed odd text, block layout, direction, phonetic
style and generated backslashes. Do not repair intended reference semantics or
silently remove duplicated display fragments. Attempted activation must issue
no query or navigation, replace no page, stop no audio, close no article search,
record no history, and create, activate or alter no tab/session state.

Except for the separately approved [N4 dot-only rule](dot-only-reference-decision.md),
successfully rewritten nonempty references retain normal legacy normalization
and exact lookup behavior through the [N2 URL boundary](lookup-target-url-parity.md),
including escaped entities, literal percent/hash/slash, residual admitted
controls, neutral cross-line references and overlapping span closures. The
[EL-01 normalized-empty rule](empty-lookup-target-decision.md) remains separate;
N3 must not classify an empty decoded browser path as an EL-01 target. Literal
`{}` remains unmatched by the frozen brace pattern.

N4 applies only to successful rewrites whose complete legacy-normalized target
is exactly `.` or `..`, before URL interpretation. It is not a failed N3 origin
or an originally normalized-empty EL-01 target. Other nonempty dot-containing
targets remain compatible. The later N4 record updates readiness for resuming
R3.7g.2; the historical N3 readiness and acceptance scope below remain intact.

This deliberate divergence avoids activating renderer-generated broken hrefs
while retaining ordinary reference compatibility. It does not classify the
dictionary source as invalid. Semantic repair, fragment/query navigation
expansion, new public APIs, generalized HTML recovery, or relaxed resource,
URL-validation or sanitizer policy are outside scope.

## Frozen evidence

Reference commit: `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
`dictdfiles.cc:349-394` applies phonetics, then references, then the
case-insensitive cleanup expression `<a href="gdlookup://localhost/([^"]*)">`.
Only matching complete opening tags receive tag-to-space replacement,
`&nbsp;` replacement, `QString::simplified()` and percent encoding. Generated
quoted span classes or div directions inside the target can prevent that match.
Source text is escaped by `Html::preformat` before these transformations.

The workspace retains two actual Qt 5 WebKit probes, each with 24 cases and
its own `probe.cpp` and `observations.json`:
`evidence/r3.7g-design-probe/` and `evidence/r3.7g-design-probe-extra/`.
They compile frozen `htmlescape.cc`, reproduce the sequential substitutions,
and record browser DOM/text/anchors, delegated clicks and independent frozen
full-text extraction. Neither probe loads product CSS: these are DOM/click
observations, not style acceptance, a real Dictd corpus or full application-state
acceptance.

For example, `{\phonetic\}` displays `phonetic">phonetic`; N3 preserves that
odd text and its phonetic span. `{a#b\c\}` emits path `/a` and fragment
`b<span class=` in the extra probe. Frozen `articleview.cc:1231-1269` routes
fragment-bearing URLs to browser navigation and otherwise consumes lookup
paths and `dict`/`gdanchor` query context. Other extra cases show dot-segment
collapse, raw-tab removal, and `%00`/`%09` decoding into NUL/TAB. These artifacts
are evidence for the N3 exception, not authorization to broaden N2 validation.

Conversely, `{first` followed by a newline and `second}` reconstructs two
ordinary active anchors with the same normalized target. `\before {middle\ after}`
also retains two successfully rewritten active anchors. A blanket rule making
all reconstructed anchors or all phonetic/reference combinations inert would
violate the decision. Frozen `article-style.css:577-597` supplies phonetic
backslashes/styles and blue `a:link` text. Removing href also changes browser
link styling, including underlining; DOM text alone proves neither appearance
nor the phonetic pseudo-elements.

## Readiness and acceptance

Readiness: **Ready for N3's documentation-only decision unit**. The approved
exception, source evidence and acceptance boundaries are explicit; no product
or architecture choice remains for this documentation unit. The separate
read-only R3.7g review is **Ready after N3 canonical decision delivery**, recorded
in the workspace's `evidence/r3.7g-readiness-20260909.md`. Delivery proceeds with
independent full-text extraction/cache migration (R3.7g.1), then the coupled
phonetic/reference display and activation unit (R3.7g.2); each requires its own
verification and audits. This decision unit implements neither and claims no
runtime acceptance.

The existing [Shared-Library And GUI Boundary](project-design-rules.md#shared-library-and-gui-boundary)
continues to apply. Private Core Dictd rendering owns generated-reference origin
and normalization; the strict article sanitizer and private Core URL/application
components retain their existing responsibilities. Widgets/WebEngine presents
the result and forwards valid intent. A private sidecar must retain origin and
rewrite eligibility through renderer reconstruction before inert output reaches
the browser; no installed interface, module, dependency or new design pattern
is needed. A private inert Dictd class with narrowly scoped CSS may preserve
the affected reference appearance when href removal changes it; never use a
fake actionable href for styling. This is a presentation adaptation within the
existing ownership boundaries, not a resource-policy change. Existing bounds,
cancellation and failure contracts still apply.

The renderer delivery must demonstrate:

- A 48-case differential comparison against both frozen probes using actual
  Qt 5 WebKit and Qt 6 WebEngine DOM/text/anchor structure and matched rendered
  layout/styles under actual product CSS. Measure computed styles and
  pseudo-elements, including link color/underline and phonetic backslashes.
  Record the N3 href/action difference and EL-01 difference separately. Raw HTML
  equality and the existing probes alone are insufficient for style acceptance.
- Provenance-based negative coverage for every failed rewrite and all its
  reconstructed anchors, including nested phonetics, direction-bearing lines,
  fragment/query/dot/percent/control cases; preserve the observed odd display.
- Successful nonempty controls with exact targets, including ordinary,
  escaped-entity, percent/hash/slash, admitted C0/DEL, neutral cross-line and
  overlapping-span references. Include unmatched markers, Unicode, literal
  `{}`, whitespace-only and generated-normalized-empty EL-01 controls.
- Actual activation attempts on affected rendered content and each reconstructed
  anchor, with evidence that input reached the intended target. Compare query
  and navigation counts, page identity, active audio, open article search,
  history and complete tab/session snapshots before and after; test normal and
  supported new-tab activation paths. Absence of href text alone is insufficient.
- Independent frozen full-text comparison, preserving its distinction from
  browser-normalized display; generated-index migration/restart, stable source
  identity, bounds, cancellation and recoverable failure checks in the full-text
  unit, retained through the later display unit. Original dictionary data and
  user-owned state remain intact.

No configuration, dictionary, resource or generated-index migration occurs in
this documentation unit. A later renderer full-text semantic stamp/rebuild is
separate implementation work. Reversing this product decision requires a
separately approved amendment; product failures retain existing recoverable
failure handling rather than fallback activation of malformed links.

Verify this unit with Markdown path/anchor validation, `git diff --check` and
consistency review of the CRD and focused status records; no product rebuild is
required. External verification/audit evidence belongs under the workspace's
`evidence/n3-malformed-reference-20260909/`. Independent completion and separate
integration audits remain required. Windows probe evidence does not establish
Linux or macOS acceptance; the requirement applies to both release-gating
platforms and macOS restoration. R3.7g, full Dictd parity and cutover remain open.
