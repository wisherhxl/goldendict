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
