# View Presentation Persistence Evidence

Scope: first functional unit in design.md, under CRD-SHELL-001/002/005.
The overall main-window parity request remains open. This record is implementation
evidence, not an independent completion verdict or publication authorization.

External evidence root: `D:/workspace/goldendict/evidence/main-window-parity-20260911`.
Base: `a3b0e06559310f0b23e894585acd139278520a06`.
Qt5 reference: `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.

| Contract | Evidence |
| --- | --- |
| Three View choices persist and reconstruct without recursive saves | view-red.log, view-green.log; goldendict_view_menu_smoke |
| Rejected saves restore the checked and visible state | goldendict_view_menu_smoke injected callback failure |
| Defaults, equality, current serialization, unknown preservation, legacy root keys, malformed/duplicate rejection | core-red-focused.txt, core-green-current.txt; ViewPresentationPreferencesMigrateAndRejectInvalidValues and ApplicationPreferencesCompareByValue |
| Actual callback saves survive process restart in both directions | goldendict_view_preferences_restart_smoke, regression-short-root.log |
| Shell/menu/tab/coordinator regression | regression-short-root.log: 12/12 passed |
| Release build including affected DTO consumers | build-final.log, build-candidate.log |

Commands run from this task through `run_with_conan.ps1 --build-type Release --`:

```text
ctest --preset conan-release -j 1 -R '^goldendict_(product_shell|view_menu|view_preferences_restart|file_menu|edit_menu|search_menu|history_menu|favorites_menu|help_menu|article_tabs|widgets_facade_preparation|preferences_coordinator_predecision)_smoke$' --output-on-failure
build/Release/bin/application_service_test.exe ViewPresentationPreferencesMigrateAndRejectInvalidValues ApplicationPreferencesCompareByValue ConfigurationRoundTripsPreferencesDeterministically MissingLegacyPreferencesRetainCurrentDefaults RejectsMalformedLegacyPreferencesAtomically
```

Native captures in `qt5-view-matched-125` and `qt6-view-matched` cover enabled and
restored-default write/read states. Both use 800x600 logical windows, DPR 1.25,
Microsoft YaHei UI 9 and requested Fusion style. Qt6's application stylesheet wraps
its style; the original empty style-name metadata is not proof of the base style.
Qt5's capture-only entry point and build provenance are retained externally; the
frozen checkout was not modified. These captures corroborate menubar and icon-mode
state after restart, not exact pixel parity: Qt5 has its default Wikipedia button,
Qt6 has no dictionaries, and pane widths/gutters and tab metrics still differ.
Dictionary names are asserted as toolbar presentation state; populated-button
visual acceptance remains in the overall matrix. User screenshots have unknown
profile/style provenance and do not authorize overwriting custom layout.

Windows 11, MSVC 194 Release, Qt6 6.11.1 and Qt5 5.15.19 were used. Linux/macOS
and whole-migration acceptance are not verified. Textual DTO coverage includes
all new field references, equality, current/legacy parsing and UI reconstruction;
no complete semantic-index claim is made. The installed DTO extension requires
consumer rebuilds, which the task Release build performed.

Preserved failed probes: regression-final.log and restart-trace.log fail during
preference recomposition with a deeply nested test profile; a shorter external
profile and the revised build-root profile pass identical operations. Graphics
context errors and exit 0xc0000409 precede the test assertion. Path sensitivity is
established; the underlying runtime cause remains open and is not declared fixed.
Offscreen Windows probes also failed. These failures are retained, not substituted
for passing evidence. Native Windows is the verified restart environment.
