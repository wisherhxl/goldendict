## Why

The user supplied paired desktop screenshots showing materially different main-window regions and approved correcting Qt6 to the frozen Qt5 shell on 2026-09-11. Current source already contains default right-hand docks, so persisted state and runtime provenance must be distinguished from missing implementation.

## What Changes

- Verify and correct main-window regions and their existing functional interactions under `CRD-SHELL-001` through `CRD-SHELL-005` in `docs/qt6-product-baseline-crd.md`.
- Cover menus/toolbars, query and group controls, article tabs/Welcome/navigation, dictionary results/Favorites/History, status presentation, and layout persistence.
- Compare fresh and restored profiles under matched window size, DPI, locale, style and dictionary state; preserve legitimate user layouts.
- Add focused regression evidence for confirmed defects and record each surface's actual acceptance status.

## Capabilities

### New Capabilities

None. Existing approved requirement conformance; metadata uses `skip_specs: true`.

### Modified Capabilities

None. This is regular parity work under the minor-correction route, not a requirement change. Existing lookup/tab and compatibility requirements remain governing dependencies. No intentional divergence is proposed.

## Impact

Presentation and tests in `apps/goldendict`, existing resource consumers, and this OpenSpec record. The first unit extends the existing installed `ApplicationPreferences` DTO and Core serialization with two boolean View settings, requiring consumers to rebuild; it creates no new service interface, library, dependency, or backend ownership. Whole migration/release acceptance, dialog interiors, and unrelated platform integrations are not claimed by main-window acceptance. Any newly discovered required broader dependency stays explicit rather than being represented as complete.
