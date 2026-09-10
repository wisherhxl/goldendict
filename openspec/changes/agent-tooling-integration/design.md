## Context

See [proposal.md](proposal.md) for scope and acceptance. This is one tooling
delivery under the existing [agent workflow](../../../docs/agent-workflow.md).
It changes contributor tooling policy, not product requirements or architecture.

## Goals / Non-Goals

Keep the three tool responsibilities distinct, configuration portable, and
future task worktree inheritance dependent only on Git-tracked files.
Do not duplicate product specs, vendor Superpowers, modify the frozen reference,
change production builds, or promise semantic completeness without a database.

## Decisions

- Keep the root agent entry concise and place operational detail in the existing
  workflow document. References preserve canonical product and design authority;
  copying policy into a second requirements hierarchy would introduce drift.
- Generate Codex skills through OpenSpec with the existing skills-only custom
  profile, retaining core workflows and Verify. Keep generated skills unchanged;
  project context and rules carry GoldenDict-specific constraints.
- Use the installed Serena project schema with portable defaults. An ignored
  local override selects an optional machine clangd; a committed executable path
  would break cross-platform/worktree portability. Trust remains machine-owned.
- Do not generate a database for this docs/configuration task. Future C++ tasks
  obtain and test their own build metadata, without altering working production
  build configurations merely to enable semantic analysis.

## Risks / Trade-offs

- Tool installation/global profile differences: verify skill discovery and
  parsing, document deliberate generator refresh, retain generated skills in Git.
- Missing database: report activation and executable checks separately from
  complete semantic readiness.
- Repeated project names: activate by exact path/cwd and verify the reported root.
- Upstream workflow instructions can overlap project gates: project authority and
  explicit user scope remain controlling; Verify is not the independent audit.

## Delivery And Rollback

Review and audit the isolated staged tooling candidate before any authorized
commit. Do not integrate this candidate in this task. Later approved integration
uses the existing contract. Any rollback is a separately reviewed tooling revert;
it does not modify product data or the frozen reference.

## Readiness

Ready for this single configuration/documentation unit under the approved tooling
scope. Ownership and acceptance checks are defined above and in tasks.md; no
production-code architecture decision is introduced. The six canonical documents
and governing product CRD have been fully reviewed; historical platform/test
snapshots are referenced through current authority rather than copied as rules.
