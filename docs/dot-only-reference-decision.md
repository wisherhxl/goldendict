# Dot-Only Dictd Reference Decision

## Authority and exact boundary

N4 is an approved CRD decision amendment dated 2026-09-09 under the
[Qt 6 product baseline](qt6-product-baseline-crd.md), specifically
`CRD-LOOKUP-008`, `CRD-LOOKUP-003`, `CRD-DICT-003` and `CRD-COMPAT-001`.
The product owner explicitly approved preserving dot-only reference appearance
while disabling activation, avoiding an empty-query navigation. The approval
and read-only readiness review are retained in the workspace's
`evidence/r3.7g2-issues-20260909.md`. This extends the approved reference
exceptions without redefining the baseline or changing architecture.

The predicate is exact: a generated Dictd reference successfully passes the
frozen complete-opening-tag href rewrite, and its complete target after normal
legacy target normalization is exactly `.` or `..`. Evaluate this before URL
encoding or browser URL interpretation. Whitespace-normalized equivalents are
included. Failed rewrites remain [N3](malformed-reference-decision.md);
successfully normalized-empty targets remain [EL-01](empty-lookup-target-decision.md).
These are distinct causes of inert output.

For N4, preserve the observed text, link color and underline, phonetic styling
and pseudo-elements where present, direction, block/inline layout and every
reconstructed display fragment. Omit actionable hrefs from every anchor derived
from the affected origin, including reconstructed anchors. Provide no substitute
activation action. Normal or supported new-tab activation attempts must issue
no query or navigation, replace no page, stop no audio, close no article search,
record no history, and create, activate or alter no tab/session state. Do not
turn these references into an empty lookup target or a literal dot query.

All other successfully rewritten nonempty references retain their existing
legacy normalization and exact lookup behavior through the
[N2 URL boundary](lookup-target-url-parity.md). In particular, `../word`, `/word`,
`...`, literal percent/entity text and other nonempty dot-containing targets
are not N4. Do not decode escaped source entities or percent literals again,
normalize arbitrary browser paths, suppress a target merely because it contains
a dot, or infer the predicate from visible text or an empty decoded URL alone.
Literal `{}` remains unmatched by the frozen brace pattern.

## Frozen evidence and diagnostic limits

Reference commit: `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
`dictdfiles.cc:349-394` applies phonetic substitution before brace references.
For a complete matching generated opening tag, cleanup replaces generated tags
and `&nbsp;` with spaces and uses `QString::simplified()` before percent encoding.
Both `{.}` and `{..}` successfully rewrite with nonempty normalized targets.
They therefore fall outside both N3 and the original EL-01 predicate.

The actual frozen Qt 5 WebKit styled probe retains the separate four-row
`evidence/r3.7g2-qt5-styled-20260909/supplement.json` and
`supplement-01.png` through `supplement-04.png`. It uses frozen product CSS;
computed anchor styles record blue `rgb(0, 0, 255)` underlined text and retain
the dot/dot-dot content and layout. The supplement is additional evidence,
not a replacement for the original 48-row styled comparison.

| Source reference | Generated Qt 5 href | WebKit delegated click / decoded path |
| --- | --- | --- |
| `{.}` | `gdlookup://localhost/.` | `gdlookup://localhost/` / `/` |
| `{..}` | `gdlookup://localhost/..` | `gdlookup://localhost/` / `/` |
| `{../word}` | `gdlookup://localhost/..%2Fword` | `gdlookup://localhost/..%2Fword` / `/../word` |
| `{/word}` | `gdlookup://localhost/%2Fword` | `gdlookup://localhost/%2Fword` / `//word` |

Frozen `articleview.cc:1231-1269` routes fragment-free `gdlookup` paths through
`showDefinition(url.path().mid(1), ...)`. Thus the two dot-only cases take the
empty-query route; the slash controls retain `../word` and `/word` respectively.
The `showDefinition` source at line 411 stops audio, emits history intent,
closes article search and loads the requested word. This is a source-based
routing conclusion, not a probe observation of every MainWindow state effect.
N4 deliberately diverges from that empty-query route to avoid its disruption.

The actual Qt 6 pointer diagnostic is
`evidence/r3.7g2-20260909/native-second/dot-supplement.json`, using the real
`ArticleSchemeHandler`. All four DOM hit checks pass. Raw
`goldendict://lookup/.` and `goldendict://lookup/..` become
`goldendict://lookup/`, emit zero `LookupRequested` signals and leave the original
page. Each slash control emits one lookup with its exact nonempty target.
This confirms the compatibility corner and existing empty-payload rejection;
it does not prove href omission, styled N4 acceptance or complete application
state preservation. The earlier diagnostic without the scheme handler is not
valid activation evidence and must not be counted as acceptance.

## Development readiness and delivery units

Readiness: **Ready** (2026-09-09). The product owner's approval and the root
read-only review precede any N4 production implementation. The exact predicate,
approved divergence, architecture boundary, acceptance and delivery sequence
are resolved; no additional product or architecture decision is needed.

The existing [Shared-Library And GUI Boundary](project-design-rules.md#shared-library-and-gui-boundary)
applies. Private Core Dictd rendering owns successful target normalization,
generated-reference origin and reconstruction. Extend the approved R3.7g.2
candidate's private `ReferenceHref` normalization decision to omit href for the
exact N4 predicate, retaining origin through reconstruction and reusing that
candidate's private inert Dictd class/CSS for appearance. At decision readiness,
these helpers and CSS existed in the paused uncommitted R3.7g.2 candidate, not in the integrated
`0659a4f12494f693ba28980c18e9ebcba5811386` baseline. Strict article sanitization and shared URL validation
retain their contracts; private application components own navigation and
Widgets/WebEngine presents content and forwards valid intent. This uses the
existing cohesive renderer and validation pattern; no new abstraction, module,
dependency, public API, navigation kind or empty-query command is justified.

1. Deliver this confirmed canonical amendment as an isolated documentation
   unit, with Markdown verification, a fresh independent completion audit,
   audited-index commit and normal task push, then separate integration
   verification/audit under the [integration contract](agent-workflow.md#integration-contract).
   This unit changes no production code and claims no runtime acceptance.
2. Resume the paused R3.7g.2 display/activation delivery on that integrated
   decision base, preserve its existing uncommitted work, implement N4 within
   the approved private boundary, and verify the coupled acceptance below.
   Its own completion and integration audits remain mandatory.

Dictionary files, settings, resources, installed interfaces and user-owned data
are unchanged. N4 needs no configuration or generated-index migration. The
independent pre-href [R3.7g.1 full-text extraction and semantic cache contract](dictd-full-text-extraction-parity.md)
must remain unchanged, including dot text in full-text output. Existing bounds,
cancellation and recoverable failure behavior remain in force; failures never
authorize fallback activation. Reversing this decision requires a separately
approved amendment; a runtime rollback follows the audited revert workflow.

## Required acceptance

The later renderer delivery must provide:

- Generated producer checks for exact dot/dot-dot and whitespace-normalized
  equivalents, preserved inert content and no actionable href on any anchor
  reconstructed from each affected origin. Keep N3 and EL-01 coverage distinct.
- Actual Qt 5/Qt 6 styled comparison for the four supplemental cases, with
  matched CSS, layout and computed link styles. Retain all original 48 styled
  comparisons, including phonetic pseudo-elements, odd text and reconstructed
  fragments; record N4's approved action difference separately from N3/EL-01.
- Actual normal and supported new-tab pointer attempts reaching each affected
  rendered anchor, followed by settled complete application-state snapshots.
  Compare query/navigation dispatch, page identity, active audio, open article
  search, history and complete tab/session state before and after. A missing
  href or zero lookup signal alone is insufficient.
- Positive exact-target controls for ordinary nonempty references, `../word`,
  `/word`, `...`, percent/entity literals, Unicode, admitted C0/DEL, neutral
  cross-line references and overlapping spans. Retain all 111 frozen pre-href
  full-text comparisons and cache/restart checks from R3.7g.1.
- The documented Conan Release build and relevant producer, URL/facade,
  application-state and native renderer checks, followed by the full serial
  CTest and Python regression checks in the owning worktree. Preserve portable
  behavior; record Windows execution first and Linux/macOS evidence separately.

The requirements apply to Linux and Windows release gates and macOS restoration.
Existing Windows probes establish no Linux/macOS execution. The bounded
Windows R3.7g.2 producer, styled and complete activation-state checks are now
verified in the [focused runtime record](dictd-inline-display-parity.md), which
owns delivery/audit status. Parent R3.7, complete Dictd parity, Linux/macOS,
R9.8 normal-article visuals and baseline cutover remain open.

For this documentation unit, run Markdown link/path/anchor validation,
`git diff --check` and consistency review of the CRD, related decisions and gap
status. No product rebuild is required. External verification and audit records
belong in `evidence/n4-dot-only-decision-20260909/` in the workspace. Freeze the
complete staged documentation and record its exact Base Commit ID and Tree ID
before requesting the independent completion audit.
