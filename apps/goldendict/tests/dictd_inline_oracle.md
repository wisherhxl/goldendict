# Dictd inline display oracle

`dictd_inline_oracle.json` retains the original 48 generated input bodies and
actual Qt 5.15.19 WebKit DOM/anchor observations from workspace evidence
`r3.7g-design-probe` and `r3.7g-design-probe-extra`. Each source contains 24
cases; `sourceIndex` is zero-based. All bodies use the default LTR base.
The probes compile frozen `htmlescape.cc` and reproduce `dictdfiles.cc`'s
sequential transformations at `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`.
These project-generated fixtures contain no third-party dictionary data and
follow this repository's GPL-3.0-or-later license.

Anchor origin attribution was reviewed against those sequential replacements,
not inferred from browser URLs. Global cases 3, 6, 10, 14, 19 and 24-47 fail
the complete opening-tag match because the captured generated span class or
div direction contains a quote. Every clone retains origin 0. Case15 contains
two independent successfully rewritten references (origins0 and1). All other
references match the complete opening tag; cases12 and23 normalize to empty
under EL-01. `completeOpeningRewrite`, `origin` and `normalizedEmpty` record
these separate facts explicitly; `raw` remains unmodified browser evidence.

The original DOM observations are unstyled input evidence. Actual styled
acceptance additionally compares product CSS, computed/pseudo-element styles,
layout and paired screenshots in workspace `evidence/r3.7g2-qt5-styled-20260909`
and `evidence/r3.7g2-20260909`. N3 and EL-01 href differences are intentional;
ordinary reference targets remain exact. Full-text has its independent oracle
in `modules/core/tests/support/dictd_fts_oracle.inc`.

`dictd_control_context_oracle.json` separately retains seven exact actual Qt5
body/DOM/text/anchor rows from immutable workspace evidence
`r3.7g2-qt5-control-contexts-20260909/observations.json` (SHA256
`193fed1014c8cf1663abee2f1f3cb6748a9c18469b2ff8fb47d004838ff30cc4`).
The 69 control matrix inputs are generated explicitly in the private test;
their source observations are `r3.7g2-qt5-control-glyphs-20260909` (SHA256
`d9ac369a3f1c31e1797c548f35fe94c9e60eab9f7c8696386787cc0c9a164e25`).
Only the exact `span.dictd_control` wrapper is projected away for DOM equality;
original control bytes, elements and reference boundaries remain unchanged.
