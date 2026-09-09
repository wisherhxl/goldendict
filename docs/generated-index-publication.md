# Generated Index Publication Safety

## Minor-correction design

This correction conforms to [CRD-FTS-003 and CRD-COMPAT-004/005](qt6-product-baseline-crd.md).
The generated-index replacement fallback deleted the working target after a
failed rename, then retried. A second failure lost the prior index. The
required outcome is one atomic replacement attempt, preserving the prior file
on failure and reporting `GeneratedIndexError` with the target path.

Ownership remains in the private Core dictionary component. Extract only the
prepared-file publication phase into `PublishGeneratedIndex` in the existing
non-installed private header, called by `StoreGeneratedIndex`. This narrow
phase boundary permits deterministic filesystem failure tests without racing
temporary-name discovery or adding injection callbacks. It accepts a closed,
fully written temporary file beside its distinct target. On failure it cleans
only that temporary file when the filesystem permits it. No installed API,
dependency, index format, source stamp, checksum, identity, setting, resource,
or architecture changes. No additional design pattern is needed.

The actual MSVC 14.44 toolchain's `include/filesystem:3662-3676` documents
replacement of an existing non-directory target. Its
`crt/src/stl/filesystem.cpp:720-726` implements rename with `MoveFileExW` and
`MOVEFILE_REPLACE_EXISTING`. Same-directory publication does not need the
cross-volume copy option or a delete-and-retry fallback. This repairs the Qt 6
cache publication contract; it does not import Qt 5 index serialization or
declare any additional feature parity complete.

## Acceptance and verification

The existing `full_text_index_test` target verifies first publication,
normal replacement, unchanged source bytes, stale/corrupt detection and
rebuilding, and typed path-bearing failure. A portable nonempty-directory
target at publication time proves failed rename cleans a removable
temporary file and preserves the target. On Windows a prepared source opened
without delete sharing will deterministically reject rename while the prior
target is independently verified deletable. Its exact old bytes must survive;
the source remains while locked and is removable after the handle closes.
This test must fail with the former delete-and-retry implementation. Platform
guards apply only to the Windows sharing-mode case, not portable coverage.

On Windows 11 with MSVC 14.44 and Qt 6.11.1, all three focused publication
cases pass. Restoring the former fallback causes the source-lock case to fail
when reopening the original target, demonstrating actual loss of the old
file. Restoring the correction makes that same assertion and exact-byte
comparison pass. Test registrations and platform scope are unchanged.

Run from the owning checkout with short unique `TEMP` and `TMP` directories:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release -j 12
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R 'full_text_index_test|stardict_reader_test|dictd_reader_test' --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
python -m unittest discover -s scripts/tests -p '*_test.py'
git diff --check
```

Validate the document's repository links. Windows execution does not claim
Linux/macOS execution; those remain separate platform gates. Build artifacts,
logs and audit identities are retained outside tracked documentation. Stage the
complete correction and freeze its base commit and staged tree for independent
completion audit before commit or push.
