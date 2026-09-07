# Observer Platform Test Isolation Correction

Change class: Minor correction.
Governing requirements: R2 test-gate stabilization in
[the baseline gap audit](qt6-baseline-gap-audit.md), and deterministic isolated
acceptance checks under `CRD-TEST-REAL-002` and `CRD-TEST-REAL-003` in
[the approved product CRD](qt6-product-baseline-crd.md).
Dependency base: `3b03f09c7635940f86a07813308e635cb1d7a0cc`.

## Issue and impact check

Windows synthetic observer tests replace `subprocess.run` but several still
query the real host through `platform.system`. On Python 3.14 Windows system
discovery can fall back to a shell subprocess. That unrelated request then
enters the observer-process test double, which expects observer-specific
environment arguments and raises `KeyError: env`. The full suite produced
11 errors during integration verification while the same command passed in
the preceding completion audit. Python 3.12 also passes; changing interpreters
does not remove the test isolation defect.

Impact check: Ready. No product, architecture, supported-platform or acceptance
assertion change is needed. The test class uses Windows fixtures by default;
its explicit POSIX case already chooses Linux. Bind the default fixture to
Windows in setup, retain the explicit POSIX override, and verify that neither
path consults real host discovery. All observation process assertions remain.

## Verification plan

- Run `python scripts/tests/qt5_real_dictionary_observer_test.py`.
- Run `python -m unittest discover -s scripts/tests -p '*_test.py'` with the
  installed Python 3.14 and Python 3.12 interpreters.
- Prove the platform boundary with a regression that makes host uname
  discovery fail if invoked, and checks both Windows and POSIX config paths.
- Run `git diff --check`; obtain a fresh read-only completion audit before
  committing this independent test-only unit. No product rebuild is necessary.

## Implementation verification

The focused suite passes 15 tests. The complete script gate passes 171 tests
with two expected platform-condition skips on both Python 3.14.7 and 3.12.14.
The regression rejects real uname discovery while asserting the original
Windows and POSIX profile paths. The explicit POSIX observation test still
executes its original process/profile assertions. Production files and
acceptance comparisons are unchanged.
