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
- In interactive collaboration, do not commit, push, or create pull requests
  automatically.
- If the user explicitly asks to commit, push, or create a pull request, do it
  without asking for another confirmation.
- For delegated implementation work, such as when the user asks an agent to
  make a plan and implement it, use the full branch, commit, push, and pull
  request flow when the work is ready.
- Create a local feature branch for delegated implementation work before
  committing.
- Do not push directly to `main` or `master` unless explicitly requested.

## Collaborative Task Lifecycle

Use this lifecycle for non-trivial implementation, architecture, build,
dependency, workflow, and design-rule changes. Tiny documentation edits, typo
fixes, and direct command requests may use a lighter workflow when the requested
action is clear.

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
   when behavior risk exists, run the relevant verification, review the final
   diff against `AGENTS.md`, applicable focused docs, and
   `docs/project-design-rules.md`, then commit, push, and open a pull request
   when the approved workflow calls for it.
6. `PR REVIEW`: treat pull request review as part of the task. If changes are
   requested, update the branch, rerun relevant checks, and update the pull
   request.
7. `DONE`: the task is complete only when the pull request is merged or the
   target is explicitly canceled.

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

Do not upload feature branches to a remote unless opening a pull request or
explicitly requested.

A feature branch may be deleted after its pull request is merged into `main` or
`master`.

## Commit Rules

- Keep commit scope focused.
- Use Conventional Commits for commit messages:
  `<type>(optional-scope): <summary>`.
- Use these commit types: `feature`, `fix`, `docs`, `test`, `opt`, and `chore`.

## Pull Request Rules

Use the same Conventional Commit format for pull request titles.

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
