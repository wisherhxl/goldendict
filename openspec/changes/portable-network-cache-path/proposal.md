## Why

P1 is the separately user-approved 2026-09-19 portable Network cache-path contract
completion. W3.2 discovered that portable configuration still selected the daily
Windows Network cache, including destructive disabled-cache initialization.
This is a bounded requirement change under CRD-DICT-004 and the existing Network
cache ownership contract, not a pure test migration or W3.3.

## What Changes

- Resolve Network cache root in the existing application path/composition layer:
  existing explicit injection first, portable data root/cache next, unchanged
  platform default otherwise. Network still appends qt-network-http exactly once.
- Select once before Network Prepare side effects and reuse the value for startup,
  configuration reapplication and recovery. Never fall back to daily cache on failure.
- Preserve Network's documented uncached degradation, fixed-directory lease,
  preparation/publication/cleanup semantics and all W1/W2/W3 behavior.
- Add safe regression and ordinary portable startup evidence separately from W3.2.

## Capabilities

### New Capabilities

- `portable-network-cache`: P1 application selection of the existing injected cache root.

### Modified Capabilities

None. No existing OpenSpec specs were present. This delta supplements the canonical
CRD and architecture contract; it does not rewrite Accepted ADRs or prior evidence.

## Impact

App-private legacy_configuration_location helper, main.cpp's single root selection,
focused test-only target, README/architecture clarification and this P1 change.
No Network public API, cleanup implementation, persistence format, index default,
WebEngine policy, user-directory migration, generic path framework or CLI addition.
Base d196d6d7c02f17e2ba91e58d34620d4f9d0c4803 in the user-selected Qt6 worktree.
No Qt5 write, reset, prior-commit rewrite, merge, push or automatic next work item.
