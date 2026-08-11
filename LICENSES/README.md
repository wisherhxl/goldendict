# License Boundary

GoldenDict is a combined distribution with an explicit component boundary:

- The GoldenDict application and product behavior, including
  `apps/goldendict/`, are licensed under GPL-3.0-or-later. The complete license
  text is in the repository root `LICENSE` file.
- Reusable Tiger build and platform infrastructure retains its MIT license.
  The `cmake/` submodule carries its own `cmake/LICENSE`; the MIT text is also
  reproduced in `LICENSES/MIT.txt` for installed and packaged notices.
- Files adapted from the Tiger template that retain an MIT SPDX header remain
  MIT-licensed. Product-facing files without a narrower component notice are
  distributed under GPL-3.0-or-later.

The resulting GoldenDict application is distributed under the GPL terms. The
MIT-licensed Tiger components remain separately identifiable and reusable
under their permissive terms.

## Dependency Inventory

Phase 2 directly declares these external components:

- Qt 6.11.1, including Qt WebEngine, resolved by Conan. The Conan recipe
  declares `LGPL-3.0-only`; the packaged Qt sources and binaries carry their
  complete module and bundled third-party notices.
- `python-html5lib/1.1`, used only in Qt's build context. Its local Conan
  recipe packages the html5lib, six, and webencodings license files and
  declares their MIT and BSD-3-Clause licenses.
- Conan `*/system` wrappers represent Linux display, graphics, and keyboard
  ABI requirements. They do not copy those system libraries into this source
  tree.

Update this inventory whenever product code or a direct dependency enters the
migration tree. Release packaging must preserve both the root GPL license and
the component notices in this directory.
