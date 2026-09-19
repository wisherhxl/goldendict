# P1 WebEngine setter supplement — approved 2026-09-19

Authority: the user selected the existing-default-profile setter option after
webengine-path-capability-20260919/blocker-supplement.md. Base
 e35e21fdd54503eb7e0e09e01283af09bf45dcec. This adds an independent supplement
commit; earlier P1 code, evidence and Fail remain historical and unchanged.

The accepted observation boundary permits constructor-time default-path
calculation, checked off-record guards and bounded negative write observations;
it does not promise zero writes for all Chromium features. Any newly identified
reachable daily-data side effect blocks its affected run.

## Selected local implementation

- Add an app-private WebEngine storage-path unit, not a profile owner or framework.
  After ResolveConfigurationLocations and before any WebEngine view/page/window,
  main calls it once with those locations and the existing optional smoke root.
- No override for non-portable mode. Portable root is config parent/webengine;
  explicit test root wins. Selected article data is root/article. Empty explicit,
  relative or unusable roots fail clearly before constructing a page; no default
  fallback. Directory creation/write probe are restricted to the chosen subtree.
- Validate owned article/inspectors directories before obtaining defaultProfile,
  then immediately setPersistentStoragePath(article). Do not change HTTP/cookie/
  permission policies or take ownership. Store the selected root as private
  profile metadata for Inspector initialization. A matching private application
  property records one-time initialization without fetching a profile on invalid
  or repeated calls. Same-root initialization is
  idempotent; a conflicting attempt is rejected without resetting the profile.
- Inspector keeps its current independent QObject-owned profile. At its existing
  creation site, use the inspected profile's private root metadata to allocate
  a unique temporary child below root/inspectors and immediately set its data
  path before its page/view. RAII directory cleanup follows page then profile
  destruction. Unconfigured non-portable profiles retain existing behavior.
  Setup failure prevents Inspector opening and reports a diagnostic.
- Normal and staged candidate ArticlePage creation and W1 bindings stay unchanged;
  they continue using the exact Qt default singleton. No MainWindow/ArticlePage
  profile injection, Builder, transaction-publication setup, Network edit or
  portable detection inside Network. Linux early help uses QTextBrowser, not
  WebEngine; registration-only exits do not create a profile.

## Verification

Use existing W1/W2/W3 presentation target reuse. Add a dedicated test target and
small direct calls to this same product initialization in relevant existing test
mains. Before fix, obtain a safe red path assertion through a no-op initialization
seam using test identity/test path mode; record that seam distinctly. The test
loads real pages, verifies default identity/memory policies, common candidate
paths via W1's real preparation/rebuild, Inspector independence and cleanup,
invalid paths/no fallback and stable reapplication. Add the selected Qt setter
characterization to the normal targeted upgrade checks, referencing the retained
Builder experiment without shipping Builder behavior.

Run relevant Inspector, W1/W2/W3, configuration/recovery and Network regressions,
separate ON/OFF builds, actual link isolation, then a copied production binary in
an owned portable layout with explicit own index/dictionary paths. No smoke arg,
Network/WebEngine bypass or artificial exit hook. Observe actual main window and
normal external Quit; monitor owned roots with known notification blind spots.
No daily sentinel, cache cleanup, global folder remap or test identity in product.
Final fresh independent read-only audit covers cumulative P1 base
 d196d6d7c02f17e2ba91e58d34620d4f9d0c4803 through the new exact candidate and separately
the supplement diff from e35e21fd. No merge/push or W3.3.
