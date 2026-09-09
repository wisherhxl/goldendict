# Dictd Index Row Recovery Parity

## Requirement and impact check

R3.7b is a minor correction under approved `CRD-DICT-003` and
`CRD-COMPAT-001` in the [product baseline](qt6-product-baseline-crd.md).
The parent [R3.7 gap](qt6-baseline-gap-audit.md) remains open.

Frozen Qt 5 commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` defines the
behavior: `dictdfiles.cc:684-699` skips index rows containing more than three
tabs, and `dictdfiles.cc:751-760` skips rows containing zero or one tab.
Processing continues at the following row. Accepted rows have two or three
tabs; `dictdfiles.cc:701-709` indexes their headwords and optional original
headword and increments their counts. Qt 6 previously rejected the entire
dictionary when a row has a different field count, hiding otherwise usable
entries from discovery and lookup.

Empty accepted-record sets remain valid: frozen `btreeidx.cc:1249-1283` builds
the index for a zero-item set, and `btreeidx.cc:750-751` explicitly handles an
empty leaf for an entirely empty tree.

The approved correction changes only the row-shape decision in Core's private
Dictd Reader. Existing accepted-row parsing, metadata, aliases, limits, typed
errors, physical source ordinals, and source-file ownership remain in place.
The existing private format adapter owns this behavior under the shared-library
and GUI boundary; no new abstraction, interface, module, dependency, setting,
or architecture decision is needed.

No full-text semantic stamp change is required. Previously accepted inputs
produce identical documents. Inputs newly accepted by this correction could
not finish opening under the former parser, so it could not publish a full-text
artifact for those inputs. Existing source snapshots invalidate artifacts when
the original index changes.

## Scope and acceptance

- Skip blank rows and rows with zero, one, or more than three tabs before,
  between, and after valid records, including a final row without a newline.
- Preserve valid three- and four-field records, aliases, metadata, counts,
  exact article bytes, source identity, and physical full-text record ordinals.
- Empty and entirely skipped indexes open with no articles or headwords and
  the filename-derived dictionary name.
- Accepted row shapes still reject malformed base64, invalid UTF-8, and
  out-of-range articles with existing typed errors and physical line numbers.
  Existing size limits stay in force, including for malformed row shapes.
- Cold and warm full-text opens retain results, document identity and original
  source snapshots. Service discovery and lookup retain valid Dictd entries
  alongside another format when the Dictd index contains a skipped row.

Ordinary-gzip disposition, complete RA-header validation, other metadata and
alias-count semantics, discovery, rendering and indexing differences remain
separate R3.7 work. This unit neither approves those differences nor closes
R3.7. Linux execution remains pending; shared changes use portable Core code.

## Delivery and verification

Implementation base: `689d58e202d2579363269f1983c91728fa19144a`.
Task branch: `fix/qt6-dictd-index-row-recovery`.
The clean prebuilt `worktrees/test-qt6-inspector-focus-parity` worktree is
reused sequentially after explicit sole-writer transfer. The previous branch
is preserved, and no other writer shares the index or build directory.

Run from the owning checkout with fresh short `TEMP`/`TMP` directories:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release -j 12
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(dictd_discovery_test|dictd_reader_test|dictd_dictionary_test|application_service_test)$' --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
python -m unittest discover -s scripts/tests -p '*_test.py'
git diff --check
```

Status: **Implementation and Windows verification complete** (2026-09-09).
The Release build passed. Final focused CTest passed 4/4 in 3.38 seconds,
including the row-shape matrix, empty-index results, retained strict
validation, source ordinals, cold/warm full-text checks, and mixed-service
discovery/lookup. Final cumulative serial CTest passed 139/139 in 64.28 seconds.

An earlier full serial run passed 136 of 139 in 192.39 seconds;
`aard_dictionary_test`,
`application_service_test`, and `xdxf_reader_test` did not start (`BAD_COMMAND`).
The owning launcher's direct service-test attempt reported WinError 4551:
application control policy blocked the file. The user subsequently disabled
Smart App Control personally and explicitly requested a retry. Read-only
verification confirmed the changed environment; unchanged original binaries
then passed the three previously blocked tests and the complete focused and
cumulative reruns above. Agents made no security-setting or binary changes to
resolve the block. Historical failed logs remain preserved.

Python regressions passed 171 tests with two Windows symlink-capability skips.
All 30 local Markdown targets in the three affected documents exist, and
`git diff --check` passed. Generated fixtures provide this unit's evidence;
no real Dictd corpus acceptance is claimed. The frozen Qt 5 checkout and
original dictionary corpus were not changed.

Pre-fix reader verification failed on the newly added recovery test and
passed after the correction. The initial direct QTest invocation also had a
separate unquoted-output-argument error; the subsequent CTest reproduction
has no such command-line error. Local logs distinguish the initial missing
test include, corrected test build, pre-fix verification, final build, focused
runs, full run, and Python checks in the workspace's
`evidence/r3.7b-20260909/` directory, outside Git.

Completion audit, commit, push, and independent integration audit follow the
project delivery policy. Their reports and exact delivery identities belong
in the workspace's `evidence/r3.7b-20260909/` directory; implementation
verification alone does not assert an audit Pass or integration.
R3.7 and Linux acceptance remain open.
