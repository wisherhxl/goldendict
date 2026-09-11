# Publication preflight

Python 3.10+ standard library and Git are required. This program only reads files
and Git state (including ls-remote); it never fetches, stages, commits or pushes.
Run it immediately before each authorized operation. Nonzero exit, Rejected, missing
output or timeout blocks publication. Eligible is not a product quality Pass.

```sh
python -B tools/delivery/preflight.py --repo /exact/task/worktree \
  --authority /evidence/task/authority.json \
  --receipt /evidence/task/review.json --review-sha256 REVIEW_OUTPUT_SHA256 \
  --context /evidence/task/context.json --operation push-task
```

After verified task push, repeat with `--operation publish-baseline`. Only after
the user explicitly accepts the candidate's functionality, include
`--human-acceptance /evidence/task/human.json --human-acceptance-sha256 HUMAN_SHA256`.
Without that acceptance record, baseline publication is rejected; task pushes do
not require it. Only after
Eligible and fresh remote/state inspection may the coordinator run a normal push
with `--no-follow-tags --recurse-submodules=no` and explicit refspec
`C:refs/heads/feature/tiger-qt6-migration`. Use those flags for task pushes too. Check remote and
canonical checkout afterward. Do not substitute arbitrary target/force options.
Automatic follow-tags, mirror or recursive submodule publication and pre-push hooks
are rejected; hook side effects need a separately reviewed policy decision.
The checker does not execute publication, authenticate human approval or provide
server-side compare-and-swap. Existing remote protection is not modified.

## Trusted inputs

These are small JSON records outside the candidate, not a new task system.
Their origin must be independently established from the user authorization and
actual reviewer session. A coordinator cannot establish independence merely by
typing a different reviewer_id. Preserve raw independent output and its origin;
derive `--review-sha256` from that recorded reviewer output, not an untrusted caller.
Review may return the following JSON directly as its immutable receipt, with raw
session output retained alongside it. Never synthesize Pass from a developer summary.

Under the project's implementation/publication authorization rule, approved
implementation supplies both `push-task` and `publish-baseline` authority unless
the user explicitly restricts it. Record that implementation approval in
`approval_source` before independent review; separate publication confirmation is
not required. This does not bypass review, preflight or the fixed target/update
constraints, and does not authorize incomplete checkpoints, PRs, releases, archive
or cleanup. If authority changes after review, preserve the original receipt and
obtain renewed independent judgement bound to the revised authority; never rewrite
an earlier review's hashes or verdict.

Publication permission does not mean manual acceptance. After automated verification
and independent review, show the exact runnable candidate (or reviewable policy/docs
artifact) to the user and wait for explicit confirmation that the checked scope
passes. Preserve actual message provenance; neither an assistant verdict nor silence
qualifies. Keep the later human receipt separate from the earlier review/context.

- Human receipt fields: `result: Pass`, `confirmed_by: user`, exact `candidate` and
  `tree` matching the independent review, `scope` exactly matching the context,
  nonempty `confirmation_source` containing the actual user confirmation and origin,
  `confirmed_at`, `artifact_identity` identifying the inspected build/artifact, and
  `environment`. `fixture` must be absent or false in real use. Hash the preserved
  receipt and supply its SHA-256 to preflight. Missing, non-Pass, incomplete, stale
  or synthetic receipts block baseline publication. Hash equality does not prove
  human authorship; the coordinator must verify the source. New candidate content
  requires renewed manual acceptance. Do not use a unit's acceptance for wider scope.

- Authority fields: `profile` = `goldendict-candidate-v1`; `target` =
  `refs/heads/feature/tiger-qt6-migration`; `operations` contains individually
  authorized `push-task` and/or `publish-baseline`; nonempty `approval_source`
  and `developer_id`; `remote` = `origin`; `remote_url` matches actual fetch and
  push URLs; `update` = `fast-forward`; full `base`, `candidate`, `tree` IDs;
  `task_ref` = exact task refs/heads name; `activation_path` = actual workspace
  activation manifest. Authorization is captured before independent review.
- Activation fields: `profile`, `enabled: true`, `files`: exactly four distinct installed
  policy records `{role, path, sha256}` with roles global-entry, global-delivery,
  workspace-entry and project-workflow. Paths are checked against the actual Git
  workspace, registered canonical checkout and CODEX_HOME (default ~/.codex). The actual deployment records global entry,
  global delivery policy, workspace entry and canonical workflow. Disabled,
  absent or partially installed records block use. Paths are local, never portable
  project settings. A later policy update needs its own reviewed activation refresh.
- Context fields: `profile`, `scope`, nonempty `environment_observation` describing
  actual measurement and relevance, and `evidence` records `{kind, path, sha256}`.
  Required kinds: requirements, policy, verification, environment. Relative paths
  resolve beside context.json. Include all necessary evidence, not just one per
  category. Record platform, tool versions, dependency/configuration identities,
  requirement and policy versions. Re-measure when conditions change: matching
  stale files do not prove the live environment still matches them.
- Independent receipt fields: `result: Pass`, `required_findings: []`,
  `reviewer_id`, `origin_reference`, `fresh_no_history: true`, full `base`,
  `candidate`, `tree`, `scope`, `authority_sha256`, `context_sha256`, `activation_sha256`.
  `fixture` must be absent or false for real use. Failed receipts are preserved,
  not rewritten. The CLI rejects synthetic fixture receipts. Candidate review
  verifies semantic base preservation; the checker cannot infer it from parent IDs.

All object IDs currently use this repository's SHA-1 format (40 hex digits).
All file digests use SHA-256. Inputs with missing/malformed fields reject or exit
nonzero. The checker permits ignored build/cache outputs, but not tracked or
untracked source changes. Review checks actual side effects before and after.

## Tests

```sh
python -B -m unittest discover -s tools/delivery -p test_preflight.py -v
python -B tools/delivery/probe_openspec.py
```

Tests initialize disposable repositories and local bare fixture remotes; they never
push a candidate to the project's real origin. Fixture Pass/Fail data exercises the
mechanical contract only, and is explicitly rejected by production invocation.
The OpenSpec probe requires the installed CLI. It checks conformance skip_specs,
native archive lookup failure, active repair and once-only corrected delta sync.
Archive preparation does not claim to execute a human/AI semantic Verify review;
fixture assertions cover its observable scenario checks. Real Verify remains a
candidate preparation activity under the project contract.

Discussion-only requests never invoke mutation workflows. The publication checker
only rejects unauthorized publication; it is not a sandbox for every agent action.
Serena absence does not block documentation or local textual investigation, but
required cross-interface impact coverage cannot be claimed from text search alone.
