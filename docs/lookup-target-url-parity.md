# Lookup Target URL Parity

## Scope and design

N2 is a minor conformance correction under CRD-LOOKUP-003, CRD-DICT-003 and
CRD-COMPAT-001 in the [product baseline](qt6-product-baseline-crd.md).
The approved bounded impact check found no new requirement, public interface,
schema, navigation kind, dependency or architecture decision. EL-01 remains
the narrow [normalized-empty exception](empty-lookup-target-decision.md).

The later approved [N4 dot-only decision](dot-only-reference-decision.md) omits
href at the private Dictd producer boundary for successful normalized targets
exactly `.` or `..`. It changes neither N2 decoding nor its completed acceptance,
and does not authorize suppressing other nonempty dot-containing targets.

Frozen Qt 5 `dictdfiles.cc:349-394` normalizes reference whitespace before
percent encoding; actual WebKit evidence at reference commit
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` retains U+0001 and DEL in
nonempty targets. The external workspace evidence
`evidence/r3.7g-design-probe/{probe.cpp,observations.json}` records 24 cases.
The existing Qt 6 generic URL decoder rejects those residual controls.

The private article URL component will select lookup-specific decoding for
bytes 01-08, 0E-1F and 7F only. Lookup construction and parsing must preserve
valid UTF-8 and exact canonical percent encoding, including literal percent,
hash and slash. Empty, NUL, TAB, LF, VT, FF, CR, malformed escapes and ambiguous
URI structures remain rejected. Resource decoding and traversal policy remain
unchanged. The private article sanitizer will preserve a canonical href only
when `ParseInternalUrl` resolves it as `kLookup`; it must not decode/re-encode
that href or apply legacy hash stripping to it. Existing legacy and external
link branches keep their established semantics.

This follows the existing private validation/composition pattern and the
[Core/GUI boundary](project-design-rules.md#shared-library-and-gui-boundary).
No new abstraction is needed. The GUI already passes encoded links through
the facade and internal-link navigation without input-field trimming. Data
files, generated index formats and configuration schemas need no migration.

## Acceptance

Verification covers every admitted byte, printable/Unicode/percent/hash/slash
targets, rejected inputs, unchanged resource rejection and sanitizer retention.
Generated backend lookup plus facade, tab and saved configuration round trips
must retain U+0001 and DEL exactly. Actual Qt 6 QUrl/ArticlePage and WebEngine
clicks must dispatch those exact targets; an empty link must emit no lookup.
Run the owning Conan Release build and focused tests, full serial CTest,
Python script tests, documentation links and whitespace checks with fresh
short temporary paths. Record native Windows evidence separately from pending
Linux/macOS acceptance.

N2 is technically resolved, with Windows implementation acceptance verified on
2026-09-09 using Qt 6.11.1/MSVC Release. `article_assembler_test` covers all 27
admitted control bytes plus printable and Unicode targets. The generated Dictd
`application_service_test` rows retain U+0001/DEL exactly through facade, tab,
configuration serialization/restoration and backend lookup, including the
existing three-symbol input guard. `article_page_lookup_routing_test` uses the
production scheme handler and article origin, actual pointer clicks, and a DOM
click marker to establish that the empty link was attempted. All three native
Windows rows preserve exact target bytes and emit no lookup for the empty link.
This boundary check does not assert every EL-01 application state consequence.

Release build, 140 serial CTest cases and 172 Python tests (two existing skips)
pass. Local Markdown targets and whitespace checks pass. Commands and logs are
retained outside the repository in the workspace's
`evidence/n2-lookup-target-20260909/`. Independent completion and integration
audits returned Pass, and N2 is complete and integrated at
`d9f4947f06c5ebe8da7b37dc69426b1581a4676c` (tree
`f41ba7033b0df06430b1657e490a441419bc1932`). The separate approved
[N3 decision](malformed-reference-decision.md) disables activation only for
failed complete generated reference rewrites; it does not broaden N2 decoding.
The coupled R3.7g Dictd inline renderer/full-text changes, full EL-01 state
acceptance, remaining Dictd parity, Linux/macOS verification and cutover remain
open. No Linux or macOS execution is claimed by the Windows evidence.
