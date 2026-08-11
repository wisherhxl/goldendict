# GoldenDict Modules

Reusable code lives in `modules/<module_name>/` and is declared with Tiger's
`ti_define_module` helper. Applications should consume modules instead of
placing reusable infrastructure under `apps/`.

Phase 2 retains `modules/tiger/` as the base module because it is reusable
Tiger infrastructure, not product-facing GoldenDict identity.
