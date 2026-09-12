## Context

This selects only W1 of draft-plan.md under the user's 2026-09-12 approval.
Normal CreateArticleView calls HandleArticleReloadStarted; prepared relay does
not. Both construct the same page/view and repeat event policy. Existing
architecture.md hidden/prepared/visible commit contracts remain authoritative.

## Goals / Non-Goals

Unify necessary initialization and bindings with explicit facade, preferences,
parent and candidate relay inputs. Keep view identity validation independent of
current tab selection. No new page framework, facade API, request ownership,
transaction allocation fix, resource contract or historical test migration.

## Decisions

1. Reuse a private MainWindow creation overload and the existing activation relay.
   Common signal connections invoke one common owner policy, with relay gating
   only on candidate-created views. A missing-call-only patch leaves drift risk;
   a new page manager is unnecessary. Normal-only state registration remains
   outside candidate preparation.
   Article delivery validates the relay's own published generation/epoch and
   owner, enabled state and interaction/shutdown gates, then the current tab/view
   identity. It must not compare against the latest *attempted preparation*
   generation: preparing and abandoning a successor does not retire the active
   page. Non-article relay delivery is outside W1 and remains unchanged.
   Normal connections retain MainWindow as their QObject context; prepared
   connections retain the candidate relay. Zoom is applied after setPage, since
   installing the real page replaces the initial WebEngine zoom state.
2. Candidate events are suppressed before publication; blank prepared pages do
   not represent active reloads. Publication enables already-connected delivery;
   subsequent current-view events update active state. No preparation work moves
   into publication. Ignore stale view callbacks rather than clearing successor
   state, and retain existing generation checks and relay lifetime.
   Hidden preparation intentionally loads no document and submits no reload.
   A hidden start/finish changes no owner reload state. A finish delivered after
   publication without a published in-flight reload cannot invent one. Subsequent
   published loads use the same start/finish state machine as normal pages.
   Active background tabs remain valid because tab membership, not selection or
   visibility, determines identity. Existing navigation/search/output versions
   and pending-reload generation checks remain in place.
   SyncArticleTabs retires a reload record when its page identity no longer
   matches the authoritative view. This existing synchronization runs after
   publication as well as normal tab creation; hidden preparation never clears
   an active page's state. A successor cannot inherit a retired in-flight reload.
3. Use a test-only QTest class with limited friend access for real action dispatch
   and observations of reload/navigation state. Reuse the existing CMake pattern
   that compiles real presentation sources excluding main.cpp. No product
   algorithms are reimplemented. Real MainWindow preparation/publication is the
   new path test; existing production-caller smokes and source call-site mapping
   establish Preferences/source/group entry connections.
   Test-only observers count navigation transitions and search-generation changes
   to detect repeated wiring/terminal work. Synthetic hidden and retired signal
   emissions exercise actual installed connections without fabricating private
   reload state. Search/F3 checks retain F3's existing Dictionaries meaning;
   cancellation uses the existing source-dialog executor seam in the test target.
4. Before the fix, build/run baseline tests, add only test-target membership and
   a friend declaration, then require an actual loadStarted observation to fail
   the missing-state-transition assertion on a prepared view. No timeout or
   launch failure is accepted as red evidence. The friend declaration changes
   access only; record this test-instrumented source identity separately.

## Risks / Trade-offs

- Windows offscreen startup failures → retain baseline failures and run a matched
  native variant where needed; no environment failure counts as red.
- Over-unifying paths → pass candidate values explicitly, keep hidden events
  inert and test active/background/abandoned/stale views.
- Duplicate connections → repeated reconstruction and navigation generation
  deltas, plus one terminal transition and subsequent-refresh assertions.

## Impact Check

Ready for the approved minor-correction scope. No public contract, persistent
format, transaction phase or state ownership changes. Test seam adds only a
test friend, not runtime behavior. Textual callers are enumerated; Serena was
activated at the exact worktree but symbol lookup returned no result, so no
complete semantic coverage is claimed. Compile and real-path tests are required.

## Migration Plan

Baseline → test-only instrumentation and red → common wiring correction → same
green assertions and regression matrix → final self-check and independent exact
candidate review. Do not advance W2. No merge/push. Rollback is limited to the
W1 code/test unit and does not migrate or remove user data.

## W2 selected follow-on

The separately approved W2 correction is recorded in [w2-design.md](w2-design.md).
The W1 design above and its independent evidence are preserved.
# W3.1 follow-on

See w3-1-design.md for the approved first-family responsibility migration.
Historical W1/W2 design sections retain their original scope and conclusions.
