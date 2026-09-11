# AI work model v1 rollout

Status: implementation authorized; activation pending old-contract audits and delivery.

## Governing approval and fixed scope

The user approved controlled GoldenDict adoption in the current task: recoverable
local commits, exact candidates, independent candidate review, fail-closed
publication checks, one OpenSpec record, external delivery receipts, and continuous
coordination within explicit authority. The three execution conditions require
correct base integration (never parent substitution), an actual archive failure
recovery experiment with a simple fallback, and executable negative gate tests.
This is a high-risk governance change, not a minor documentation correction.

This rollout itself uses the OLD contract at base
`923bf0a28b9ec8292561c8ba63ccc53de67efee7`: no implementation commit before a fresh
no-history staged-tree completion Pass; task push; separate independent integration
audit in a dedicated integration worktree; normal fast-forward target push.
The new policy cannot authorize its own delivery. Only new tasks started after
explicit activation may select `goldendict-candidate-v1`. Existing tasks, notably
the staged richtext worker, retain their old contracts and state.

## Implementation and verification plan

1. Preserve old user/workspace/project policies and existing task identities outside
   the candidate. Stage candidate copies of external policy files; never edit live
   user/workspace policies while developing or auditing.
2. Replace the project lifecycle and integration text; shorten AGENTS; align
   OpenSpec records without modifying generated skills or upstream plugins.
3. Add one standard-library Python read-only publication preflight and temporary
   Git tests. Inputs bind authority, target/base/candidate/tree, independent receipt,
   requirements/policy/environment evidence hashes and exact checkout/remote state.
   The gate is not a product reviewer or an authenticity/signature service.
4. Test valid and invalid identities, authority, evidence, checkout and remote
   states against local fixture remotes only. Synthetic receipts must be labeled
   fixtures and cannot be used as real review evidence.
5. Exercise installed OpenSpec 1.13.0 using temporary repositories: conformance
   without duplicated requirements and archive after verification, failure, repair,
   rediscovery and resynchronization. Prefer one light post-review archive delivery
   if native recovery is not simple; never introduce a recursive change.
6. Record scenario results and limits (no real product build, semantic readiness,
   server protection modification or real test-candidate push).
7. Freeze all delivery files, request independent completion audit under the old
   policies, commit unchanged index after Pass, push task branch, independently
   audit integration, and fast-forward canonical under the old contract.
8. Verify copied policy hashes before activation. With a pending activation marker
   keeping the old route active, install reviewed user/workspace copies from the
   delivered commit, then publish the external activation record last. Preserve
   reversible backups. Recheck canonical, frozen Qt5 and all pre-existing tasks.

## Acceptance

- New lifecycle has scope-bound authority, four risk classes, repair/review loop,
  singular requirements/design/tasks ownership and candidate-external receipts.
- No checkpoint push or baseline publication without independent exact review.
- Base advances require actual content integration and full relative diff review.
- Gate rejects changed candidate/base, missing or Fail review, wrong target,
  missing authority, stale/missing evidence, wrong checkout and unfinished Git state.
- OpenSpec behavior is tested, not inferred from guidance; unsupported defaults
  are routed away from, not patched in generated skill files.
- Archive has a tested native path or documented simple nonrecursive fallback.
- No product source/build configuration or Serena/upstream/generated skills change.
- Other projects default to strict staged review; old GoldenDict tasks are preserved.
- External live policy changes occur only after old-contract audit and delivery.

## Recovery

Before activation, discard only this candidate if needed; live rules remain old.
After activation, disable the activation record and restore verified external
backups, stop new deliveries and use the old contract retained in the prior Git
revision. Published history is never reset: revert the project policy via a new
audited normal delivery. Preserve checkpoints, staged changes and all old receipts.
