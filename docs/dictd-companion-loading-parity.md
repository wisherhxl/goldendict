# Dictd Companion Content Loading Parity

## Requirement and impact check

R3.7a is a minor correction under approved `CRD-DICT-003` in the
[product baseline](qt6-product-baseline-crd.md). It restores valid Dictd data
loading when a supported companion's suffix does not describe its compression.
The parent [R3.7 gap](qt6-baseline-gap-audit.md) remains open.

Frozen Qt 5 commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8` is the product
authority. `dictdfiles.cc:637-638` selects `.dict` before `.dict.dz` and passes
that selected file to `dict_data_open`. `dictzip.c:290-364` distinguishes plain
data from gzip and RA dictzip by its bytes; `dictzip.c:653` reads RA chunks.
Qt 6 previously read every selected `.dict` as raw bytes, so real dictzip with
that name produced incorrect article bytes or an article-range error. Existing
`CompressDictdFixture` fixtures use ordinary gzip, without an RA table, and
therefore do not prove real dictzip compatibility.

The correction stays inside the existing private Dictd reader in Core. Data
selection retains its precedence, then content selects the existing bounded
plain or Zlib decompression path. Original source paths and snapshot ownership
are unchanged. There is no new interface, dependency, module, setting, GUI
behavior, or architecture decision. The existing format adapter and local
helper functions suffice; no additional design pattern is required.

Full-text artifacts need one private semantic stamp, following the existing
StarDict adapter pattern. A compressed `.dict` index range at offset 12/size 2
previously read the valid UTF-8 RA header letters rather than the article.
Such a document could persist successfully, and unchanged source stamps alone
would reuse it after the fix. Dictd now adds
`goldendict:dictd-content-detection-v1` only to its full-text source key. The
Reader's original two-file snapshot and dictionary identity remain unchanged.
Old Dictd artifacts rebuild once; subsequent opens reuse the corrected data.
No shared cache format or other backend is changed.

## Scope and acceptance

- Generated version-1 RA dictzip uses independently decompressible full-flush
  chunks and a valid gzip trailer. Independent test-side header/table parsing
  and raw inflation with the frozen reader's exact output-buffer size and
  flush mode verify the fixture without calling the product Reader.
- Both `.dict` and `.dict.dz` load real dictzip and plain bytes, with exact
  article ranges within, across, and beyond the first chunk.
- `.dict` takes precedence when both companions exist, even when it contains
  compressed data. Invalid selected data does not fall back to another file.
- Names, descriptions, counts, exact lookup bytes, and the two-file source
  snapshot follow the selected original data. Cold and warm full-text indexes
  preserve lookup and search results.
- An artifact containing the exact former RA-header interpretation, bound to
  unchanged source files, rebuilds to the decoded article and is then reused.
- Damaged compressed streams, checksum failures, and truncation produce the
  existing typed invalid-data errors. Existing bounds remain in force.

Other R3.7 metadata, index, rendering, identity, restart, and corruption variants
remain separate work. Ordinary gzip is an observed unresolved difference:
frozen `dictzip.c:620-629` returns `Cannot seek on pure gzip format files`, while
Qt 6 already decompresses ordinary gzip. This correction retains that support;
it neither approves the difference nor claims complete Dictd parity. Complete
RA-header validation and ordinary-gzip disposition are outside this unit.

## Delivery and verification

Implementation base: `4ddf676dbe092023a701c2c7cb2b425a40803c12`.
Task branch: `fix/qt6-dictd-companion-loading`.
The clean, prebuilt `worktrees/test-qt6-inspector-focus-parity` worktree is
reused sequentially after its previous delivery was integrated. Sole writer
ownership was transferred for this unit; its prior branch was not rewritten,
and no concurrent writer shares its index or ignored build output.

Run from this checkout with the existing Conan environment, fresh short
`TEMP`/`TMP` roots, and serial CTest:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release -j 12
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^(dictd_discovery_test|dictd_reader_test|dictd_dictionary_test|application_service_test)$' --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
python -m unittest discover -s scripts/tests -p '*_test.py'
git diff --check
```

Implementation and Windows verification completed on 2026-09-08:

- Full Release build passed; the final strengthened reader fixture check was
  rebuilt and the four focused CTest targets passed again (4/4).
- The cumulative serial Release CTest run passed 139/139 in 208.65 seconds.
  The only subsequent C++ change tightened the independent test inflater to
  the frozen buffer/flush settings; the final focused rerun passed 4/4.
- Python regressions passed 171 tests with two Windows symlink-capability
  skips. Local Markdown targets in all four affected documents exist, and
  `git diff --check` passed.
- Generated RA fixtures cover both suffixes, companion precedence and failure
  without fallback, exact within/cross/later-chunk bytes, aliases, names,
  descriptions, counts, original source snapshots, restart and cold/warm
  full-text results. Eight damaged-data rows pin header truncation, invalid
  DEFLATE blocks, checksum damage and truncated trailers under both names.
- The old-cache regression demonstrates a valid former artifact containing
  `RA` and its replacement with the decoded `OK` article without modifying
  original dictionary files; another open reuses the corrected artifact.

Local command output and the pre-fix reproduction are retained outside Git in
`evidence/r3.7a-20260908/` under the workspace container. Its README records
commands and distinguishes an initial QTest command-line error from the
reproduced reader failure. No dictionary corpus or frozen Qt 5 files were
modified. Linux execution is pending; portable C++/filesystem and existing
Zlib boundaries are preserved without Windows APIs in shared logic.

Final status (2026-09-08): **Complete and integrated** for bounded R3.7a Windows
acceptance. The implementation record above remains historical. Independent
completion and integration audits returned **Pass**, both bound to base
`4ddf676dbe092023a701c2c7cb2b425a40803c12` and tree
`ffe9cd94736c5ca8e7a0afa277a6c3c5e152c4c3`. The normal fast-forward push to
`origin/feature/tiger-qt6-migration` integrated commit
`ced8bb1abdbece6b07313b31095d01307670b8f9`. The separate integration auditor
passed the full Release build, focused CTest 4/4, cumulative serial CTest
139/139, and 171 Python tests with two Windows capability skips. Reports and
the actual target-push record are retained as `completion-audit.md`,
`integration-audit.md`, and `integration-verification.md` in the workspace's
`evidence/r3.7a-20260908/` directory. Parent R3.7 remains **Open**; Linux
execution, ordinary-gzip disposition, complete RA-header validation and
remaining Dictd variants remain pending.
