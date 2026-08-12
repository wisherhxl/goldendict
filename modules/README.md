# GoldenDict Modules

GoldenDict product logic initially lives in the public `goldendict_core`
shared library. Its Tiger short name is `core`, so the source lives under
`modules/core/` and is declared with `ti_define_module(core ...)`. The GUI
executable consumes its desktop facade and remains responsible only for
presentation and composition. A future dictionary-service executable consumes
the separate headless API from the same library, primarily to provide
structured dictionary retrieval to AI clients.

Phase 4 keeps these as internal source components of `goldendict_core`, not
separate DLLs:

- foundation: low-level value types, errors, files, text, and concurrency;
- dictionary: dictionary contracts, requests, discovery, and shared index
  lifecycle;
- formats: private local-format adapters, beginning with StarDict;
- article: browser-independent article and resource assembly;
- application: configuration, catalog, lookup, and use-case facade consumed
  by the GUI;

Only the headless dictionary API, desktop facade, transport-neutral DTOs, and
genuine extension contracts belong under `include/goldendict/core/`. Headless
results carry bounded content, match metadata, stable dictionary provenance,
and typed resources suitable for AI retrieval. Public APIs must not expose Qt
Widgets, Qt Gui, Qt WebEngine, GUI-thread assumptions, or a service transport.
Internal components and concrete formats stay under `src/` or private headers.
The internal network integration module is the first demonstrated optional
dependency boundary: it owns Qt Network based transport without making local
headless core consumers resolve Qt's runtime graph. A later audio, desktop,
transport, or plugin module likewise requires a demonstrated optional
deployment, dependency, platform, or ABI boundary. Source-level separation
alone does not justify another DLL.

Phase 2 retains `modules/tiger/` as the base module because it is reusable
Tiger infrastructure, not product-facing GoldenDict identity.
