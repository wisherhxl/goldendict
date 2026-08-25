# Legacy Application Catalog Sources

This directory contains the complete 45-file Qt Linguist source catalog set
from the pinned GoldenDict migration baseline
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`, under the upstream GoldenDict
GPL-3.0-or-later license. The files originated at `locale/*.ts` in
`https://github.com/wisherhxl/goldendict.git`.

The 44 sources listed as disabled in `catalog_sources.cmake` are byte-for-byte
imports. They are source inventory only: the build does not compile, stage, or
install them, and their presence does not claim Qt 6 translation coverage or
runtime language support.

`ru_RU.ts` remains the already-enabled catalog accepted in the preceding
Phase 9 leaf. It derives from the pinned legacy file and differs only by two
trailing-whitespace cleanups plus the migration-specific
`GoldenDictInterfaceTranslations` and `PreferencesDialog` contexts. Its
runtime handling is unchanged. Each additional locale requires a separate
review of current Qt 6 contexts, focused smoke coverage, and explicit movement
from the disabled to enabled inventory.
