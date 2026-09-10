# Tooling Integration Verification

This record covers the configuration candidate, not a product parity release or
baseline cutover. The independent staged-tree audit result is recorded separately
after the candidate is frozen. No commit or integration is part of this handoff.

## Policy preservation and consistency

The six existing documents were read in full. `docs/migration.md`,
`docs/feature-parity.md`, `docs/porting-map.md`, and
`docs/project-design-rules.md` are unchanged. Every original line of `AGENTS.md`
and `docs/agent-workflow.md` is retained in order; only tooling sections are added.
The product CRD was also fully read to resolve precedence of historical entries.

Retained authorities include the product baseline/approved CRDs, Qt 6 ownership,
Conan/CMake boundary, build/runtime launchers, English documentation, staged-tree
independent audit, branch/commit rules, and the Integration Contract. New rules
define tool responsibilities, discovery reuse, portability, exact-worktree
activation, generated metadata, and verification/archival boundaries.

No new competing product requirement set is introduced: this tooling-only change
uses `skip_specs: true`. OpenSpec config restates concise execution constraints
for artifact injection and explicitly points to canonical documents; it is not a
second product baseline. Generated OpenSpec skills are unchanged upstream output.

Historical Linux-first sequencing and old explicit-push wording remain in their
original documents. Their existing current CRD/workspace/workflow precedence is
preserved, not rewritten by tooling. No historical test counts, leaf completion
claims, or current execution assignment are promoted into permanent policy.
OpenSpec Verify and archive remain distinct from independent audit and Git gates.
Sufficient approved discovery is reused; planning-only workflow boundaries still
apply when a user asks only for a proposal. Explicit user scope controls execution.

## Checks and evidence

| Check | Result |
| --- | --- |
| OpenSpec generation | OpenSpec 1.13.0 initialized Codex using skills delivery and the seven-workflow custom profile; no global profile changes |
| Codex discovery | App-server `skills/list` with this worktree as `cwds` and `forceReload: true` returned all seven OpenSpec skills, enabled, with paths inside this worktree |
| OpenSpec config | Parsed with the installed YAML parser and strict `ProjectConfigSchema`; `schema: spec-driven`, four artifact rule lists, apply/archive guidance present |
| Change validation | `openspec validate agent-tooling-integration --strict --no-interactive` passed; zero product deltas explicitly accepted for tooling |
| Root isolation | `openspec context --json` resolved this worktree's local root |
| Serena parsing | Installed `ProjectConfig.load` parsed project and local override, with `goldendict`, `cpp`, LSP, and gitignore-aware indexing; did not rewrite project.yml |
| Portable settings | Project settings contain no machine path, fixed clangd version, or executable override; `ls_specific_settings` is empty |
| Serena activation | Explicit-path activation reported this tooling worktree; `get_current_config` reported LSP and language-server status ready |
| Local exclusion | `git check-ignore -v .serena/project.local.yml` matched `.serena/.gitignore`; `git ls-files .serena/project.local.yml` returned no entries |
| clangd executable | Local executable returned clangd 22.1.8 for Windows x86_64 |
| Documentation | Original-policy preservation and local Markdown path checks passed; `git diff --check` passed |
| Product/reference safety | No business-code/build changes; frozen Qt 5 tracked contents unchanged; canonical Qt 6 baseline unchanged and clean |

Local reproduction helpers and detailed discovery output are under ignored
`build/agent-tooling/`: `check-config.cjs`, `check-codex.cjs`, and
`codex-skills.json`. These are inspection evidence, not portable project tooling
or committed dependencies. The staged diff and audit provide final attribution.

## Limits

- The optional local executable override is parsed, but Serena's machine trust
  list is empty. The running session therefore ignores project/local LS-specific
  settings; activation does not prove it selected clangd 22. No global trust
  settings were changed. Trust remains a separate machine-level decision.
- No `compile_commands.json` or `.clangd` was generated. Complete C++ semantic
  readiness is not claimed; the language-server ready state is not proof of Qt
  include/define correctness or complete references.
- No product build, runtime/parity suite, Linux execution, or macOS execution was
  performed. This candidate changes only documentation/configuration/skills, so
  configuration and policy checks are the proportionate gate.
- Commit, push, integration, and archive await the separately authorized next
  step; the canonical baseline has not inherited this candidate yet.
