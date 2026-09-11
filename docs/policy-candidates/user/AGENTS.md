# Global Engineering Policy Entry Point

This file contains only the global rules that must always be loaded. Detailed
procedures live under `policies/` and are read on demand. Resolve policy paths
relative to the directory containing this file.

Project instructions may add or specialize architecture, testing, safety,
review, release, commit, and push rules. They must not silently weaken
requirement-first delivery, independent completion audit, or
audit-before-commit. An exception requires explicit user approval and durable
documentation.

## Required policy routing

- Before requirements planning, readiness review, implementation, completion
  audit, commit, or push for any engineering change, read
  `policies/engineering-delivery.md` completely and follow the lifecycle for
  the applicable change class.
- Before architecture work, production-code design, implementation, or code
  audit, read `policies/software-design.md` completely and apply its SOLID and
  design-pattern selection rules.
- When a question, conflict, audit finding, or blocker requires user direction,
  read `policies/issue-resolution.md` completely before presenting it.
- Read the applicable policy again after context loss or compaction if its full
  contents are no longer available in the active context.
- Read project-level instructions and canonical project documents required by
  those instructions. More specific project rules take precedence where this
  entry point or the delivery policy explicitly allows project specialization.

Purely conversational answers that make no project change and perform no
delivery audit do not require the engineering-delivery policy.

## Explicit optional delivery profiles

The default below remains strict staged-tree review for every project. A user
may explicitly approve a project-scoped candidate-delivery profile under
`policies/engineering-delivery.md`. Only an activated, hash-verified project
selector may opt in; merely possessing proposed policy files never opts in.
The optional profile does not authorize its own rollout or convert old tasks.

## Core delivery guarantees

- `Baseline`, `CRD`, and `Minor correction` are distinct change scopes, not
  mandatory serial documents. A baseline governs a major development scope; a
  CRD changes requirements within an existing baseline; a minor correction
  conforms implementation to an already approved requirement.
- Do not create a baseline or CRD when the governing approved requirement
  already defines the correction. Reclassify work when investigation reveals
  a requirement or major-scope decision.
- Requirement-changing work must be confirmed and audited for development
  readiness before production-code edits.
- Completed implementation must pass an independent, fresh, no-history,
  non-modifying completion audit before commit.
- Audit failure returns the work to development. The auditor never repairs the
  work, and any tracked change after Pass invalidates that Pass.
- In Git repositories, put each concurrently developed delivery on its own
  branch and dedicated worktree with one development writer. A read-only audit
  may inspect the same worktree.
- Stage the complete isolated delivery before audit and bind the audit to its
  base Commit ID and staged Tree ID. Commit directly from that unchanged index
  after Pass; a changed base or tree requires a new audit.
- Combine audited delivery branches one at a time in a dedicated integration
  worktree with one integration writer. Conflicts return to the original
  development worktree; the combined candidate requires its own independent
  integration audit before the target baseline advances.
- Keep commits scoped to coherent, independently verifiable functional units.

## Commit and push authority

Project-level commit and push rules take precedence. When a project defines no
commit or push policy, an independent audit Pass is standing authority for the
development thread to commit the audited delivery and push it normally to the
remote feature branch without requesting additional confirmation.

Autonomous integration into a shared development baseline requires a durable
project-level Integration Contract naming the allowed target and protected
branches, integration method, verification, audit, push, conflict, rollback,
and cleanup rules. Without that contract, stop after pushing the audited
delivery branch; do not infer integration authority.

Do not force-push, rewrite published history, merge, or open a pull request
unless the user or applicable project policy explicitly authorizes it.

## Documentation language

- Use English for all design work and durable documentation, including
  baselines, CRDs, plans, architecture decisions, audit reports, tests,
  migration and operational documents, release notes, and
  documentation-oriented code comments.
- Keep terminology and requirement identifiers consistent in English across
  planning, implementation, audit, and release evidence.
- User conversation may use the user's preferred language. Product UI
  translations, localization resources, quoted evidence, and source material
  that must preserve its original language are allowed.
- A localized documentation deliverable requires explicit user direction;
  retain English as the canonical engineering/design version unless the user
  explicitly changes that policy.

## Maintenance

Keep this entry point concise. Put detailed or conditional workflow in the
focused policy documents rather than expanding this file. Write all policy
documents in English.
