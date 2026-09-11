# Engineering Delivery Policy

Read this policy completely before requirements planning, readiness review,
implementation, completion auditing, committing, or pushing for an engineering
change. It defines the default workflow for every project. Project rules may
add architecture, testing, safety, review, or release gates, but an exception
to requirement-first delivery, independent completion audit, or
audit-before-commit requires explicit user approval and durable documentation.

## Optional profile: candidate-delivery-v1

The numbered sections remain the default strict route. An explicitly user-approved
project may select the candidate-delivery profile in its durable contract, only
for tasks started after verified activation. Without that selector, or during its
rollout, use the default. Existing tasks retain their recorded contract.

For selected tasks, the following replace the default mechanics (not quality):
- Local isolated checkpoint commits are allowed before final review. They are
  recoverable work, not reviewed deliveries, and cannot be pushed automatically.
- Review the actual final committed candidate before task publication. Bind base,
  commit/tree, approved requirements/design/policy, environment and evidence.
  Require fresh no-history independent review, no required unresolved findings,
  and no candidate mutations by the reviewer. Never manufacture a review Pass.
- Candidate content must actually integrate the target base. Reparenting an old
  tree is not integration. Preserve development snapshots and mappings when a
  distinct coherent delivery commit is necessary; do not rebuild good commits.
- For an unchanged direct-successor fast-forward with an identical reviewed result,
  a fail-closed publication preflight may reuse content review. It must check
  authorization, exact identities, remote state, evidence applicability and role.
  Separate full integration review is required when composition changes or its
  impact cannot be established. Project contracts define allowed operations.
- Keep implementation state in the change record and review/publication receipts
  outside the frozen candidate. Do not invalidate a candidate just to record its
  publication. Actual candidate/requirement/environment changes require appropriate
  renewed independent judgement and verification; evidence reuse is explicit.
- Approved change artifacts may satisfy CRD content requirements without another
  normative CRD copy. Preserve requirement IDs, meaning, approval and history.
- Scope-bound authorization persists through tool changes and in-scope repair.
  Requirements, architecture, acceptance and permission changes still pause.
- Isolation, product verification, truthful reporting, preservation of existing
  work, normal non-force publication and protected-target boundaries remain.

This is an optional controlled exception to the default audit-before-commit and
mandatory separate same-result integration-audit mechanics in the entry point and
Sections 2, 4, 5, 7 and 8. It does not change other projects by default. Policy
changes must themselves follow the previously active route; candidate text cannot
approve itself. User-level files are first reviewed as candidate copies, backed
up, installed while the activation selector remains disabled, and enabled last.
On a partial install or hash mismatch, block new-profile work; restore backups or
finish the reviewed installation. Do not guess which half-installed rules apply.

## 1. Classify the work before choosing a lifecycle

`Baseline`, `CRD`, and `Minor correction` are different scopes of change, not
mandatory serial documents for every task. Classify the requested work first
and follow only the applicable route. Do not create a new baseline merely to
make a CRD, and do not create a CRD merely to repair implementation that
already has an approved requirement.

### 1.1 Baseline initiative

A baseline governs a major development scope such as a product generation,
large release, migration, or similarly broad program. It is the durable
product and engineering contract for that scope and may define mission,
supported observable behavior, architecture and ownership rules,
compatibility, platform scope, data and migration obligations, security and
licensing constraints, non-goals, and release or completion gates.

Create or revise a baseline only when the intended decision establishes or
materially reshapes that broad scope. Confirm it once, decompose its delivery
into functional units, and retain it as the authority throughout
implementation and subsequent testing. Do not recreate the baseline for each
feature, defect, or follow-up adjustment.

### 1.2 Change Requirements Document (CRD)

A CRD changes, adds, clarifies, or deliberately diverges from requirements
within an existing baseline without redefining the whole baseline. CRDs often
arise from product decisions, testing, feedback, or discoveries made during or
after baseline implementation. A CRD is required when the requested outcome
changes approved product intent, scope, observable behavior, compatibility,
acceptance criteria, or another requirement-level decision.

A CRD must identify its governing baseline and state, proportionately to the
change:

- the problem and intended outcome;
- in-scope and out-of-scope behavior;
- user-visible behavior and exact compatibility expectations;
- functional and non-functional requirements;
- data, configuration, migration, failure, and rollback behavior;
- affected platforms and integration boundaries;
- acceptance criteria and required automated/manual evidence;
- any approved divergence from the baseline and its rationale;
- the functional delivery units when the change is too large for one
  independently reviewable commit.

The CRD must not silently override the baseline. Resolve material ambiguity
and obtain user confirmation before implementation.

### 1.3 Minor correction

A minor correction repairs or aligns implementation, tests, resources,
configuration, or documentation when the intended result is already fully
defined by an approved baseline or CRD. It does not introduce a new product
requirement, intentional behavior change, architecture decision, public
contract change, supported-scope change, or policy decision.

Do not create a baseline or CRD for a minor correction. Record the issue, cite
the governing baseline or CRD requirement, state the expected conforming
outcome, and define proportionate verification. If investigation shows that
the desired outcome is not already authorized, reclassify the work as a CRD.
If it materially reshapes the major development scope, treat it as a baseline
decision instead.

When classification is uncertain and the answer would change product intent,
use the issue-resolution policy before editing. When the user's request or
canonical documentation already makes the class clear, record it and proceed
without asking for redundant confirmation.

## 2. Applicable delivery routes

### 2.1 Baseline route

1. Draft and confirm the baseline.
2. Audit development readiness and decompose delivery into functional units.
3. Implement one approved functional unit.
4. Obtain an independent completion audit for that unit.
5. Commit and push the unit according to project policy.
6. Repeat steps 3-5 until the baseline completion gates are satisfied.

### 2.2 CRD route

1. Identify the governing baseline.
2. Draft and confirm the CRD.
3. Audit development readiness and define its functional units.
4. Implement one approved functional unit.
5. Obtain an independent completion audit for that unit.
6. Commit and push the unit according to project policy.
7. Repeat steps 4-6 until the CRD acceptance criteria are satisfied.

### 2.3 Minor-correction route

1. Record the issue and cite the governing approved requirement.
2. Perform a concise impact check confirming that no requirement or
   architecture decision is changing.
3. Implement the correction and its proportionate verification evidence.
4. Obtain a proportionate independent completion audit.
5. Commit and push the correction according to project policy.

### 2.4 Worktree and concurrency isolation

For a Git repository, use one dedicated task branch and one dedicated worktree
for each delivery that can be developed concurrently. Create it from the
recorded approved base commit before implementation begins. A project may
prescribe its own branch names, worktree locations, setup commands, or a
pre-existing isolated task worktree; follow those rules without weakening the
isolation guarantees below.

- Allow only one development writer for a worktree at a time. Read-only audit
  sessions may inspect that same worktree.
- Never let concurrent development tasks write to the same worktree, index, or
  build directory.
- Keep baseline, reference, and integration worktrees clean unless their
  documented role explicitly authorizes a controlled integration operation.
- Give each worktree its own build, cache, generated-output, and dependency
  state when the tools do not safely support sharing.
- Put independent concurrent deliveries in separate worktrees even when they
  start from the same base commit.
- Treat changes that must share uncommitted intermediate state as one delivery,
  or develop them sequentially with an explicit dependency. Do not simulate
  parallelism by using multiple writers in one worktree.
- Use a distinct branch for each persistent delivery. Do not check out the
  same branch in more than one worktree.

For a non-Git project, use an equivalent isolated writable copy or environment
per concurrent delivery and document how its audit snapshot is identified.

### 2.5 Controlled integration worktree

When audited delivery branches must be combined into a shared development
baseline, use a dedicated integration branch and dedicated integration
worktree. Allow exactly one integration writer for that worktree and keep it
clean between integration candidates. Project policy controls whether the
agent is authorized to merge, cherry-pick, advance the target branch, or push
the integrated result.

Autonomous integration requires a durable project-level Integration Contract.
The contract must define:

- the exact development baseline branch or branches that may be advanced;
- protected branches that must never be advanced autonomously;
- the permitted integration method and whether merge commits are allowed;
- the targeted and cumulative verification required for each candidate;
- the independent integration-audit evidence and identity to record;
- whether the integration branch or target baseline may be pushed
  autonomously;
- conflict, rollback, failure, retry, and worktree-cleanup behavior;
- any pull-request, release, or branch-protection gates that remain manual.

If a project has no Integration Contract, stop after the audited delivery
branch is committed and pushed. Do not infer a target branch or treat the
global commit/push default as authority to integrate, merge, or advance a
shared baseline.

Integrate audited deliveries one at a time in documented dependency order:

1. verify the delivery branch is committed and pushed, its commit tree equals
   its audited Tree ID, and its audit evidence is complete;
2. update the clean integration worktree to the current target baseline and
   record that integration base commit;
3. prepare the proposed integration without finalizing a new integration
   commit when the project integration method permits it;
4. if the integration reports a conflict or requires any feature-code change,
   abort the candidate and return it to its original development worktree;
5. have the original development thread update from the current integration
   base, resolve the problem, rerun verification, obtain a new independent
   delivery audit, and push the replacement delivery;
6. for a clean candidate, record the combined candidate Tree ID (the staged
   Tree ID when the integration method uses the index) and run the feature's
   targeted verification plus the cumulative regression checks required by
   the project;
7. request a fresh, no-history, read-only integration audit of the combined
   candidate, bound to the integration base commit and combined Tree ID;
8. after `Pass`, finalize the integration exactly from the audited candidate,
   verify the resulting committed tree equals the audited combined Tree ID,
   and push or advance the target only when project policy authorizes it;
9. restore a clean integration worktree, record the integrated delivery and
   evidence, then select the next candidate.

The integration owner coordinates and verifies composition; it does not repair
feature code or resolve semantic conflicts in the integration worktree. An
individual delivery Pass remains evidence for that delivery commit, but it
does not by itself prove the behavior of the combined baseline. The separate
integration audit is the completion evidence for the integrated result.

Do not skip a step within the selected route. A failed gate returns the work
to the applicable earlier step; it does not authorize the auditor to repair
the work. Project policy controls whether commit and push occur autonomously.
If project policy requires user direction, stop after the audit passes, report
that the audited delivery is ready, and wait without treating it as fully
delivered.

## 3. Development-readiness audit

For a baseline initiative or CRD, after the user confirms the requirements and
before production-code edits, perform a read-only readiness audit. Check that:

- the proposed baseline or CRD is consistent with its governing authority and
  contains no unapproved scope reduction or behavioral redesign;
- ownership, dependency direction, public interfaces, and data flow comply
  with the documented architecture;
- the proposed design complies with `software-design.md`, including its SOLID
  and design-pattern selection rules;
- compatibility, migration, security, licensing, platform, and failure
  behavior are implementable without hidden policy decisions;
- acceptance criteria are observable and the test plan can prove them;
- the work is decomposed into coherent functional units that can each be
  implemented, audited, committed, and pushed independently;
- required tools, credentials, environments, fixtures, and permissions are
  available, or a non-blocking fallback is documented;
- no unresolved question remains that would materially change implementation.

Record `Ready` or `Blocked`. If blocked, revise the applicable baseline or CRD
and obtain confirmation for every material requirement change before auditing
again. If ready, record the architecture choice, test plan, and delivery-unit
boundaries durably.

When project policy authorizes it, commit and push the confirmed requirements
and readiness artifact as its own documentation unit before implementation.
Otherwise preserve the artifact without treating this policy as commit or push
authorization.

A minor correction does not require a baseline/CRD readiness audit. Its
read-only impact check confirms that the governing requirement is unambiguous,
the correction stays within current architecture and policy, verification can
prove conformance, and no hidden requirement decision is being made.
Reclassify it rather than stretching the shorter route when a check fails.

## 4. Autonomous implementation

Once baseline/CRD readiness is `Ready`, or a minor correction passes its impact
check, implement the approved functional unit or correction without routine
human interaction. The development thread must:

- follow the governing baseline, CRD when applicable, architecture choice, and
  acceptance plan;
- follow `software-design.md` for SOLID responsibilities, dependency design,
  interface design, and evidence-based pattern selection;
- make conservative in-scope decisions by reference to project behavior and
  conventions;
- install missing in-scope tools or libraries when authorized and safe;
- preserve unrelated user and agent changes;
- add or update focused tests and durable documentation with the behavior;
- run proportionate builds, tests, and manual checks;
- keep a concise non-blocking issue log and continue safe independent work
  when one check is unavailable;
- stop only when new authority is genuinely required, such as an intentional
  requirements divergence, material architecture change, destructive or
  external action outside approved scope, unavailable secrets/access, or an
  ambiguity that would materially change the product outcome.

Do not create a large unreviewable delivery. When a baseline initiative or CRD
contains multiple functional units, finish the full workflow for one unit
before developing the next. Implementation remains uncommitted until its
independent completion audit passes.

### 4.1 Design and deliver by functional unit

Before implementation, decompose the approved scope into functional units and
record a proportionate design for the next unit. Identify its governing
requirement, observable outcome, in-scope and out-of-scope behavior, ownership
and affected interfaces, dependencies, and acceptance criteria with verification
commands. Use an existing approved baseline, CRD or task record when sufficient;
a minor correction may use a short design note in its impact check. Do not
create a new baseline or CRD merely to document each unit's implementation.

Within each development stream, finish one unit's complete delivery loop before
starting the next: design, implementation, tests, independent completion audit,
authorized commit and push, then applicable project integration or PR gates.
Where policy requires additional authority, stop at that gate without claiming
delivery or accumulating another unit in the same change. This does not prohibit
independent concurrent streams isolated as required by Sections 2.4 and 2.5.

If a unit is too large to design, review or verify coherently, split it before
expanding implementation. Each subunit must have a meaningful functional
boundary, explicit dependencies and an independently verifiable, working state.
Keep directly related code, tests, resources and documentation together. Do not
write a large batch first and rely on retrospective commit splitting; do not
split mechanically by file or publish a knowingly incomplete or broken state.

### 4.2 Keep development context bounded

Treat each completed functional unit as a natural boundary for a fresh
development session. Prefer that boundary over carrying an ever-growing history
of unrelated units. A single session should focus on one bounded delivery;
do not rely on repeated automatic compaction to manage oversized work.

Before context pressure obscures the active requirements, decisions or state,
pause implementation and prepare a concise durable continuation record. If a
unit must span sessions, record:

- requirement and design references, scope and acceptance criteria;
- exact repository/worktree, branch and base, or equivalent non-Git snapshot;
- completed and remaining work, pending changes and their ownership;
- verification commands, results and evidence locations;
- unresolved issues, next steps, delivery/audit stage, and any frozen snapshot
  identities or authority still required.

Prefer references to canonical documents and evidence over copied transcripts.
The continuing developer must reload applicable instructions, inspect actual
workspace state and reconcile it with the record before writing. Transfer the
single-writer role explicitly; a fresh session does not authorize concurrent
writes or discard existing changes. If a fresh session is unavailable, retain
the handoff and narrow the active scope; never claim a context reset occurred.

This development handoff is distinct from independent completion or integration
audit. A continuing developer receives the necessary implementation context;
an auditor receives only the canonical requirements, candidate identities and
verification inputs allowed by the audit policy, not the development transcript
or inherited reasoning. A session change does not bypass an audit, unfreeze a
candidate, or preserve a Pass after its audited identity changes.

## 5. Independent completion audit

When implementation believes a functional unit or minor correction is
complete, it must prepare an immutable Git audit identity before requesting a
fresh audit session:

1. confirm the dedicated worktree contains only this delivery plus ignored
   build/cache output;
2. stage every tracked change, deletion, and new file belonging to the
   delivery, without committing;
3. confirm there are no unstaged tracked changes or relevant untracked files;
4. record the current base commit ID and the staged Git Tree ID;
5. pause all development writes to the worktree until the audit finishes.

Unless project tooling supplies an equivalent command, identify the base with
`git rev-parse HEAD` and the staged tree with `git write-tree`. Record both
exact object IDs in the audit request and report.

The audit must:

- have no inherited conversation, developer reasoning, or hidden working
  context; use a new session or agent with no forked history;
- receive only the repository/worktree location, target branch and base,
  canonical governing-requirement paths, applicable baseline/CRD or minor
  issue record, delivery identifier, recorded base commit ID, recorded staged
  Tree ID, and verification commands;
- independently read applicable global and project instructions, requirements,
  architecture rules, diff, tests, and relevant product evidence;
- make no source, test, resource, documentation, configuration, staging,
  commit, or push changes;
- not run auto-fixers or formatters that mutate tracked files;
- run only read-only inspection plus builds/tests that write to ignored build,
  cache, log, or temporary locations;
- judge the work without relying on development-thread claims or summaries.

This policy is standing user authorization to create the required clean audit
session or agent; do not ask the user for the same authorization again. Prefer
a no-history auditor attached to the same working tree so it can inspect the
exact uncommitted diff.

The development thread must provide an auditable dedicated worktree containing
the complete delivery in the staged diff, related tests and documentation, no
unstaged tracked changes, no relevant untracked files, and no knowingly mixed
unrelated changes. The auditor verifies that the current base commit and staged
Tree ID equal the recorded values. If identity or attribution is unclear, the
audit fails until development restores a single unambiguous delivery snapshot.

The auditor verifies at least:

- every applicable baseline, CRD, or cited conformance requirement mapped to
  implementation and evidence;
- architecture and dependency compliance;
- compliance with `software-design.md`, including SOLID responsibilities and
  whether each applied design pattern has concrete value without unnecessary
  abstraction;
- observable behavior, edge cases, errors, migration, and compatibility;
- test adequacy and actual test/build results;
- absence of silent omissions, scope expansion, regressions, unsafe behavior,
  unrelated changes, generated artifacts, and undocumented divergence;
- whether the audited delivery is genuinely complete rather than compiling.

The result is exactly `Pass` or `Fail`; do not use “pass with required fixes.”
A Pass report includes the audited base commit ID, staged Tree ID, a concise
requirement/evidence matrix, and verification commands/results. A Fail report
contains prioritized actionable findings with file/evidence references and
identifies the step to which work returns.

## 6. Audit failure and rework loop

On `Fail`, send the complete report to the original development thread. The
auditor must not fix the work. Development resumes ownership, implements the
findings, updates tests and documentation, and reruns verification. If a
supposed minor correction requires a new requirement decision, reclassify it
as a CRD before continuing.

Then request a new clean, no-history audit. Do not reuse the previous auditor's
conversation as the completion gate. Repeat until `Pass`. Unresolved,
unverified, or ambiguous acceptance evidence is a failure, not a reason to
lower the gate.

## 7. Commit and push after audit

Only the original development thread may perform a commit or push authorized
by applicable project policy. Project-level rules take precedence. If a
project has no explicit commit or push policy, `Pass` is standing authority to
commit the audited delivery and push it normally to the remote feature branch
without another confirmation.

When commit and push are authorized, the development thread must:

1. confirm the current HEAD equals the audited base commit ID;
2. confirm the current staged Tree ID equals the audited Tree ID;
3. confirm there are no unstaged tracked changes or relevant untracked files;
4. invalidate the Pass and obtain a new audit if the base, tree, or worktree
   state differs;
5. inspect the already-staged diff for secrets, generated output, unrelated
   changes, and correct functional scope without restaging it;
6. create one reviewable commit directly from the audited index, following the
   project's message convention;
7. verify the committed tree equals the audited Tree ID;
8. run required post-commit verification that depends on commit state;
9. push normally to the configured remote feature branch;
10. report the delivery, audit identity, commit identifier, remote branch, push
   result, verification evidence, and separately tracked work.

Never combine unrelated features into one large commit. Tests, resources, and
documentation that directly prove or explain one feature belong with it.
Split large work at coherent, independently verifiable functional boundaries,
not arbitrary file boundaries. Do not force-push, rewrite published history,
merge, or open a pull request unless the user or project policy authorizes it.

## 8. Audit integrity

- A completion audit is evidence, not implementation assistance.
- Any change to the audited base commit, staged Tree ID, tracked source, or
  documentation after Pass invalidates the Pass. Rebasing, merging,
  cherry-picking, or resolving a conflict requires a new audit whenever it
  changes the recorded base or resulting tree.
- Integrating an individually audited delivery into a different combined tree
  requires a separate integration audit. Do not reuse the delivery Pass as
  proof that the combined baseline is complete.
- A development session's confidence is not audit evidence.
- Compilation alone never proves completion unless the approved requirement
  defines compilation as the complete observable outcome.
- Development and audit remain distinct roles even on the same model or host.
- If a genuinely fresh no-history audit is unavailable, do not commit. Report
  the blocker and preserve the uncommitted implementation for later audit.
