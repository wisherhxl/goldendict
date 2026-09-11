## 1. Matched Baseline

- [x] 1.1 Verify activated delivery policy, clean synchronized base, isolated task worktree and native OpenSpec root; record identities externally.
- [x] 1.2 Build the exact task baseline with its own Conan environment; record binary identity and run existing shell/menu/tab checks.
- [ ] 1.3 Reproduce fresh and restored layouts against frozen Qt5 at matched style/font/DPI/window/profile state; record geometry, source evidence and per-region gaps.

## 2. Main-window Conformance

### First delivery unit: View presentation persistence

- [x] 2.0.1 Record DTO/serialization impact and obtain independent readiness review.
- [x] 2.0.2 Demonstrate missing persistence with failing shell and Core tests; implement the three View settings using the existing callback.
- [x] 2.0.3 Verify reconstruction, rejected saves, legacy/current configuration, and five-process restart behavior.
- [x] 2.0.4 Capture native Qt5/Qt6 View states and document evidence limits.

### Remaining delivery units

The items below remain open; the first delivery is not whole-window acceptance.
Confirmed findings include history/favorites activation semantics, selection and
management actions, result dictionary icons and selected-row restoration, and pane
gutters. Determine each next unit's exact scope before implementation.

- [ ] 2.1 Correct confirmed region/default/restoration defects under CRD-SHELL-001/002, with failing-before/passing-after regression evidence and preservation of valid custom layouts.
- [ ] 2.2 Verify menu/toolbar/query/tab/pane/status interactions under CRD-SHELL-001/003/004/005; correct demonstrated local discrepancies with focused tests.

## 3. Acceptance

- [ ] 3.1 Run the focused Release shell/menu/tab/lookup/history/favorites regression suite through run_with_conan.ps1 and record exact commands/results.
- [ ] 3.2 Capture matched Qt5/Qt6 initial, lookup, changed-layout and restart states; assess controls, geometry, icons, text and usable article area.
- [ ] 3.3 Complete the region/evidence matrix, identify any unverified platform or dependency requirements, and validate this OpenSpec change with native tooling.
