# Agent Workflow

This document contains Tiger's detailed agent workflow, branch, commit, pull
request, review, and documentation rules.

## Working Rules

- Keep changes scoped to the requested task.
- Preserve existing user changes. Do not revert unrelated files.
- Prefer existing project patterns over introducing new structure.
- Update documentation when changing project behavior, build steps, layout, or
  contributor workflow.
- Avoid guessing project policy. If a decision is not documented in
  `AGENTS.md`, this repository, or these docs, ask before encoding it as a rule.
- Do not commit generated build output.
- Before starting a new task, fetch the remote and make sure the local base
  branch is up to date.
- If the local branch has unpushed commits, uncommitted changes, or diverges
  from the remote, inspect the state and resolve it before creating a feature
  branch.
- Follow the applicable workspace or user-approved commit and push policy. If
  neither grants authority, do not commit, push, or create pull requests
  automatically.
- If the user explicitly asks to commit, push, or create a pull request, do it
  without asking for another confirmation.
- For delegated implementation work, such as when the user asks an agent to
  make a plan and implement it, use the full isolated-delivery, audit, commit,
  push, and applicable integration or pull-request flow when the work is
  ready.
- Create a local feature branch for delegated implementation work before
  committing.
- Do not push directly to `main` or `master` unless explicitly requested.

## Collaborative Task Lifecycle

Use this lifecycle for non-trivial implementation, architecture, build,
dependency, workflow, and design-rule changes. Tiny documentation edits, typo
fixes, and direct command requests may use proportionately lighter discussion,
requirements, planning, and verification when the requested action is clear.
The lighter path never waives delivery isolation, a fixed staged snapshot bound
to its Base Commit ID and Tree ID, an independent completion audit, or
commit-after-Pass order for tracked changes.

Use explicit stage changes so discussion, planning, implementation, and review
do not blur together.

1. `DISCUSSING`: clarify the target, constraints, risks, alternatives, success
   criteria, and relevant best practice. Read the high-level governing docs
   early, including `AGENTS.md`, `docs/project-design-rules.md`, and any focused
   doc relevant to the target. Ask when intent or project policy is unclear.
   Do not guess, write a plan, edit files, commit, push, or create a pull
   request in this stage.
2. `AWAITING PLAN APPROVAL`: when the direction seems clear enough to plan,
   propose the stage change to `PROPOSING PLAN` and stop. Keep this gate light:
   ask for approval to write the plan, or ask what still needs discussion.
3. `PROPOSING PLAN`: read the nearby code and focused docs needed to make the
   plan concrete. Write a plan that covers scope, likely files, test strategy,
   verification commands, expected pull request shape, and the repository rules
   governing the change. Do not edit files in this stage.
4. `PLAN REVIEW`: revise the plan through discussion until implementation is
   explicitly approved. Treat plan feedback as part of this stage. Do not edit
   files, commit, push, or create a pull request while the plan is still being
   adjusted.
5. `IMPLEMENTING`: after explicit approval, execute the approved plan without
   requiring another confirmation. Implement the change, add or update tests
   when behavior risk exists, run the relevant verification, fix verification
   failures within the approved scope, and report out-of-scope findings with
   impact and options before expanding the change. Review the final diff
   against `AGENTS.md`, applicable focused docs, and
   `docs/project-design-rules.md`. Stage only the complete functional unit,
   record its Base Commit ID and staged Tree ID, and stop changing it before
   the completion audit.
6. `COMPLETION AUDIT`: request a fresh, no-history, read-only audit of the
   staged delivery against its governing requirements and verification plan.
   The auditor must not change tracked files, the index, commits, branches, or
   remotes. A `Fail` returns the delivery to `IMPLEMENTING`; after rework,
   verification and a new independent audit are required. The `Pass` binds to
   both the recorded Base Commit ID and staged Tree ID. Any tracked change,
   index change, or `HEAD` movement after `Pass` invalidates the result.
7. `DELIVERING`: after `Pass`, confirm that `HEAD` still equals the audited Base
   Commit ID and `git write-tree` still equals the audited Tree ID. Commit
   exactly that staged snapshot, and push the task branch when the applicable
   project policy authorizes it. Then use the authorized Integration Contract
   or open a pull request when the approved workflow calls for one.
8. `INTEGRATION OR PR REVIEW`: treat integration and pull request review as
   part of the task. If changes are
   requested, update the branch, rerun relevant checks, and update the pull
   request or integration candidate, including a new completion audit whenever
   tracked delivery content changes.
9. `DONE`: the task is complete only when its audited delivery reaches the
   project-authorized target, its pull request is merged, or the target is
   explicitly canceled.

Before proposing a stage change or opening a pull request, identify conflicts
with documented project policy. If the requested direction or final diff
conflicts with `AGENTS.md`, `docs/project-design-rules.md`, or another focused
doc, stop and ask for direction instead of guessing.

## Branch Rules

Name task branches as `<type>/<short-kebab-case-description>`.

Use these branch types:

- `feature/` for new user-visible or template functionality;
- `fix/` for bug fixes;
- `docs/` for documentation-only changes;
- `test/` for test-only changes;
- `opt/` for optimization work;
- `chore/` for maintenance, tooling, dependency, or cleanup work.

Push audited task branches when the applicable workspace or user-approved
policy authorizes it. Otherwise, do not upload a feature branch unless opening
a pull request or explicitly requested.

A task branch may be deleted after its audited delivery reaches the authorized
target or its pull request is merged.

## Integration Contract

This contract governs autonomous integration during the Qt 6 migration. It
does not authorize releases or changes to the frozen Qt 5 baseline.

### Branch authority

- The sole autonomous integration target is
  `refs/heads/feature/tiger-qt6-migration` on the configured `origin` remote.
- `main`, `master`, release branches, tags, and every other shared branch are
  protected. Advancing any of them requires explicit user direction or a
  separately approved contract.
- A pull request is not required for the authorized Qt 6 target. Pull requests,
  releases, force pushes, and history rewriting remain manual operations.

### Eligible deliveries

- Integrate only a cohesive task branch created from the Qt 6 baseline and
  pushed to `origin` after an independent completion audit returned `Pass`.
- Record the delivery commit, its audited Tree ID, its base commit, the audit
  result, and verification evidence. The pushed commit tree must equal the
  audited Tree ID.
- Integrate delivery branches one at a time in documented dependency order.

### Integration method

- Use a dedicated integration branch and worktree created from the current
  remote target. Allow exactly one writer and keep the worktree clean between
  candidates.
- The only autonomous integration method is fast-forward. The delivery must be
  a direct descendant of the current remote target, and advancing the target
  must not create a merge commit.
- Do not merge, cherry-pick, squash, rebase, resolve conflicts, or modify
  delivery content in the integration worktree. If the target has advanced or
  the candidate is not fast-forwardable, return the delivery to its original
  development worktree. Update it from the current target, rerun verification,
  obtain a new no-history completion audit, and push the replacement delivery.

### Verification and integration audit

- Fetch `origin` immediately before preparing the candidate. Record the remote
  target as the integration Base Commit ID and the candidate commit and Tree
  ID. Recheck the remote target before pushing; any movement invalidates the
  integration audit.
- Run the delivery's targeted checks and the cumulative checks relevant to the
  combined Qt 6 baseline. Documentation-only workflow changes require at least
  Markdown link/path validation, `git diff --check`, and consistency review of
  the affected policy files; they do not require a product rebuild.
- Request a separate fresh, no-history, read-only integration audit. Give it
  only the repository/worktree location, target and candidate identities,
  governing documents, and verification commands. The auditor must not change
  tracked files, the index, commits, branches, or remotes.
- A `Pass` must bind to the Integration Base Commit ID and candidate Tree ID
  and confirm branch authority, delivery-audit evidence, fast-forwardability,
  verification results, protected-branch safety, and absence of unrelated
  changes. Any tracked change or target movement after `Pass` invalidates it.

### Finalization, failure, and cleanup

- After `Pass`, verify the candidate commit still has the audited Tree ID and
  push it normally with an explicit refspec to
  `refs/heads/feature/tiger-qt6-migration`. Never force-push.
- On conflict, divergence, failed verification, or failed audit, do not repair
  feature content in the integration worktree and do not advance the target.
  Preserve the report and return the delivery to its development worktree.
- Do not rewrite or reset a published target to roll back a regression. Create
  a separate audited revert delivery and integrate it through this contract.
- After a successful push, record the target commit and evidence, restore a
  clean integration worktree, and remove temporary integration worktrees and
  branches when they are no longer needed. Remove a remote task branch only
  when project policy or explicit user direction authorizes that cleanup.

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
