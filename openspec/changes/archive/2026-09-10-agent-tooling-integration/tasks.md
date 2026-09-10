## 1. Isolated tooling configuration

- [x] 1.1 Create the requested Qt 6 task branch/worktree from the clean synchronized baseline; verify branch, HEAD, status, and initialized cmake submodule.
- [x] 1.2 Review the six existing canonical documents completely and preserve existing policy; verify the tracked diff contains only additive agent/workflow guidance.
- [x] 1.3 Initialize Codex OpenSpec skills only with core workflows and Verify; verify seven repository skill files and no vendored Superpowers.
- [x] 1.4 Add migration context, artifact rules, and operation guidance; verify OpenSpec schema parsing and strict change validation.
- [x] 1.5 Add portable Serena settings and ignored local override; verify schema loading, required values, and gitignore/staging exclusion.

## 2. Verification and review

- [x] 2.1 Verify Codex discovers the seven enabled skills from this exact worktree, using the app-server skills/list endpoint.
- [x] 2.2 Activate this exact worktree in Serena and verify the reported root; check local clangd --version, report trust and missing-database limitations separately.
- [x] 2.3 Check policy preservation, local Markdown links, whitespace, portable tracked configuration, and absence of business-code edits; record results in verification.md.

## 3. Review boundary

Prepare the complete staged configuration snapshot for independent review. The
fresh no-history audit must bind to its base Commit ID and staged Tree ID. Record
that result outside the frozen candidate so reporting cannot invalidate it.
Present the complete diff and verification results, then stop without commit,
push, integration, or archive. These delivery gates remain separate from the
implementation checkboxes above.
