# W3.1: inventory and full-text dictionary projection test ownership

Approved scope: the first W3/A4 family only, from
feba6359e2f630cffde9c02a1e20d0765e163485. W3 remains in progress; no second
family, W4, merge or push is selected. Original Draft and W1/W2 artifacts remain
historical and unchanged. This is an existing-contract responsibility migration,
not a public requirement change or new application architecture.

## First-family selection and boundary

Select RunFullTextDictionaryProjectionSmokeCheck and its internal command-line,
fixture-selection and CTest dispatch entries. Its cohesive contract is dictionary
scope composition for all supported dictionaries, an unchecked toolbar entry,
hidden toolbar fallback, and a group containing muted/unresolved dictionaries.
It must neither start a lookup request nor show the query composer.

This family has a single real MainWindow query-composition path, uses the existing
Dictd fixture and core facade, and has no production startup/recovery assertion,
restart protocol, external process execution or bespoke failure injector. It can
leave production completely without extracting the startup coordinator or touching
lookup ownership. Selection is based on those boundaries, not method length.

Move all steps, expected results, event pumping and assertions into a dedicated
test target. Reuse the production presentation source/link closure already exposed
for W1/W2 targets, actual Qt resources/MOC and real Core facade activation. No
production .cpp includes, altered access macros, duplicate business algorithm,
test-specific production implementation or application-to-runner dependency.

Use a source-private, test-target-only access class with three bounded operations:
compose the existing full-text query, select/refresh the existing group path, and
read active request count. It cannot replace the facade, arbitrary window state or
transaction internals. UI toolbar/action discovery uses real QObject objects.
Production gains only the narrow friendship declaration, not test step code.

The runner owns isolated configuration/index/cache/temporary data, actual
QApplication/scheme registration, source configuration, facade lifetime and
MainWindow initialization. Preserve the original enabled external-program catalog
entry and disabled online-source entries as test fixtures; no command is executed.
Those fixture values stay shared in main.cpp for other unselected smoke families.

Keep the original CTest name as the compatibility entry, redirecting it to the new
runner. The old --full-text-dictionary-projection-smoke switch is an internal test
dispatch, not a documented product diagnostic. Remove it from production dispatch,
smoke classification and fixture selection. Do not delete any other diagnostic or
legacy smoke option. No dedicated CI/script call of this flag exists beyond its
CMake entry; whole-suite CTest consumers inherit the replacement.

## Verification and regression prevention

Baseline: build actual goldendict and existing projection composer test; run the
old full-text projection smoke and W1/W2/Network tests in isolated data. Pure
migration requires assertion mapping, not artificial product red evidence.

BUILD_TESTS is the actual switch. Keep build/Release ON and independent
build/W3-off OFF; both source directories must resolve to this worktree. Use the
existing Conan toolchain/environment without package installation or tool changes.
Build and run the migrated family ON; build and isolate-launch goldendict OFF.
Existing historical app-test targets that ignore BUILD_TESTS are explicitly
inventoried, not silently swept into this family or newly exempted.

Add a focused target-closure check preventing this family's test sources/access
implementation/runner target from entering goldendict's production dependency
closure. Exercise both accepted and deliberately violating CMake fixture targets
to prove it rejects actual target membership/dependency violations. Inspect real
generated compilation/link metadata as additional production-isolation evidence.

W2 follow-up is validation only: cite its preserved decision/dispatcher review and
add one Network test of split publication thread identity, observer completion,
no GUI event-loop reentry and deferred post-work. If it exposes a contract change,
record a separate W2 issue; do not repair transaction behavior here.

Completion requires the complete current inventory, explicit old/new assertion
mapping, actual ON/OFF builds, nonempty passing tests, W1/W2 regression, negative
architecture-check evidence and fresh read-only exact-candidate review. Remaining
historical families stay pending and A4 is not closed.
