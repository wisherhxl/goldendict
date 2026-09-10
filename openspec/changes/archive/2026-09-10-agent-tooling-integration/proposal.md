## Why

Qt 6 task worktrees need versioned AI tooling that follows their own source tree
and preserves existing migration authority. Configuration held only in a frozen
Qt 5 checkout cannot be inherited through the Qt 6 development baseline.

## What Changes

- Add Codex OpenSpec skills-only workflows, including Verify, and migration-aware
  context, artifact rules, and apply/archive guidance.
- Add the OpenSpec WHAT/WHY and Superpowers HOW boundary to existing agent rules;
  approved OpenSpec discovery is not repeated, and canonical requirements remain
  references rather than duplicated product specs.
- Add portable Serena project settings, ignored optional machine overrides, exact
  worktree activation, and local compilation-database ownership guidance.
- Preserve existing delivery/audit/integration rules and all product code.

## Capabilities

### New Capabilities

None at product-spec level. This is a tooling/workflow change; `skip_specs: true`
avoids introducing a parallel product requirements source.

### Modified Capabilities

None. The approved product requirements in
[the product CRD](../../../../docs/qt6-product-baseline-crd.md),
[migration](../../../../docs/migration.md), [parity](../../../../docs/feature-parity.md),
and the [porting map](../../../../docs/porting-map.md) remain authoritative.

## Impact

Only agent configuration, generated OpenSpec skills, and contributor workflow
documentation change. No product dependencies, APIs, business code, build
configuration, or supported-platform contract changes. Superpowers remains a
user/plugin dependency. Machine overrides and compilation databases stay local.

## Acceptance

- Codex discovers all seven repository OpenSpec skills in the task worktree;
  OpenSpec configuration/change validation succeeds.
- Serena parses portable project settings and activates the exact task worktree.
  An optional local override is ignored and absent from the staged delivery.
- The local clangd is callable; no full C++ readiness claim is made without a
  tested database from this worktree.
- Existing policy text is preserved and reviewed for duplicates/conflicts.
  The reviewed diff contains no business code or machine-specific tracked paths.
- This delivery stops at reviewed configuration and verification evidence;
  commit, push, integration, and archive are outside this execution scope.
