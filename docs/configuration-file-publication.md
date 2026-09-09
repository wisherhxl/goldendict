# Configuration File Publication

## Governing behavior and scope

This correction conforms to `CRD-COMPAT-002`, `CRD-COMPAT-003`, and
`CRD-COMPAT-005` in the [approved baseline](qt6-product-baseline-crd.md):
configuration and persisted user state survive saving and upgrade, unknown
supported records remain intact, and persistence failures remain diagnosable
without replacing working data.

The frozen Qt 5 reference is commit
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`. Its `config.cc` writes the
serialized document to a temporary, closes it, and calls `renameAtomically`.
`atomic_rename.cc` uses `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` on
Windows and `rename` on POSIX. The Qt 6 configuration schema and serializer
remain authoritative for current files; this correction changes neither
their bytes nor the supported settings and legacy import policy.

## Private ownership and failure boundary

`SaveConfiguration` retains validation and serialization. Its concrete
byte-publication tail belongs to the non-installed Core helper
`src/application/configuration_file.*`, following the existing
[Core/GUI ownership boundary](project-design-rules.md#shared-library-and-gui-boundary).
The public function, configuration paths, sibling `.tmp` name, and
`std::filesystem::rename` replacement semantics are unchanged.

On Windows, the CRT writer is created with `wbN`. `N` makes the file handle
noninheritable during creation, so a concurrent child launch cannot keep the
configuration temporary open after its owning writer closes. `_SH_DENYNO`
retains the preceding MSVC ofstream's read/write sharing mode. Writes and
close are checked before publication. POSIX retains the preceding binary,
truncating ofstream behavior.

Opening, writing, closing, and replacement failures report a
`std::filesystem::filesystem_error` with the primary operation, affected path
or paths, and error code. This remains within the existing runtime-exception
boundary. A failed replacement preserves the old target. Cleanup uses the
nonthrowing filesystem overload and never replaces the primary exception;
when cleanup is denied, the unpublished sibling temporary can remain for a
later save. Failure to open a temporary does not claim ownership or remove it.
No retry, GUI modal-policy change, file-schema change, durability guarantee,
or general transaction-persistence refactor is introduced.

The one optional source-private checkpoint runs after writing while the
temporary remains open. It is not a flush, sync, or durable checkpoint. Tests
use it for deterministic process-inheritance and file-sharing interleavings;
production callers pass no callback. There is no installed or global test hook.

## Windows diagnosis and limits

An unchanged Qt 6 baseline reproduced the dictionary-bar smoke timeout with
fresh D-drive temporary profiles while C-drive profiles passed. The original
rename returned Windows error 32 (`ERROR_SHARING_VIOLATION`); throwing cleanup
then masked that error. The GUI caught the cleanup exception and waited in its
existing error dialog until the smoke timed out.

Restart Manager and native process identity/times identified an exited
GoldenDict child, or possible clone, whose parent was the active application.
The holder's image query failed; the exact launcher and ordinary-child versus
clone distinction were not established. No external process or security
product is attributed as the cause.

A separate real-child probe proved ordinary inheritance of the prior
ofstream handle and exact old-target preservation after rename/cleanup denial.
A controlled actual-smoke comparison then reproduced failure with both the
original ofstream and an equivalent `wb` FILE writer; selecting `wbN` in the
same diagnostic binary passed repeated fresh C/D profiles. This supports the
noninheritability correction without overstating the holder attribution.
All temporary diagnostics and experimental environment switches were removed.

## Regression verification

The existing `application_service_test` covers:

- first save, identical repeated save, changed save/reload, and opaque-field
  byte preservation;
- a while-open failure and an actual directory-target replacement failure,
  retaining the primary operation/code/paths and earlier data;
- on Windows, a real child alive through writer close and publication,
  with the original inheritable ofstream as a failing negative control; and
- on Windows, an actual target sharing denial, with and without simultaneous
  temporary cleanup denial, exact old-file preservation, retained primary
  replacement diagnostics, and recovery after the test-owned handles close.

Run the owning Release build and full serial suite. Also repeat the
dictionary-bar smoke using fresh temporary roots on both Windows drives:

```powershell
.\run_with_conan.ps1 --build-type Release --with-build-environment -- cmake --build --preset conan-release
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R 'application_service_test|user_state_upgrade_test|dictionary_bar_smoke' --output-on-failure
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 --output-on-failure
```

Set both `TEMP` and `TMP` to the same newly created disposable directory for
each drive/profile repetition. Preserve failing logs and profiles. A passing
Windows run does not establish Linux or macOS runtime acceptance, and does not
complete the separate dictionary parity delivery that exposed the failure.
