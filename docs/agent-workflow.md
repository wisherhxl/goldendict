# Agent Workflow

Version: `goldendict-candidate-v1` (2026-09-11), with the user-approved
implementation/publication authorization refinement of 2026-09-11.

## Activation and authority

This replaces the former Collaborative Task Lifecycle and AI tooling/Integration
Contract for NEW tasks only. Selection requires an enabled workspace-local
`.ai-work-model-activation.json` whose installed file hashes all match. Until then,
use the old contract at Git base `923bf0a28b9ec8292561c8ba63ccc53de67efee7` and
preserved external policy copies. A partially installed profile blocks new work.
The governance rollout itself is delivered under that old contract, including
staged-tree completion audit and a separate integration audit/worktree.

Existing tasks retain their recorded contract; do not automatically convert staged
work, repair a different task, or treat an old Fail as a Pass. Explicit conversion
requires a preserved base/index/working-state snapshot and ownership handoff.
User-level candidate delivery is opt-in; other projects keep the strict default.

User approval defines scope and important decisions. Planning permission does not
imply code writes. Unless the user explicitly restricts it, implementation permission
includes in-scope repairs, local checkpoints, normal publication of the independently
accepted candidate to its task branch, and same-result fast-forward publication to
origin `refs/heads/feature/tiger-qt6-migration` under the Integration Contract.
Record `push-task` and `publish-baseline` operations with the implementation approval
as their source before review; do not ask for redundant publication confirmation.
Archive and cleanup authority remain separate. Existing explicit restrictions and
end-to-end authority survive tool and
session changes. Pause for material scope, architecture, acceptance or permission
changes, not for routine approved repairs. For unresolved decisions present the
recommended resolution and real alternatives, without inventing choices.

This authorization refinement applies to the current main-window parity task and
subsequent approved implementation under this activated profile. It does not convert
legacy tasks to a different delivery lifecycle. Policy rollout itself uses the
previously active rules and the user's explicit publication authorization; proposed
policy text cannot authorize its own installation.

## Lifecycle and risk

| State | Owner and output | Automatic transition / failure |
| --- | --- | --- |
| DEFINE | User chooses scope; coordinator records requirements, risk, authority and necessary design | Approved sufficient scope proceeds; discussion-only ends without writes |
| BUILD | One writer implements, verifies, maintains tasks and recoverable checkpoints | Complete verified unit becomes candidate; in-scope failures are repaired |
| REVIEW | Fresh no-history independent reviewer judges exact candidate and evidence | Pass permits authorized delivery; Fail returns to BUILD without changing scope |
| DELIVER | Coordinator checks publication prerequisites and remote results | Eligible exact candidate publishes; identity/environment changes block |
| CLOSE | Coordinator records outcome and verifies evidence/resource disposition | Safe authorized cleanup; uncertain resources retained |

Exploration is read-only unless artifact writing was requested. Clear maintenance
has a short issue/requirement reference, proportionate checks and concise independent
review; no mandatory new OpenSpec change. Regular parity work uses one OpenSpec
change, focused tests and applicable Qt5/runtime/platform evidence. Major behavior,
interface, architecture, compatibility or approval/audit-policy changes require
explicit design approval and independent readiness review even when the diff is tiny.
Stop after three nonconverging repair/review rounds, or two attempts on the same
blocker without new evidence; report findings and a concrete next step. This never
turns an unmet requirement into Pass. An independent reviewer never repairs work.

## One record per concern

The product baseline CRD and its approved decisions retain their IDs, meaning and
approval history. Migration, parity, porting and project design documents retain
existing authority. Reference them instead of copying requirements. A genuine new
requirement change is recorded in OpenSpec proposal/delta specs/design and explicit
approval; those artifacts satisfy CRD content without another normative CRD copy.
Do not silently relocate existing requirements or rewrite approved intent.

OpenSpec design owns change-level technical decisions. Tasks own implementation
and verification steps, not audit/push/integration checkboxes. Elaborate execution
steps directly in tasks; a detailed attachment is optional only when it adds value,
references design/version and cannot redefine requirements. Design/interface changes
invalidate affected execution details. No parallel docs/superpowers specification.

A short external `evidence/<task-id>/run.json` (or Markdown equivalent) references
scope, risk, approval source, profile version, current state, writer/worktree/branch,
base/candidate, findings and next action. Independent review and publication receipts
are separate immutable files under that task; never overwrite a failed receipt.
Raw evidence includes source identity, capture environment, purpose and SHA-256.
A hash is content integrity, not proof of truth or independent authorship.
Review receipts come from the actual independent agent/session, not coordinator-
authored verdict text. Capture its origin and exact output/hash outside the tree.

## Tool invocation

OpenSpec 1.13.0 is the tested local version. Confirm `openspec context --json` resolves
this task's root. Use native status/instructions/validate for artifacts. Artifact
`done` or `ready` is not approval/readiness/quality evidence. Existing-requirement
conformance may declare `skip_specs: true` in change metadata without creating spec
files; tested native status/apply/validate accepts this. Never manufacture product
requirements merely to satisfy the schema. New requirement deltas must not skip specs.

The generated propose skill is planning-only and explicitly stops before apply.
Use it for a planning-only request. For an already authorized end-to-end task,
coordinate native artifact instructions and apply without calling propose as a
universal entry. Do not claim advisory config overrides mandatory skill behavior.
Explore is read-only; its write confirmation is respected when that workflow is
chosen. Read current artifact instructions, not hardcoded generated templates.

Use Superpowers debugging, focused TDD, task decomposition, early review and
verification-before-completion where useful. Its configurable spec/plan locations
point to the same OpenSpec record. Local development commits are checkpoints only.
Do not invoke its generic worktree fallback or finishing Git actions: linked-worktree
status does not establish task role, and direct merges/PRs are not this contract.
The coordinator owns those operations. Do not edit generated OpenSpec skills,
Superpowers cache, or Serena upstream. Config guidance is advisory, not enforcement.
Pure config/docs use parsing, consistency and behavioral checks rather than artificial
TDD. Product changes require risk-appropriate tests/runtime/parity; compiling is not
completion. Run CTest/build-tree programs through this checkout's Conan launcher.

## Serena and degraded operation

Portable project configuration remains versioned; machine paths belong only in the
ignored local override. Activate the exact task path, never a shared name alone.
Definitions, references and interfaces use semantic tools when reliable. Text/config
search uses rg. No reliable database is needed to discuss or edit documentation.
A local implementation-only correction may use explicit textual impact analysis
plus relevant compile/tests. Cross-module interface/ownership changes require
reliable analysis or documented equivalent coverage; lacking both blocks that work.
Never describe textual search as complete reference analysis.

Compilation databases belong to the actual source worktree/toolchain/dependencies
and generated headers. Refresh when configuration or membership changes; never
reparent another checkout's database or commit machine paths/databases. No full C++
semantic readiness claim without a verified database. Memories are navigation aids.
Read-only review permits isolated ignored cache/index/build writes, but not candidate,
index, ref or policy edits. Inspect actual tool side effects and verify candidate
identity before/after; never assume a query is literally filesystem-read-only.

## Candidate construction and independent review

Develop on an isolated task branch from a recorded Qt6 baseline. Local checkpoints
may precede final review and remain unpublished. Reuse an already coherent qualifying
commit. If a separate delivery snapshot is needed, preserve the development ref and
record base, development commit/tree, candidate commit/tree and the complete diff.

Candidate MUST contain the confirmed target base plus correctly integrated task work.
If the base advances, incorporate it in the development worktree and resolve conflicts
there. Compare the full resulting diff against the new base: no lost baseline changes,
unrelated task changes, or unauthorized reversions. Assigning the old tree a new
parent is forbidden. A graph/tree check cannot prove semantic preservation: the
reviewer checks the full result and verification. Form a coherent single-parent
candidate directly after the confirmed target; do not rebuild commits mechanically.
No force-push, published-history rewriting or content repairs during publication.

Review the actual candidate identity (including Git-derived version behavior when
relevant), complete diff/result, approved requirements/design and applicable evidence.
Bind base, candidate commit/tree, authority, policy and environment evidence. Fresh
no-history reviewers receive canonical inputs, not developer reasoning. Pass requires
all applicable acceptance evidence; no Pass with required fixes. Development feedback
is not final review. On Fail repair in scope and obtain renewed independent judgement.

## Integration Contract

Only origin `refs/heads/feature/tiger-qt6-migration` is authorized for baseline
publication; master/main/releases/tags/other shared targets remain protected.
Task push and canonical push require recorded operation authority inherited from
implementation approval as described above, or explicit publication approval.
Independent review, applicable verification and successful publication preflight
remain mandatory; implementation permission alone never permits publishing an
unreviewed checkpoint. PR creation, releases/tags, other targets, force pushes,
archive and cleanup are not included. One publication writer at a
time; always normal non-force updates with explicit refspecs and
`--no-follow-tags --recurse-submodules=no`. Preflight rejects automatic extra-ref
publication settings and pre-push hooks requiring separate side-effect review. Do not modify remote
protection. Recheck remote state immediately before push; local inspection is not a
server-side transaction lock. A rejected push stops; never retry by forcing it.

Run `python -B tools/delivery/preflight.py --help` for required inputs before EACH
task or canonical push. See [publication inputs](../tools/delivery/README.md).
Rejected/error/nonzero means do not execute publication. Eligible is mechanical
permission to proceed under existing authority, never a new quality Pass. Verify
live environment observations remain true; invalidate and refresh them when relevant
tools/configuration change. Inputs are trusted records, not a replacement for actual
review origin or environment measurement.

If base/candidate/result and relevant requirements/policy/environment match, reuse
independent content review and run publication preflight instead of a second full
content audit. If target or candidate changes, stop, integrate correctly in development,
form a new candidate and independently assess affected composition/verification.
Document reusable evidence and why it remains applicable; don't rerun unrelated checks
mechanically, and don't reuse acceptance just because Tree IDs match.

Task branch must exist on origin at the exact candidate before canonical publication.
For an identical fast-forward no separate integration worktree is required. A clean
frozen task checkout can serve verification. Use a temporary separate verification
environment when build isolation requires it. No autonomous non-fast-forward composition.
After canonical push, verify its remote tip, advance the clean canonical checkout only
by fast-forward, and verify local/remote/tree/clean state. If local state is unexpected,
preserve it and report delivered-remote/local-sync-blocked. Never reset user changes.

## Archive and closure

The installed native status/apply cannot resolve an archived change by its active
name (see rollout probe). Therefore v1 uses SIMPLE POST-REVIEW ARCHIVE CLOSEOUT;
early archive is not the default. Keep the change active through implementation
review/failure/repair so tasks/materials remain directly addressable.

After implementation delivery, run Verify, reconcile delta specs, archive once and
review that small tracked maintenance candidate. It needs no new OpenSpec change.
Use native `archive <name> --json --yes` only under existing archive authorization;
for tooling/conformance without deltas use metadata skip_specs and `--skip-specs`.
Never use `--no-validate`. Inspect actual synced main specs and archive paths. If sync
changes requirements or has ambiguous effects, pause for the existing approval gate.
The closeout has exact independent review and preflight but no recursive archive.
Failed implementation candidates remain undelivered and their receipts remain intact.

Completed means accepted delivery reached the authorized target. Cancelled and
closed-incomplete are explicit decisions, never Pass; preserve unmet tasks and do not
sync unimplemented proposed requirements into accepted specs. Record disposition in
external run/receipt history, not by falsifying implementation checkboxes.

## Worktrees, builds and evidence

Never write development work in frozen Qt5 or canonical. One writer per task/index;
one worktree per concurrent delivery, not per tool invocation. Reuse a sequential
checkout only after registry, role, refs, tracked/untracked/ignored contents and Git
operation checks establish safety. Submodule initialization is per worktree. Build
ownership follows source path/toolchain; no product build is required for policy-only
changes. Temporary audit environments have explicit cleanup owners.

Write durable evidence outside build directories from the start. Copy selected unique
screenshots/corpus/diagnostics with provenance and hashes, not entire generated builds.
Verify a second durable copy before destroying the only source or claiming cross-machine
recoverability. Local evidence alone is not backup. Unknown runtime provenance is a
retention reason. Paused/failed tasks retain staged work, checkpoints, findings and
handoff; generated builds may be released only when proven reconstructible and unused.

After verified remote delivery, automatically remove authorized temporary worktrees
using git worktree remove only if no unique work remains. Inspect ignored data too.
Checkpoint commits not reachable from canonical need a retained ref or verified bundle;
tree equivalence alone does not authorize deleting their history. Branch cleanup is
separate. No age-only deletion; preserve unknown items and report their reason.

## Branch Rules

Use `<type>/<short-kebab-case-description>` with feature, fix, docs, test, opt or
chore. Existing task names are preserved. Temporary verification branches have explicit
ownership; cleanup never expands into unrelated branches.

## Commit Rules

- Keep commit scope focused.
- Use Conventional Commits for commit messages:
  `<type>(optional-scope): <summary>`.
- Use these commit types: `feature`, `fix`, `docs`, `test`, `opt`, and `chore`.

## Pull Request Rules

Use the same Conventional Commit format for pull request titles.

Assign pull requests to `wisherhxl` and request review from `wisherhxl` when
opening them.

Use this pull request description template:

```markdown
## Summary

-

## Changes

-

## Verification

-

## Notes

-
```

## Pre-PR Checklist

- Keep the change focused on the requested task and avoid unrelated refactors.
- Run the relevant Debug or Release build workflow, with Release preferred
  before completion.
- Run the relevant `ctest` preset when tests exist.
- Run install verification when install or package behavior changes.
- Run `conan install` after dependency changes.
- Update the right Markdown file when behavior, workflow, or project policy
  changes.
- Do not commit generated build output.
- Review `git diff` and `git status` before committing or opening a pull
  request.
- Mention unverified areas or known limitations in the pull request `Notes`.

## Documentation Policy

- `README.md` should serve project users.
- `AGENTS.md` should serve contributors and coding agents as a short entry
  point.
- `docs/*.md` should own detailed contributor guidance by topic. Prefer updating
  an existing focused doc before expanding `AGENTS.md`.
- Treat repository Markdown as durable project guidance, not a transcript of the
  current conversation. Do not write conversational recaps, user-specific
  wording, or one-off implementation commentary into `AGENTS.md`, `README.md`,
  or `docs/*.md`; record stable rules, decisions, commands, constraints, and
  rationale that future contributors can rely on.
- Build instructions should be tested before being presented as the main path.
- Platform-specific notes should identify the affected platform explicitly.
- Update `README.md` when a change affects prerequisites, configure/build/test/
  install commands, module/app/proto creation, template consumption, public
  behavior, or examples.
- Update `AGENTS.md` only when a change affects instructions agents must know
  before editing the repository.
- Update `docs/agent-workflow.md` when a change affects contribution workflow,
  branch, commit, pull request, review, or task execution rules.
- Update `docs/architecture.md` when a change affects project structure, module
  design, system structure, or design rationale.
- Update `docs/build.md` when a change affects build prerequisites,
  dependencies, CMake/Conan instructions, install behavior, packaging, or build
  troubleshooting.
- Update `docs/testing.md` when a change affects test commands, test strategy,
  CI expectations, verification workflow, or test troubleshooting.
- Update `docs/coding-style.md` when a change affects coding style, naming,
  formatting, generated-file rules, or code organization.
- Update `docs/project-design-rules.md` when adding or changing explicit
  project design rules.
- If a change affects users and contributors, update `README.md` and the
  relevant contributor doc.
