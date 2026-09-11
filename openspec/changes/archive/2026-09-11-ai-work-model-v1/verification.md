# Controlled pilot verification

Local tools: OpenSpec 1.13.0, Superpowers 6.3.0 inspected, Python 3.14.7.
No product source/build reconfiguration, real fixture-remote publication, generated
skill edit, Serena change or pre-existing task conversion was performed.

## Executed checks

- 30 temporary-Git preflight tests pass. Valid task and baseline publication eligibility
  succeeds; changed candidate/base, wrong target/role, no operation authority, absent/
  Fail/self review, missing/stale evidence, dirty checkout, unfinished operation,
  unpushed task, synthetic receipt, changed receipt digest, pending activation and
  partial policy installation reject. Additional tests reject replaced activation manifests, missing/duplicate roles, wrong installed paths and multiple push destinations. Extra tag/mirror/submodule publication, pre-push hooks and pending canonical operations reject; an actual explicit no-follow-tags fixture push leaves the remote tags empty. Discussion-only authority rejects publication.
- A fixture base advance was actually integrated using Git; its new file survived and
  the resulting diff against that base contained only task.txt. Old review identity
  rejected the new result. Parent identity alone is not semantic preservation proof.
- Installed OpenSpec accepted a conformance change citing an existing requirement with
  skip_specs: true: status, apply instructions and strict validation passed without
  product specs. Native archive preserved metadata, removed active change and produced
  one archive. Active-name status failed after archive: early-archive recovery is NOT
  adopted. Use one lightweight post-review closeout, without a recursive change.
- The fallback kept a failed task active, repaired tasks and delta, validated and
  archived once. Actual main spec sync contained the corrected requirement once,
  retained its scenario and did not carry the incorrect text. Original simulated
  Fail receipt remained unchanged; fixture receipts are never real review evidence.
- Local Markdown file links resolve; git diff --check passes; required product/
  configuration boundaries were inspected.

## Scenario scope and limits

Maintenance: short reference and parsing/ignore/diff checks, no product build or
mandatory new change. Parity: existing-requirement native schema/apply is exercised;
real Qt5/Qt6 runtime parity is not tested by these fixtures. Major interface work
requires readiness and adequate impact coverage; absence of a reliable Serena database
must not be reported as semantic readiness. Discussion only remains read-only; the
preflight cannot sandbox arbitrary implementation actions. Actual agent routing is
specified and inspected, not claimed as an end-to-end product benchmark.

Fail repair, archive and base-advance scenarios use fixtures. Independent completion
and integration audits for THIS rollout occur separately under the old contract.
Receipts bind immutable identities and live environment observations must be renewed
when relevant conditions change. Hashes cannot authenticate a self-authored review or
prove a stale environment description true. No remote protection was inspected or
changed; the preflight is not a remote transaction lock.

## Raw evidence

External task evidence directory: evidence/ai-work-model-v1 (workspace-relative).

- preflight-tests-3.txt: SHA-256 `139a3298c7f8bdc3ea50801c66aad173712fb71c170d30b54ac843d91525ed33`.

- openspec-probe.json: SHA-256 `af10d44c08ed0d4b19baed8c9ee64fb43091fa962dfa5e539b0f2e6e5aa956ce`.

- old-rules-hashes.json: SHA-256 `c96195a894cb3c14ab8fd2f860c2497095c5336b1a3d188bea78dfb4ed5b0f37`.

- readiness.md: SHA-256 `f2502a530b5bfe3c0fcac87f0c70af7e597612e5e3dcfa053ece82aa3dd0d61f`.
