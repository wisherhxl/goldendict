# Empty Lookup Target Decision

## Authority and scope

EL-01 is an approved CRD decision amendment dated 2026-09-09 under the
[Qt 6 product baseline](qt6-product-baseline-crd.md), specifically
`CRD-LOOKUP-006`, `CRD-LOOKUP-003`, `CRD-DICT-003` and `CRD-COMPAT-001`.
The product owner approved rejecting empty targets after the R3.7g N1
discussion. This record resolves N1; it does not redefine the product baseline.

A Dictd reference target that is empty after normal legacy normalization,
including a whitespace-only target, is inert. Do not issue a lookup or
navigation, replace the page, stop audio, close article search, record history,
or create, activate or alter a tab. The renderer must preserve the reference's
inert content and surrounding layout while omitting its actionable href.
Keep current empty-target rejection; do not add an empty-query API, special
compatibility navigation action, or empty lookup/session state.

This is a narrow approved divergence from observable Qt 5 behavior, chosen to
avoid disruptive navigation with no lookup target. It does not authorize
rejecting other nonempty targets, changing legacy target normalization,
loosening URL decoding or sanitization, or changing resource/security policy.
R3.7g N2 (nonempty targets containing retained C0/DEL characters) is independent
and technically resolved by the [lookup URL correction](lookup-target-url-parity.md)
under the existing parity requirements. Ordinary blank/welcome pages remain
outside this decision.

The separately approved [N3 decision](malformed-reference-decision.md) covers
failed complete generated href rewrites and every reconstructed anchor from
those origins. Its classification precedes browser URL interpretation and must
not be conflated with EL-01's normalized-empty target rule.

The separately approved [N4 decision](dot-only-reference-decision.md) makes
successfully rewritten targets exactly `.` or `..` after legacy normalization
inert as well. These targets are nonempty before browser interpretation and
are not EL-01. N4 alone authorizes this additional narrow divergence; it does
not change EL-01, shared URL validation or other nonempty-reference behavior.

## Frozen evidence and current implementation

Qt 5 reference: `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
In `dictdfiles.cc:349-394`, phonetic substitution precedes brace-reference
substitution. Link-target cleanup replaces generated tags and `&nbsp;` with
spaces and calls `QString::simplified()` before percent encoding. Evaluate
emptiness at that normalized-target boundary, not on arbitrary source bytes.
`{}` is not matched by the legacy nonempty brace pattern; `{ }` is matched but
normalizes to empty. Literal escaped source entities must not be newly decoded.

The workspace's `evidence/r3.7g-design-probe/probe.cpp` compiles frozen
`htmlescape.cc` and reproduces those sequential transformations using actual
Qt 5 WebKit. Its 24-case `observations.json` records DOM, full-text extraction
and delegated `linkClicked` signals. Both `{ }` and the tab/vertical-tab/form-feed
whitespace case emit `gdlookup://localhost/`. This is a rendering/click probe,
not an end-to-end observation of every application state change.

Frozen `articleview.cc:1229` forwards the empty path target to `showDefinition`;
the latter at line 411 stops audio, emits history intent, closes article search
and loads an empty-word request. The separate `blank=1` route in
`articleview.cc:368` and `article_netmgr.cc:402` is not that empty-word request.
Historical commit `15d9779a4767a4dd2a15114bf0256fe0e66dcf9a` adds generic link
cleanup; it supplies no evidence that empty-target navigation was an intended
feature. The exception remains explicit regardless of that historical intent.

At Qt 6 base `6bb463881839bbff94f3ed18d2575f5b34e494cb`,
`modules/core/src/article/internal_url.cc` rejects empty decoded payloads;
`application/desktop_facade.cc` resolves only validated internal URLs;
`application/article_tab_session.cc` requires nonempty lookup/internal-link
queries; and `application/dictionary_service.cc` rejects empty queries (the
latter three paths are relative to `modules/core/src/`). These are source
observations, not proof of future Dictd producer or no-side-effect acceptance.
At that base R3.7f preserved literal inline markers. The later
[R3.7g.1](dictd-full-text-extraction-parity.md) and
[R3.7g.2](dictd-inline-display-parity.md) records own their separate runtime
implementation and verification status.

## Development readiness and acceptance

Readiness: **Ready for EL-01's documentation-only decision unit**. The approved
outcome, exception boundary and observable acceptance are unambiguous. No
production change is part of this unit. The [N3 record](malformed-reference-decision.md)
identifies the separate R3.7g technical readiness and delivery sequence;
renderer implementation and full state acceptance were not part of this
documentation decision. N2 has a separate bounded implementation and acceptance
record linked above.

Architecture is unchanged under the
[Shared-Library And GUI Boundary](project-design-rules.md#shared-library-and-gui-boundary):
the private Dictd renderer owns target normalization and inert markup; private
Core URL/application components retain validation and navigation ownership;
Widgets/WebEngine presents the result and forwards valid intent. Existing
composition and validation boundaries suffice; no new pattern, module,
dependency or installed interface is justified. Dictionary files, configuration,
resources and user-owned data are unchanged, with no migration needed for this
decision unit. Any later full-text cache update belongs to the R3.7g renderer
delivery. Rollback of this requirement requires a separately approved amendment.

The future R3.7g delivery must provide:

- Generated producer tests for empty/whitespace-only and generated-markup
  targets that normalize to empty, showing preserved inert text/layout and
  absence of an actionable href. Include ordinary nonempty references and
  literal `{}`/escaped-entity controls to prove the exception stays narrow.
- Focused URL/facade rejection and real dispatch tests that attempt activation
  and compare page identity, query/navigation dispatch counts, active audio,
  open article search, history and tab/session snapshots before and after.
  Rejection must precede all those side effects. A build or string assertion
  alone cannot prove this contract.
- Actual Qt 5/Qt 6 generated-reference comparison recording the approved
  empty-target difference separately from remaining parity gaps. Verify on
  Windows and Linux for their release gates; the same requirement applies to
  macOS restoration. Current Windows probe evidence does not imply Linux or
  macOS execution.

For this decision unit, verification is Markdown link/path validation,
`git diff --check`, and consistency review of the CRD, this record and the
[R3.7 gap status](qt6-baseline-gap-audit.md). No product rebuild is required
because only documentation changes. External evidence is retained under the
workspace's `evidence/empty-target-decision-20260909/`. A fresh completion audit
must bind the staged documentation to its base commit and Tree ID before
commit; separate integration verification and audit remain required.

The bounded Dictd Windows producer and complete inert-activation state checks
are now verified by [R3.7g.2](dictd-inline-display-parity.md), including the
visible actual-NBSP empty reference and each visible reconstructed clone.
That focused record owns the runtime delivery/audit status. This does not
close other producers, Linux/macOS, full Dictd parity or baseline cutover.
