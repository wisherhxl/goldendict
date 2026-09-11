# GoldenDict workspace

This directory is the local container for the GoldenDict project. It is not a
Git checkout itself.

## Project mission

- The current engineering goal is to reproduce all supported, observable
  functionality of the frozen Qt 5 GoldenDict baseline on the Qt 6 codebase.
- The Qt 5 reference is commit
  `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. It defines product behavior,
  feature scope, resources, settings, and compatibility evidence. Keep this
  checkout clean and unchanged.
- Functional parity does not require copying the Qt 5 source structure.
  Implement legacy behavior through the Qt 6 architecture and current project
  design rules.
- Linux is the first delivery target, but Windows and macOS restoration remain
  part of complete Qt 5 feature parity.
- Do not silently omit, redesign, or intentionally change legacy
  functionality. Any intentional divergence, unsupported legacy behavior, or
  scope exclusion requires explicit user approval and durable documentation.
- During the parity phase, prioritize closing documented parity gaps. Do not
  introduce unrelated new product functionality unless explicitly requested.
- The parity phase ends only when the user or canonical project documentation
  explicitly declares it complete. An agent must not infer completion from an
  individual task or a passing build.
- After parity is explicitly complete, Qt 6 becomes the sole baseline for all
  continuing development. New work must never branch from the frozen Qt 5
  baseline.

## Baselines and checkout routing

- `master/` is the read-only Qt 5 reference checkout pinned to commit
  `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
- `worktrees/feature-tiger-qt6-migration/` is the current Qt 6 development
  baseline.
- Create task branches and worktrees from the Qt 6 baseline under `worktrees/`.
  Keep both baseline checkouts clean.
- Replace `/` in branch names with `-` in worktree directory names.
- Do not advance the Qt 5 checkout when `origin/master` changes. Updating the
  frozen baseline requires a separately approved reconciliation task.
- Do not open `master/` as the project root for a normal development session.
  Start from this container or from the Qt 6 checkout.

## Sources of truth

Before planning parity work, read:

- `worktrees/feature-tiger-qt6-migration/AGENTS.md` for repository-wide rules.
- `worktrees/feature-tiger-qt6-migration/docs/migration.md` for the migration
  objective, provenance, phases, and gates.
- `worktrees/feature-tiger-qt6-migration/docs/feature-parity.md` for implemented
  and missing functionality.
- `worktrees/feature-tiger-qt6-migration/docs/porting-map.md` for Qt 5 evidence
  and Qt 6 ownership boundaries.
- `worktrees/feature-tiger-qt6-migration/docs/project-design-rules.md` for
  mandatory Qt 6 architecture rules.
- `worktrees/feature-tiger-qt6-migration/docs/agent-workflow.md` for task,
  branch, review, and documentation workflow.
- The focused build, test, architecture, or feature document relevant to the
  task.

## Delivery selection

New tasks may select `goldendict-candidate-v1` only after the workspace-local
`.ai-work-model-activation.json` is enabled and every listed file hash matches.
Use the Qt6 baseline's `docs/agent-workflow.md` as the single operational contract.
A missing, disabled or mismatched selector blocks the new profile. The old rules
remain available in `evidence/ai-work-model-v1/old-rules/` and Git base
`923bf0a28b9ec8292561c8ba63ccc53de67efee7`.

The rollout itself and all tasks predating activation use their old contracts.
Do not automatically commit, migrate or clear their staged/unstaged contents.
Read-only discussion needs no implementation permission. Approved implementation
and explicitly authorized publication/cleanup proceed without tool-by-tool prompts.
Parity still requires exact Qt5 evidence and applicable runtime/platform checks;
compilation alone never establishes parity.

## Start-of-task safety

- Fetch the remote before starting and inspect the Qt 6 baseline for
  uncommitted changes, unpushed commits, or divergence.
- Preserve all existing user changes. Do not resolve unexpected state by
  discarding or overwriting it.
- Initialize the `cmake` submodule in every new worktree with
  `git submodule update --init --recursive`.
- Follow the checkout's documented Conan/CMake workflow. Current presets use
  the checkout's ignored `build/` directory; use the container-level `builds/`
  only when an external build directory has been explicitly configured.
- Write repository documentation in English.

## Publication boundary

The explicitly approved new-profile contract permits local recoverable commits,
independent exact candidate review, task publication and same-result fast-forward
publication only to `feature/tiger-qt6-migration` on origin, after its preflight.
It does not authorize force pushes, PR creation, other shared targets, product
releases or destructive cleanup of unknown data. Preserve the frozen Qt5 reference.
Keep operation-specific authorization and independent receipts outside candidates.
Tasks under old contracts continue their audit-before-commit and separate
integration-audit route. Never use the new candidate's rules to deliver its rollout.
