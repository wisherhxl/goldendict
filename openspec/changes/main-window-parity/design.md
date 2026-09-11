## Context

The approved product baseline, Section 6 and `CRD-SHELL-001..005`, defines the visual and functional contract. The user's 2026-09-11 approval accepts the region-by-region implementation and validation plan. Qt5 evidence is commit `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`; Qt6 task base is `a3b0e06559310f0b23e894585acd139278520a06`.

## Goals / Non-Goals

Use the existing Qt Widgets presentation architecture to conform each approved main-window region and verify its interactions. Preserve Core/facade ownership of lookup, workflows and persistence, and preserve existing supported profile state. No voluntary redesign, forced startup layout reset, new service API, or unrelated refactor.

## Decisions

1. Diagnose fresh startup separately from profile restoration. `ApplyDefaultPaneLayout` already creates right-stacked result, favorite and history docks. A restored custom state is not automatically a bug. The reference screenshot is evidence of a state, not authority to overwrite all saved layouts.
2. Keep changes local to the existing presentation owner and resource paths. Use direct existing methods and Qt signals; no new architectural pattern is needed for local shell corrections.
3. Verify all approved regions; implement only demonstrated conformance gaps. Use existing shell/menu/tab tests for backed actions and dedicated checks for newly observed defects. Keep test logic in test utilities when practical.
4. Maintain a matched capture matrix including initial/empty, lookup, pane visibility/docking, and restart. Record source/binary identity, style, font, logical size, device pixel ratio and profile origin. Different dictionary contents or fonts must not be misclassified as layout implementation gaps.
5. Develop and build only in the task worktree. The old `fix/qt5-default-pane-layout-parity` task retains its old contract and history; it is evidence only, not transferred work.

## Risks / Trade-offs

- Existing user state could be overwritten by forced defaults: test isolated copies and preserve valid restored state.
- Stale executables could misrepresent source: build the exact task checkout with its own Conan environment and record identities.
- Platform-native metrics vary: use matched style/font/DPI captures and semantic geometry checks; retain the product baseline's narrow native tolerance.
- Large historical smoke tests can mutate their profile: run only with isolated profile roots and verify restart using the resulting intended state.
- Serena currently has no task compilation database: use explicit textual impact analysis for local presentation changes until configuration produces a matching database; do not claim full semantic coverage.

## Impact Check and Verification

Existing approved requirements define the result without an architecture or product decision. The approved scope may proceed through BUILD. Confirmed corrections use a failing focused test followed by a minimal fix. Run the owning checkout's Conan launcher for builds and CTest; validate native Windows captures and applicable portable checks. A fresh independent review must bind the final candidate and all required evidence. Unmet acceptance remains open.

## Migration and Rollback

Preserve user profiles and existing task branches. Remote publication, canonical advancement and archive/cleanup are separately authorized operations under `goldendict-candidate-v1`.

## First Functional Unit: View Presentation Persistence

Qt5 `mainwindow.cc:3426-3457` retains menubar visibility, dictionary-bar names and toolbar icon size. Qt6 only changes widgets. The first unit repairs this existing behavior through `ApplyDisplayPreferences`, the same application-owned persistence callback used by search placement and zoom. Add two boolean values, `show_dictionary_bar_names` and `use_small_toolbar_icons`, to the existing transport-neutral `ApplicationPreferences` DTO, its equality, and existing preference serialization. Both default false, matching Qt5. Existing `hide_menubar` is reused. This extends the existing data contract (consumers must rebuild), not service ownership or a new service interface. Native Qt widget types remain absent from Core.

Recognize Qt5's root `showingDictBarNames` and `usingSmallIconsInToolbars` elements through the legacy parser, retaining duplicate/malformed rejection and source-file preservation. They are not children of `preferences`. New current fields use the existing preference format and unknown-field preservation; no file-format version replacement is required. Presentation reconstruction blocks action signals. Each user action requests persistence once; failed save restores action/widget state without recursive requests. Acceptance covers all three toggles, serialized reload/restart, default false, legacy true/false, malformed/duplicate rejection, and rejected saves. This is a coherent independently reviewable first delivery; the remaining main-window findings stay open in the same record.
