# Preferences Backed-Page Layout Conformance

Change class: Minor correction under the approved product baseline CRD.
Requirements: `CRD-PREF-001` through `CRD-PREF-004`, `CRD-SHELL-005`,
and the visual structure rules in Section 6 of
[the product CRD](qt6-product-baseline-crd.md).
Base: `cd48f6b4fc1584b0dad8327cd2cffa126c2ccda7`.

## Scope and dependency refinement

The current General page mixes Interface and Advanced controls in one tall
column. This is not the frozen product structure and can force the dialog
beyond usable screen height. The existing complete-candidate apply transaction
already backs every control being moved.

This delivery is the shared page-layout prerequisite within R4.1, not closure
of R4.1 or R4.2. It restores the existing controls to Interface, Network and
Advanced pages in their relative legacy order. It does not add empty Scan Popup,
Hotkeys, Audio or Full-text Search pages, or inert controls. Those pages enter
with their working runtime features. R4.1's full closure additionally requires
tray/autostart from R7.4 and the style/language/help asset/runtime prerequisites
of R9; R2.3 alone is sufficient only for this backed layout correction.

Remaining R4.1 units are the backed tray/startup group and appearance/language
selectors followed by complete page acceptance. R4.2 still owns unsupported
Advanced controls, exact remaining bounds/defaults and platform-specific groups.
No missing legacy capability is excluded or declared complete by this split.

## Frozen evidence and expected result

Frozen `preferences.ui` and `preferences.cc` at
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8` define:

- Interface first, with the two-by-two Tabbed browsing group, article-click
  and ESC controls, dictionary context limit, and existing language controls.
- Network before Advanced, with its existing controls retained and the proxy
  group above the cache row as in the frozen page.
- Advanced groups History and Favorites side by side; Articles contains the
  collapse and input-limit rows, optional-parts and diacritics controls;
  synonym search follows the Articles group.
- Exact relevant page labels/mnemonics, tab group wording/tooltips, existing
  configure icon and imported Interface/Network tab icons at 15 logical pixels.
- Existing values, enablement rules, cancel/failure behavior, atomic complete
  candidate application and persistence remain unchanged.

Absent runtime-owned controls remain explicitly pending in the gap inventory;
native font/style differences are not permission to rearrange the backed
controls. Retain the existing Linux-only language/help runtime gate in this
layout unit; cross-platform enablement remains separately required.

## Readiness and architecture impact

Result: Ready. This is presentation-only conformance, not a new requirement or
architecture decision. `PreferencesDialog` owns page/layout construction and
intent collection; Core retains persistence and runtime behavior. Existing
composition-root callbacks remain unchanged. No new abstraction, public API,
dependency or design pattern is needed. Assets are copied byte-for-byte from
the frozen product and embedded through the existing QRC system.

Windows Qt 5 and Qt 6 runtimes and deterministic empty profiles are available.
Linux execution remains a later platform acceptance obligation; the shared
widget hierarchy is checked here without claiming a Linux visual Pass.

## Verification plan

- Add a focused `preferences_dialog_test` for page order, parent ownership,
  grid positions, labels/icons, usable geometry, and successful/cancelled/failed
  complete-candidate apply across pages. Keep existing runtime-effect smokes.
- Run through `run_with_conan.ps1 --build-type Release`:
  `ctest --preset conan-release -R 'preferences|edit_menu' --output-on-failure -j1`.
- Build Release and run the cumulative CTest gate plus script tests; verify
  installation and resource provenance hashes.
- Capture matching Qt 5/Qt 6 backed page states with the same host, scale,
  style and font, retaining missing-control differences without masks.
- Stage the cohesive delivery, obtain an independent fresh read-only audit,
  commit and push only after Pass, then use the separate integration gate.

## Verification and remaining visual differences

The Windows Release build and focused 15-test Preferences/Edit suite pass.
The script suite passes 171 tests with two platform-condition skips. A parallel
cumulative run reported one `hunspell_provider_test` process-launch
`BAD_COMMAND` without a test assertion. Three consecutive isolated repeats
pass; the final serial cumulative run passes all 136 tests in 40.80 seconds,
without an overlapping build. Installation to `build/Release/install` also
passes. The launch failure is retained as a host observation, not silently
waived or described as a proven root cause.

Paired evidence is retained at workspace-relative
`evidence/preferences-layout-final/{qt5,qt6}/{0,1,2}.png` with per-version
`metadata.json`. Both use Windows QPA, Fusion, Segoe UI 9 pt, DPR 1 and a
670 by 475 logical-pixel client area, empty isolated profiles and default
preferences. Qt 5 is 5.15.19 and Qt 6 is 6.11.1. Qt 5's seven-page minimum
expands its nominal 636 by 431 form; the partial Qt 6 form naturally remains
636 by 431 before capture resizing. Final geometry parity remains a complete
page acceptance obligation. The preliminary offscreen captures with missing
Qt 5 text are invalid and are not acceptance evidence.

The external reproducible capture adapter in
`evidence/preferences-layout-tools/prepare_qt5_capture.py` links the frozen
product's unchanged Preferences, generated UI, configuration and resource
objects with a capture-only replacement `main.o`; it does not edit the frozen
checkout or apply preferences. `run_capture.py` isolates profiles and runtime
paths, and the Qt 6 invocation runs through `run_with_conan.ps1`. Qt 6 captures
the actual production `PreferencesDialog` linked into the focused test.
`compare_capture.py` produces unmasked pixel counts and both image hashes.
The `comparison.json` SHA-256 is
`442472463f1d6a7673c79bc2093ab1809ebc590cdcdb191beda53b44ebf92721`.

| Page | Verified correction | Remaining required differences | Unmasked changed pixels |
| --- | --- | --- | --- |
| Interface | Original icon, tab group two-column order, click/selection/ESC and context-limit placement | Tray/startup, appearance/language/help, four other pages; absent groups change absolute vertical placement | 113441 / 318250 |
| Network | Original icon and proxy-before-cache order; retained controls and values | System/SOCKS/authentication proxy modes, proxy control types/defaults, remote-content/header/update controls and Windows Help | 154501 / 318250 |
| Advanced | History/Favorites side by side, Articles two-row grid, exact diacritics label and synonyms below | History/Favorites save intervals, Windows scan technology controls and remaining bounds/platform groups | 155911 / 318250 |

These are partial-conformance screenshots, not pixel-equivalence or full-page
Pass claims. No masks or exclusions hide missing controls. R4/R7/R9 retain the
remaining requirements, including historical fields that older Phase 8 notes
called excluded. The complete candidate transaction and runtime effects remain
covered by the existing smokes; no dictionary bytes, lookup or index behavior
change in this presentation-only unit. Real-corpus backend acceptance is not
replaced by synthetic UI evidence.
