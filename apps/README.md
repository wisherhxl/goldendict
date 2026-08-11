# GoldenDict Applications

Applications live in `apps/<app_name>/` and use Tiger's application helpers.
The Phase 2 product target is `apps/goldendict/`, declared with
`ti_add_qt_app` and linked to the Qt components resolved by Conan.

Application source belongs in `src/`. Optional runtime resources belong in
`resources/`; Tiger copies and installs them through its existing app rules.
