## Why

W1/A1 in draft-plan.md identifies divergent normal/prepared article lifecycle
wiring. The user approved W1 implementation only on 2026-09-12, including real
binding-path red/green evidence and independent review. A2–A6 remain Draft.

## What Changes

- Conform existing CRD-SHELL-003 and CRD-LOOKUP-003/004 lifecycle behavior by
  sharing necessary article initialization, bindings and owner event handling.
- Preserve candidate gating, QObject ownership, background tabs, stale callback
  guards and the published operation allowlist.
- Add a focused test target using the real MainWindow implementation; no new
  production scenario methods, request ownership or public Core contracts.

## Capabilities

### New Capabilities

None. This is an approved minor correction; skip_specs records conformance.

### Modified Capabilities

None. No baseline or accepted design is replaced.

## Impact

MainWindow article initialization/binding code, one test-only target and W1
OpenSpec records. The user selects the existing Qt6 worktree at 3f3f2bf4 and
prohibits automatic merge/push. Preserve the original draft-plan.md unchanged.

## W2 follow-on selection (2026-09-12)

The user accepted W1 and approved W2/A5 only from candidate 693c3e8ccb65a99fea8754476341690178f93fa2.
Fully construct Core/Network publication objects before the durable decision,
preserving private ownership, publication order, failure/recovery semantics and
W1 implementation. See w2-design.md. Prior W1 text and evidence remain historical;
A2/A3/A4/A6 are not selected. No merge, push or automatic W3 execution.

## W3.1 follow-on selection

The user accepted W2 and approved a complete current inventory plus one bounded
test-family migration. See w3-1-design.md. Only the full-text dictionary projection
family is selected; W3/A4 remains in progress and W4/W5/W6 remain unselected.

## W3.2 bounded follow-on

The user approved DictionaryBarSmoke within the established migration pattern.
See w3-2-status.md for the locked scope and revised startup isolation acceptance.
W3/A4 remains in progress; no other family or product path change is selected.
