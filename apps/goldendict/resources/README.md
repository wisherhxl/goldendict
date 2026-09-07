# GoldenDict Product Resources

The files in this directory are GoldenDict product resources imported from the
frozen Qt 5 product baseline
`3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.

They retain their original GoldenDict GPL-3.0-or-later provenance and resource
paths. Import resources in reviewed feature-owned batches; do not replace an
available product resource with a demonstration placeholder.

The toolbar lookup control uses the pinned `system-search.png` and
`1downarrow.png` assets for the leading search marker and suggestion-popup
toggle. Their resource names and bytes match the Qt 5 baseline.

The backed Preferences pages consume the pinned `interface.png` and
`network.png` tab icons directly at 15 logical pixels. `configure.png` remains
the dialog window icon; Advanced has no tab icon in the frozen UI. The two new
files are byte-identical imports from `icons/` at the frozen revision and are
embedded by `product_resources.qrc`, without a runtime legacy-checkout dependency.
