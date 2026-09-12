# Draft: Constrained Qt6 Architecture Remediation Review

Status: **Draft — planning only; not approved, not Ready, no finding closed.**
Date: 2026-09-12.
This is one planning attachment in the existing OpenSpec change hierarchy, not
a new normative architecture system or a completed OpenSpec implementation change.
No proposal/spec delta or apply-ready metadata is manufactured for unapproved
interface decisions. Promote the selected unit through native OpenSpec artifacts
only when its design is selected. Do not apply this entire queue automatically.

## 1. Baseline, authority, and loaded sources

- Initial shell directory: `D:/workspace/goldendict/master`; no edits there.
- Inspected Git root and sole permitted worktree:
  `D:/workspace/goldendict/worktrees/feature-tiger-qt6-migration`.
- Branch: `feature/tiger-qt6-migration`.
- HEAD and audit baseline: `3f3f2bf4c54eaf2c5d77c1159947212b1b43169c`.
- Initial tracked/untracked status was clean. Fetch succeeded; HEAD versus
  `origin/feature/tiger-qt6-migration` was `0 0`. The audit-to-HEAD diff is empty.
- Audit read in full: `D:/workspace/goldendict/evidence/architecture-audit-20260912.md`.
- Frozen Qt5 behavioral reference remains
  `3d93dd66197aea10edf6c29998ddc9c213d0aaa8`; no new Qt5 runtime evidence collected.
- Scope authorization: read-only review, safe checks, and this Draft only. No
  production/test/build/rule edits, staging, commit, push, reset, cleanup, tool
  upgrade, initialization, configuration, or worktree switch is authorized here.

### Loaded rules and documents

Fully read applicable entry points: `C:/Users/dev/.codex/AGENTS.md`,
`D:/workspace/goldendict/AGENTS.md`, and target-root `AGENTS.md`.
Read all three global policies under `C:/Users/dev/.codex/policies/`:
`engineering-delivery.md`, `software-design.md`, `issue-resolution.md`.
Fully read `docs/agent-workflow.md`, `docs/project-design-rules.md`,
`openspec/config.yaml`, the active `main-window-parity` proposal/design/tasks/
verification, `modules/README.md`, `apps/README.md`,
`docs/configuration-file-publication.md`, and
`docs/observer-platform-test-isolation.md`.

Focused canonical sections actually inspected (not a claim to load every
historical leaf of these large documents):

- `docs/architecture.md`: purpose/module boundary, Inspector ownership,
  P8-FT-87/88, prepared Core activation, hidden Widgets preparation, stable
  slots/bindings, visible commit, reservations, coordinator, production
  Preferences/source/group invocation and startup recovery (6518–6919).
- `docs/testing.md`: Core activation, coordinator and hidden Widgets gates,
  reservation gates, Conan execution, QTest and isolation (6740–7079).
- `docs/migration.md`: governing product baseline, provenance, rules and early
  migration phases; `docs/feature-parity.md`: governing status and Core matrix;
  `docs/porting-map.md`: ownership, dependency graph and fixture policy.
- `docs/qt6-product-baseline-crd.md`: default-to-Qt5, goals/non-goals, shell,
  lookup, state, compatibility and real-corpus isolation requirements.
  Primary traceability: CRD-SHELL-003, CRD-LOOKUP-001..004,
  CRD-STATE-004, CRD-COMPAT-001..005, CRD-TEST-REAL-002/003.
- `docs/qt6-baseline-gap-audit.md`: governing status and platform assignment;
  `docs/build.md`: prerequisites, hermetic launcher, Windows/Linux Release,
  install distinction; `docs/coding-style.md`: language/generated/module rules.
- Source-private Core, Network and Widgets contracts and relevant CMake sources;
  `apps/goldendict/tests/widgets_facade_preparation_smoke.cmake`.

Ancestor and repository searches found no applicable `AGENTS.override.md` or
nested module AGENTS. `docs/policy-candidates/user/AGENTS.md` is candidate content,
not an instruction for source modules. No standalone ADR file or explicit
Accepted ADR record was found by filename/content search in docs/OpenSpec.
Accepted design evidence is embedded in focused documents and architecture
leaves; lack of an ADR directory does not remove their authority.

The activation selector is enabled for `goldendict-candidate-v1`; all four
listed installed SHA-256 hashes matched. For this new task its operational
contract is `docs/agent-workflow.md`. Planning permission grants no publication
authority. No independent completion audit or development-readiness Pass is
claimed by this review.

### Conflicts and disposition

1. Workspace normally requires isolated development outside the canonical
   checkout; this request explicitly selects that checkout and allows a Draft
   there. Only this Draft is written. No generic worktree creation is performed.
2. Generic Superpowers brainstorming/planning defaults suggest separate
   `docs/superpowers` records and eventual commits. Project OpenSpec and explicit
   user scope override those defaults. Methods used: context discovery,
   systematic source tracing, bounded decomposition and verification of claims.
   The generated `.agents/skills/openspec-propose/SKILL.md` planning boundary
   was read; this user-requested single Draft is intentionally not a full
   apply-ready scaffold. Native context/list were used without init/update.
3. The audit numbering differs: this document's A1/A2/A3/A4/A5/A6 map to original
   audit A1/A2/A4/A3/A6/A5 respectively. Original A7 remains explicit debt.
4. High-level rules forbid GUI lookup orchestration and concrete backend
   dependencies in common application policy, while historical leaves explicitly
   place executor ownership in ServiceState and transaction composition in the
   app. Preserve those lifetime invariants; formally clarify A3 ownership, and
   do not label every app-private include an accidental violation.
5. Historical test counts and completed leaves are evidence for their own
   candidates, not current validation. Existing main-window parity remains open.

## 2. Evidence-based finding revalidation

All six concerns still exist at the identical baseline. Structural facts below
are source-confirmed; no new product execution, allocator experiment, race test,
or red/green regression was performed in this review.

### A1 — Page initialization and lifecycle wiring diverge (P1)

Evidence: `apps/goldendict/src/main_window.cpp:8581` CreateArticleView creates
ArticleView/ArticlePage, initializes facade/preferences and connects events.
The normal loadStarted path calls HandleArticleReloadStarted at 8607.
PrepareFacadeCandidate reconstructs the same pair at 12457 and connects
loadStarted to WidgetsFacadeActivationRelay::ArticleLoadStarted at 12476.
That relay method (384–394) invalidates output/matches and advances navigation
generation but omits HandleArticleReloadStarted. Its loadFinished route (486)
does call the common HandleArticleLoadFinished.

Call/state chain: ReloadCurrentArticle (8701) -> StartPendingArticleReload
(8714) sets in_flight_generation and load_started=false -> view->reload ->
loadStarted -> HandleArticleLoadFinished (8737). Completion retires the request
only when in_flight_generation, load_started and !page->isLoading all hold.
Without retirement, a later refresh cannot dispatch at 8710/8719.

Confirmed: inconsistent wiring and predicates. Inferred conditional behavior:
after successful replacement, a real refresh can leave the generation in flight
and suppress a later pending refresh. Not established: universal refresh failure,
data loss, or a measured interactive reproduction.

Rules: CRD-SHELL-003, CRD-LOOKUP-003/004; hidden preparation/relay completeness
and generation contracts in architecture 6645–6766; software-design SRP.
Gap: no concise shared initialization/event-policy rule or route-crossed reload
acceptance matrix. History establishes a missed parallel update: relay existed
in c9cce39de; commit 638161b930582a6064944d61fd3fc41fe693ab93 added the started
handler to normal creation and serialized reload tests without changing that
relay. This supports duplicated-update drift, not speculation about motivation.

Minimum correction: one app-private initialization/binding recipe with explicit
active/candidate inputs and one owner handler for each shared lifecycle event.
Keep relay gating, candidate ownership and preparation/publication timing.
Use explicit facade/preferences/parent inputs rather than reading active state
when constructing a candidate. Do not mechanically equate intentionally different
staging and active operations. Verify real repeated reloads across both paths,
including each production replacement caller and abandoned/stale candidates.

### A2 — MainWindow owns lookup use-case lifecycle (P2)

Evidence: StartLookupInTab (14544) builds navigation and calls OpenArticleTab;
StartNavigationLookup (14580) cancels prior requests, builds LookupQuery,
applies navigation dictionary scope, calls DictionaryService::StartLookup and
starts polling; FinishLookup (14638) owns requests_, IsFinished/Await, and
ComposeLookupPage, mixed with DOM publication, scroll and presentation work.
DesktopFacade's installed surface (desktop_facade.h:206–246) exposes these
fragments. DesktopFacadeImpl (desktop_facade.cc:68) already owns service and
tab records, providing the existing owner to extend.

Confirmed: request/tab/query/composition orchestration lives in the window.
Inferred risk: another presenter would duplicate lifecycle/cancellation policy;
no second presenter failure, hang, or cancellation-time bound was measured.
Rules: explicit Shared-Library And GUI Boundary, porting ownership, and
CRD-LOOKUP-001..004. This is an existing rule not fully implemented. Historical
main.cpp history/Favorites composition is documented and is not swept into this
ticket; no unsupported chronology or developer attribution is asserted.

Minimum correction: move a complete desktop lookup operation into the existing
Core desktop implementation, including request ownership, supersession/cancel,
completion/error handling and article composition. UI may schedule neutral
completion polling but cannot own backend handles or Await policy. Keep page
identity, render generation, scroll, DOM and actual Qt event dispatch in Widgets.
Characterize current navigation/history/group behavior before migration; test
headlessly and through the real desktop path afterward. Public facade changes
require selected design, readiness and consumer rebuild evidence.

### A3 — ServiceState mixes concrete composition with common policy (P2)

Evidence: dictionary_service.cc:1015 constructor discovers Hunspell at 1026,
opens sound/StarDict formats at 1062/1070, computes generated-index paths,
creates AARD at 1311, registers ports/holders at 1314, retains concrete AARD
pointers at 1294 and queries StartupArtifactEvidence at 1474. It resolves groups
at 1450, applies lifecycle policy at 1463 and creates the executor at 1479.
The same class implements SearchFullText (1494), lookup/suggestions and resource
policy (2093). Its executor declaration at 2194 is ordered after its dependencies.

Confirmed: concrete format creation/startup details and general query policy
share an owner. Not established: a broken format, unsafe destruction, duplicate
executor, or OCP violation merely because a finite creation list exists.
Rules: private built-in composition and abstraction dependencies in design rules;
porting graph separates composition from application. Architecture P8-FT-87/88
explicitly requires ServiceState executor ownership and old-stop/new-start order.
Disposition: common-policy isolation is incompletely implemented; the precise
composition product and lifecycle registration seam need an approved design
clarification. Do not rewrite the historical accepted ownership record.

Minimum correction: a private composition function/product in existing Core
returns owned abstract sources, diagnostics and concrete-independent lifecycle
registration/evidence inputs. ServiceState retains authoritative service state,
groups, coordinator and executor unless separately approved. The composition
product must preserve port/holder backing lifetime; no raw concrete AARD access
remains in general query methods. No public registry or format-per-DLL framework.
Verify IDs/order/diagnostics, groups, empty/mixed sources, reconciliation, idle
candidate behavior and shutdown-before-port-destruction.

### A4 — Test scenarios and fault orchestration reside in production (P2)

Evidence: main_window.h:252–335 exposes Run*Check scenario methods and 574–575
stores injection steps. main_window.cpp:2309 owns shell assertions; 3930 owns
the Preferences failure scenario, mutates the dialog executor and loops injection
cases; RunArticleSearchReloadCheck near 5980 owns event counters and synthetic
completion. main.cpp:24 includes ../tests/view_preferences_smoke.h; smoke flags
and dispatch live in the normal entry point. App CMake declares the production
target and registers smoke commands against it. Scenario edits therefore change
the production class and executable. Real-product coverage remains valuable.

Confirmed: build/source responsibility mixing. Not established: a normal launch
executes a fault scenario accidentally. Rules: global SRP, app presentation/
composition boundary, QTest and isolated runtime requirements. Existing workflow
supports real-caller smokes but lacks a strict target ownership rule; a narrow
test access seam and test-target policy need explicit acceptance.

Minimum correction: move scenario state, doubles, assertions, injected schedules
and smoke-only dispatch to test targets under apps/goldendict/tests. Reuse the
same compiled production implementation/private composition entry; do not copy
it into a testing implementation. Narrow test access may expose observations
or injected collaborators, not scenario methods. Preserve production startup/
transaction ordering and packaged executable smoke coverage.

### A5 — Fallible allocation remains after durable decision (P2)

Evidence chain: coordinator.cpp (full filename
configuration_reload_transaction_coordinator.cpp) PersistDesiredConfiguration
at 175 -> BeginDesiredRuntimePublication at 196 -> Network publication at 229
-> Core publication at 240 -> Widgets publication at 250.
NetworkRuntimeTransaction::Publish, network_runtime.cc:971, is noexcept;
PublishOnly at 983 precedes make_unique<Published::Impl> at 985.
DesktopFacadeActivationOwner::PublishReservedOnly, activation_owner.cc:227
(full filename desktop_facade_activation_owner.cc), is noexcept and allocates
PublishedCoreFacadeCandidate::Impl at 235 before publishing its own facade.

Confirmed: both allocations are after the coordinator decision; Network has
already published when its allocation runs. If either allocation throws bad_alloc,
noexcept terminates before the coordinator can record its structured injected
failure. No actual allocation-failure crash or data loss was reproduced.
The durable applying record permits restart recovery; failure recording/finalization
are themselves forward fallible work and are not promised infallible under OOM.

Rules: architecture 6767/6789, especially 6798, and reservation/forward recovery
tests. History: 84905691691c435541a875e43daa970c30f78b78 introduced both allocations
and the same commit's architecture text requires fallible preparation before
reservation. This is implementation nonconformance, not absence of a rule.

Minimum correction: prepare publication-token storage during existing fallible
preparation, before reservation/decision; publication consumes reserved storage
with established nothrow operations. Do not just remove noexcept, catch and claim
rollback after publication, reorder owners, or alter persistent schema. Inspect
the bounded transitive publish operations including observer callbacks; preserve
intentional invariant fail-stop behavior. Verify failure at each moved allocation
before decision and absence of those allocations during reserved publication.

### A6 — Resource outcome distinctions are erased (P2)

Evidence: runtime_dictionary_source.h:18 defines unavailable/invalid/cancelled/
deadline/unsupported errors and :105 returns optional resource with bytes/MIME.
dictionary_backend.h:11–20 aliases that exact contract (no separate error
translation is necessary). DictionaryServiceImpl::GetResource at
dictionary_service.cc:2304 forwards to ServiceState::GetResource (2093): unknown
dictionary -> {}, absent resource -> {}, successful zero bytes -> {}, caught
dictionary::Error -> {}. Other exception types are not caught by this block.
Installed DictionaryService::GetResource at dictionary_service.h:279 returns
only vector<byte>. ArticleSchemeHandler::requestStarted (45–48) maps empty to
UrlNotFound; AudioPlaybackService::Play (45–49) maps it to kEmptyResource.
Installed consumer test_package/headless_api_test.cpp also uses the byte API.

Confirmed: information loss, including cancellation reported by a backend.
Not established: every backend checks cancellation correctly or a reproduced
browser/audio failure. This is not proof of an LSP violation under the current
byte-only declared interface. Rules require explicit errors, ownership and
structured neutral results; the resource-specific public outcome, error precedence
and compatibility policy are missing and require approved delta design.

Minimum correction: structured neutral resource outcome all the way to consumers;
only a named compatibility boundary may collapse it. Preserve backend codes and
dictionary/resource identity. Add local-source and external-source outcome
fixtures plus installed headless, real scheme-handler and audio consumer tests.

## 3. Minimum target responsibilities and dependency boundaries

| Boundary | State and lifecycle owner | Allowed dependencies | Forbidden dependencies | Independent proof |
| --- | --- | --- | --- | --- |
| A1 page lifecycle | MainWindow owns GUI tab/view, navigation/reload/render state; QObject parent and candidate registry own object lifetime; relay gates delivery | Existing ArticleView/Page, explicit facade/preferences, binding registry, common owner handlers | Hidden candidate mutating active maps; new connection/allocation/load during irreversible publication; duplicate event policy | Same lifecycle matrix for normal and reconstructed pages, hidden silence, stale rejection, real reload retirement |
| A2 desktop lookup | Existing DesktopFacade implementation owns per-tab request lifecycle, request generation and service snapshot; closes/cancels on supersession/tab close/facade retirement | Headless DictionaryService, tab session and article composition; neutral intent/result values | Qt Widgets/WebEngine pointers, GUI thread requirement, format constructors; UI owning LookupRequest/Await | Headless operation tests plus UI parity; late result cannot update a newer tab request |
| A3 built-in composition | Core-private composition owns incomplete sources until transfer; ServiceState owns completed abstract sources/groups/coordinator/executor | Composition -> concrete formats and abstract registrations; query policy -> dictionary abstractions | Common query policy -> concrete format/startup API; formats -> UI; registry/DLL proliferation | Composition fixtures preserve catalog/errors; lifecycle fixtures prove backing objects outlive joined executor |
| A4 scenario ownership | Test target owns fixture profiles, doubles, scenario counters and fault schedules; product owns real object lifetimes | Test harness -> narrow private access and same production objects | Production target -> tests headers/sources/dispatcher; copied product algorithms in tests | Target graph plus equal scenario inventory/assertions and real-caller coverage |
| A5 transaction | Existing coordinator owns one transaction; each module owns prepared/reserved/published tokens and old-state leases | Existing private reservation/prepare/publish/finish seams | Recoverable allocation in reserved publish; post-decision rollback; event-loop reentry | Allocation-negative-control subprocess, exact publication/cleanup order, recovery and lease matrix |
| A6 resource outcome | Service owns lookup/extraction result; backend owns operation data until return; consumer owns received bytes/diagnostic values | Neutral outcome and preserved source code/identity; explicit legacy adapter | Core -> WebEngine/audio/HTTP status; success inferred from byte count | Outcome table through real service, browser/audio mapping and installed consumer |

Keep Adapter and optional capabilities, separate headless/desktop facades,
transaction coordinator, RAII/move ownership, snapshot/generation identity,
stale-callback guards, FullTextRequestController, rendered-match controller and
Inspector view-before-profile destruction. No new class is a prerequisite;
private functions/owned values inside existing modules are preferred where enough.

Approaches considered: (1) independently verified local seams in this queue
(recommended: smallest coherent changes); (2) only patch missing calls/allocations
(quick but leaves responsibility work open); (3) broad manager/factory rewrite
(large unneeded change surface, excluded). Moving files or forwarding methods
without transferring authoritative state and decisions cannot close an item.

### A6 outcome contract to select before interface implementation

Draft contract for review, not an installed API declaration:

- Outcome is one of success, dictionary-unavailable, resource-not-found,
  cancelled, deadline-exceeded, invalid-data, unsupported, or backend-failure.
  Error retains original known backend code, dictionary/resource IDs and bounded
  diagnostic detail. Never encode a GUI or HTTP error enum in Core.
- Success may contain zero bytes; failure carries no usable payload. Successful
  content retains media type without deriving success from media type or size.
- Check pre-cancellation before backend I/O and cancellation before delivering a
  success; cancellation takes precedence over an otherwise successful completion.
  Preserve a reported backend failure rather than inventing a later success.
  Crossed failure/cancellation ordering needs deterministic tests and explicit
  design approval, not incidental catch ordering.
- Keep the old byte API only as an explicitly named/documented compatibility
  adapter if selected. Preferred migration: add structured access and rebuild
  affected consumers, then migrate callers; adding a virtual still affects ABI.
  An atomic breaking signature change is a valid smaller alternative only with
  an approved full-consumer rebuild contract. Do not install two competing
  resource services. Package identity follows existing SCM/Conan revision policy.
- Browser mapping proposal: actual missing -> UrlNotFound; cancelled -> aborted;
  backend/invalid/deadline -> failed. Zero-byte success reaches the browser as a
  successful response. Audio may reject zero-byte success as unplayable while
  retaining that it was retrieved successfully. Exact Qt mappings, visible
  diagnostics and compatibility fallbacks require consumer/product approval.

## 4. Ordered independently closeable work orders

No implementation checkbox below is complete. Each selected unit requires fresh
baseline inspection, risk-appropriate verification, and independent no-history
non-modifying review under the activated contract before authorized delivery.
Do not combine tickets to avoid a failing gate. Runtime rollback means the
documented transaction behavior; delivery rollback means a normal follow-up
revert/change, never reset or published-history rewriting.

### W1 / A1 — Unify article lifecycle initialization

- Goal: restore refresh progression after reconstruction and eliminate the shared
  lifecycle policy's duplicate update sites.
- Non-goals: A5 token changes, request ownership migration, test-suite extraction,
  resource API, page visual redesign, or publication-gate relaxation.
- Expected files: main_window.cpp/.h and existing app-private ArticleView/Page
  wiring only if necessary; focused test code under apps/goldendict/tests;
  minimal owning test-target registration if required; selected OpenSpec record.
- Dependencies: existing approved reload/tab/hidden-candidate contract; focused
  minor-correction impact check. A2/A3/A4/A5/A6 are not prerequisites.
- Behavior acceptance: two sequential and overlapping refresh requests finish
  with no in-flight generation; later refresh can start; correct query/F3 state
  survives. Same result after Preferences, local source, online/external source
  and dictionary-group replacement. Close/reopen, stale/failed completion and
  abandoned candidate cannot affect active successor.
- Architecture acceptance: shared initial values and lifecycle event recipe,
  explicit candidate context, one owner event policy, inert relay before publish;
  no extra publication work or new service/DLL.
- Verification: new regression must fail on the unchanged implementation through
  the replacement route, not by directly setting load_started; normal route is
  its control. Real WebEngine events plus deterministic state observations; run
  existing search-menu, WebEngine interaction, tabs, hidden-facade and production
  source/group/Preferences smokes under isolated roots. Retain failing evidence.
- Rollback boundary: only this common wiring/correction and associated tests;
  no schema/data migration. Preserve regression history, reopen A1 if reverted.
- New guard: route-crossed lifecycle matrix; no new scenario bodies in MainWindow.

### W2 / A5 — Reserve publication storage before decision

- Goal: remove the two evidenced allocation crash points from reserved publish.
- Non-goals: changing durable records, failure visibility, owner order, locks,
  exception policy, whole allocator design or A1 wiring.
- Expected files: Core activation owner .cc/.h, Network runtime .cc and private
  transaction header, owning Core/Network tests; coordinator tests only for the
  existing boundaries. Keep coordinator production changes minimal if required.
- Dependencies: W1 is sequencing preference, not technical coupling. Existing
  transaction design makes this a minor correction after an impact check.
- Behavior acceptance: each token-storage allocation failure rejects before
  decision, preserves current Core/Network/Widgets authority and old work,
  unwinds candidates, and leaves no falsely committed desired runtime.
  Publish remains Network -> Core -> Widgets, then forward continuations;
  retained old readers and startup recovery still work.
- Architecture acceptance: prepared storage owns the future published token;
  publish performs no token allocation; abort/move/destruction remain one-shot.
- Verification: targeted allocation interception in a test-only subprocess,
  constrained to each production allocation site/phase. Old source demonstrates
  termination after decision; corrected source demonstrates rejectable failure
  before decision. Independently arm a no-allocation observation around reserved
  publication. Generic pre-boundary injection alone is insufficient evidence.
  Run application_service_test, http_client_test, full_text_index_test and
  configuration/Widgets coordinator smokes; exercise restart applying evidence.
- Rollback boundary: token representation and its prepare/publish transfer only;
  never roll back an already published runtime transaction.
- New guard: allocation-site negative controls and reviewed transitive publish
  operation inventory. Fail-stop invariants are preserved, not suppressed.

### W3 / A4 — Separate scenario ownership from production targets

- Goal: test-only ownership with the same real production implementation.
- Non-goals: changing product algorithms, dropping fault cases, turning real
  workflows into mocked successes, renaming away the production class burden.
- Expected files: main_window.cpp/.h scenario methods/state, main.cpp smoke-only
  dispatch, apps/goldendict/tests and owning CMake target definitions. Retain
  useful narrow production injection seams without embedding their scenarios.
- Dependencies: preserve W1/W2 regressions; approve target/composition seam.
- Behavior acceptance: before/after scenario and assertion inventory maps every
  existing smoke to a real path with equivalent fixtures/failure checkpoints;
  product startup and packaged smoke still exercise the real binary.
- Architecture acceptance: scenario definitions/doubles/dispatch absent from
  production target; no test includes in production; one implementation of each
  product operation. Test target links shared private production objects or an
  existing compatible seam; production class has no Run* scenario API.
- Verification: characterize the current suite first; compare CTest names,
  arguments, assertions and profile isolation; inspect target graph/link inputs
  and rebuilt production/test binaries. No weak count-only acceptance.
- Rollback boundary: migrate coherent scenario families independently (page,
  transaction, then remaining shell/state) with exact old/new coverage mapping;
  each family can be reviewed/reverted separately. W3 closes only after every
  inventoried family and smoke dispatcher is migrated. Do not stage one huge
  mixed migration then retrospectively split it.
- New guard: production-to-test dependency rejection and explicit old-debt list
  reduced per family, with no broad path exemption.

### W4 / A2 — Transfer the complete desktop lookup operation

- Goal: existing desktop owner controls requests; UI handles intent/presentation.
- Non-goals: unrelated history/Favorites policy migration, suggestions/full-text
  controller rewrite, format composition, asynchronous framework replacement.
- Expected files: desktop_facade.h/.cc, existing private tab/application code,
  main_window.cpp/.h consumers, application service/desktop tests, affected
  test doubles and installed consumer if the exported surface changes.
- Dependencies: W3 removes affected scenario churn; selected operation contract
  and independent readiness for the facade/API change. A6 outcome decision may
  be recorded early but resource implementation is not a lookup prerequisite.
- Behavior acceptance: current/new/background tabs, history recording exactly
  as before, back/forward, authoritative-empty/nonempty dictionary scopes,
  group/muting/filtering, no-result/error and HTML composition remain unchanged.
  Supersession, close, retirement and stale completion have one request owner.
- Architecture acceptance: requests_ backend ownership and Await decisions leave
  MainWindow; Core returns neutral completion identity/content. Render/page
  generations remain distinct from request identity. No GUI callbacks in Core.
- Verification: characterize operations before migration; service doubles control
  finish order but exercise the real desktop implementation. Test old facade
  retained during replacement and two requests completing out of order; replay
  existing UI tab/search/group/history regressions; rebuild installed consumers.
- Rollback boundary: one coherent end-to-end lookup use-case transfer with its
  API/consumer changes; do not ship half-transferred lifecycle state. If too large,
  return to design for a real functional boundary, not file-by-file splitting.
- New guard: MainWindow cannot create/retain raw LookupRequest or call Await;
  headless operation lifecycle acceptance.

### W5 / A3 — Isolate built-in construction and startup details

- Goal: private composition changes independently of general query policy.
- Non-goals: plugin discovery framework, new DLL, backend behavior rewrite,
  changed IDs/order/groups, new executor schedule or persistence format.
- Expected files: dictionary_service.cc, a private composition function/product
  beside existing Core application composition, private AARD lifecycle seam only
  if needed, owning tests and Core source membership.
- Dependencies: approved clarification of P8-FT-87/88 responsibility; W4 preferred
  to avoid concurrent edits, not a hard API dependency.
- Behavior acceptance: catalog order/identity, skipped-source diagnostics, groups,
  morphology and external source ordering, policy/reconciliation and executor
  activation/destruction match the characterized baseline.
- Architecture acceptance: common query code has no concrete format constructor,
  type or startup evidence method; composition can depend on them. ServiceState
  continues to control executor shutdown and abstract source lifetime.
- Verification: tiny built-in/mixed/external fixtures; composition failure with
  no submitted candidate work; AARD startup artifact and old-snapshot tests;
  full_text_index_test/application_service_test and existing backend tests.
- Rollback boundary: private composition transfer only, no public resource API.
- New guard: concrete-format include edges confined to named composition owners,
  plus runtime lifetime proof (textual include checks alone are insufficient).

### W6 / A6 — Preserve resource outcomes through compatibility boundaries

- Goal: distinguish absence, cancellation, backend failure and successful empty
  content through the real service; preserve typed identity/error detail.
- Non-goals: changing URL security, resource limits, parsers, retry/network policy,
  new transport service, broad facade segregation or visible message redesign.
- Expected files: dictionary_service.h/.cc, resource DTO location in existing
  installed Core surface, article_scheme_handler.cpp, audio_playback_service.cpp,
  their tests and test_package/headless_api_test.cpp; runtime backend contract
  comments/tests only where necessary. No forced format rewrite.
- Dependencies: select outcome/API/consumer mapping before W4/W5 implementation
  contracts are frozen. Implementation may follow W5 to avoid shared-file churn;
  it can precede W5 if urgent because the outcome contract uses existing abstract
  sources. Neither W1 nor W2 depends on it. Requires genuine OpenSpec delta,
  approval and independent interface/compatibility readiness.
- Behavior acceptance: data-driven unknown dictionary, absent resource,
  nonempty success, empty success, pre/during cancellation, invalid data,
  unavailable/unsupported/deadline and backend failure all remain distinguishable;
  legacy mapping occurs only at the chosen adapter. UI mappings follow approval.
- Architecture acceptance: DTO has no Qt/transport types; service never uses
  bytes.empty() as outcome; no uncaught exception policy chosen by accident.
- Verification: real service with controlled RuntimeDictionarySource plus a local
  resource fixture, installed consumer build, real scheme handler and audio sink
  tests; retain byte-API compatibility tests while that API remains supported.
  Demonstrate the current loss using the old API before claiming regression.
- Rollback boundary: versioned interface/consumer unit or explicitly compatible
  staged adoption. Never leave consumers compiled against a different ABI.
- New guard: exhaustive outcome tests and explicit compatibility mapping inventory.

## 5. Proposed minimal rules and checks (not active)

Suggested short AGENTS entry, after approval, linking to existing focused rules:

> For lifecycle or ownership changes, identify the authoritative state owner,
> permitted dependencies, and affected normal/replacement/error paths in the
> OpenSpec record. Keep scenario orchestration and doubles in test targets.
> Preserve the documented preparation/publication boundary and typed resource
> outcomes. Validate shared paths and register remaining violations explicitly;
> file moves, forwarding layers, counts, and broad exemptions do not prove closure.

Detailed contracts belong in existing project-design-rules/architecture/testing
sections, with selected OpenSpec deltas only for actual changes: A1 shared recipe
and hidden gating; A2 per-tab request completion/cancel/retirement; A3 composition
ownership and backing lifetimes; A4 production/test target seam; A5 preallocated
publish operation list and OOM boundaries; A6 outcome/ABI/compatibility mapping.
Do not amend Accepted historical records to describe their defects as intended.
Add a dated superseding decision referencing them when a design must change.

Candidate CI/build checks, to implement only in selected tickets:

- Inspect target source/link graph and compiler-resolved dependencies to reject
  production -> tests, public Core -> GUI/transport, and common query -> concrete
  format edges. Account exactly for the existing private image-codec exception
  and the approved helper process; do not broadly exempt Core or apps.
- Block new forbidden request ownership in MainWindow; textual searches provide
  a cheap warning, while compiler/AST analysis and human review resolve aliases
  and indirect calls. No line-count/SOLID score thresholds.
- Audit runtime launchers for unique task-owned HOME/XDG/APPDATA/LOCALAPPDATA/
  GOLDENDICT_TEST_CONFIG_ROOT, TEMP/TMP, index and cache directories; fixture
  dictionaries are read-only and network fixtures are local. Refuse recursive
  cleanup unless the resolved target is owned by that exact test invocation.
- Run A1 route matrix, A5 allocation/state-order tests and A6 outcome matrix;
  retain W3 scenario-to-target mapping and A2/A3 behavior characterization.
  Fail on missing binaries, zero selected tests and unintended skips, even if
  discovery tools themselves return zero. Do not suppress assertions to pass.

Mandatory human review: cohesive state/decision ownership; transitive fallibility
of publish including callbacks/destructors; worker cancellation/lease lifetime;
equivalence to Qt5 observable behavior; useful versus ceremonial abstractions;
consumer-specific safe diagnostic disclosure and actual ABI compatibility.

Explicit debt outside selected closure: original A7 facade breadth; app compiling
three Core-private transaction sources; composition-root history/Favorites policy;
unmeasured cooperative-cancellation latency; historical/main-window parity gaps;
missing test binaries; Windows WebEngine offscreen/path-sensitive historical
failures. Do not expand these debts while fixing a selected ticket. No global
architecture/SOLID conformance or absence of races is claimed.

## 6. Commands, outcomes, and unverified matters

Commands were executed with explicit target workdir except the initial shell
location and ancestor-rule inspection. Repeated Get-Content/rg reads were used
for the source ranges listed above; all reads and failed probes changed no
source. PowerShell batch exit zero is not proof every inner command succeeded.

| Command/check | Outcome |
| --- | --- |
| Get-Location; git -C target rev-parse --show-toplevel / branch --show-current / rev-parse HEAD / status --short | Exit 0; initial master shell, target root/branch/HEAD above, clean status |
| Get-Content rules/audit; Get-ChildItem ancestor AGENTS; rg --files --hidden -g AGENTS* | Applicable rules inventoried; no module overrides found; candidate policy excluded |
| Selector JSON + Get-FileHash SHA256 comparison | Four installed hashes matched, enabled candidate profile |
| git fetch --no-tags --no-recurse-submodules origin | Succeeded, no branch checkout/reset; tracking refs only |
| git rev-list --left-right --count HEAD...origin/feature/tiger-qt6-migration | 0 0 |
| git diff --exit-code 3f3f2bf4 HEAD | Exit 0, empty |
| git log -5; git blame -L 384,394 / 8603,8610 main_window.cpp; blame Core 235 and Network 983,985; git show 638161b93 / 849056916 | Successful history inspection; evidence described in A1/A5 |
| openspec context --json; openspec list --json | Exit 0; exact target root, preexisting main-window-parity in-progress (6/12), not completion proof |
| rg CMAKE_HOME_DIRECTORY/CMAKE_BUILD_TYPE/CMAKE_GENERATOR build/Release/CMakeCache.txt | Exit 0; correct target root, Release, Ninja |
| run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -N | Exit 0; 143 registrations; numerous executable-not-found warnings; no tests executed |
| Test-Path build/Release/bin/goldendict.exe / application_service_test.exe / http_client_test.exe | True / False / False; application existence is not exact-HEAD binary provenance |
| git diff --check (before Draft) | Exit 0; no tracked edits |

Failed exploratory probes retained in this record: querying build/CMakeCache.txt
failed because the actual cache is under build/Release (rg error 2); literal
PowerShell paths ending runtime* caused rg invalid-path errors (123); searching
a nonexistent root tests directory caused rg error 2. Subsequent searches used
existing directories and -g filters. Some initial long read outputs were
truncated; policy tail, full audit and focused affected contracts were re-read.
No missing output is treated as verified evidence.

No compilation, product test, UI reproduction, allocation failure experiment,
Qt5 parity run, Linux/macOS runtime check or full semantic reference analysis
was performed. Serena portable configuration exists, but no reliable semantic
session/database was activated or reconfigured; this is explicit textual
relationship coverage for a Draft. Cross-module implementation needs reliable
semantic analysis or recorded equivalent consumer coverage before readiness.
No new OpenSpec validate/Ready claim applies to this single Draft attachment.
No fresh independent completion auditor was invoked because this is an
unapproved planning output, not an implementation candidate for delivery.

## 7. Next request limited to A1

Select W1 only. Keep this exact requested worktree and preserve this Draft and
any later changes. Reconfirm baseline/sole-writer role and selected contract;
do not infer permission to change worktrees or advance other tickets.

1. Record the minor-correction impact check against CRD-SHELL-003,
   CRD-LOOKUP-003/004 and the existing hidden Widgets contracts. Confirm the
   helper changes no publication phase or facade API.
2. Restore/build only the required existing test targets through the documented
   Conan launcher when implementation/build permission is given. Do not upgrade
   or reconfigure tools automatically. Establish executable-to-source identity.
3. Add an A1 regression in test-owned code with a narrow private observation seam
   if required; do not add another MainWindow Run* scenario. Use a local fixture,
   unique short profile/index/cache roots, and the real owner/candidate/relay.
4. Normal page control: load a searchable local document, trigger refresh again
   while the first is active, observe real started/terminal events, drained state,
   correct article search and ability to start a third refresh.
5. Repeat on a prepared/published replacement and after actual Preferences,
   local-source, online/external-source and group invocations using local test
   adapters. Negative control must expose the unmodified relay's missing event
   propagation. Synthetic loadFinished during an active load cannot falsely
   retire it. Failed terminal completion must retire the actual current load.
6. Implement common initialization and owner event handling with explicit staging
   inputs. Retain inactive candidate silence and generation/view checks; preserve
   existing binding-slot/retirement ownership and publication operation allowlist.
7. Verify hidden/stale/aborted candidate emissions, tab close/page replacement,
   two immediate facade generations, back/forward, search/F3 and scroll/zoom/
   click/Inspector initialization. Run relevant existing regression smokes and
   record platform/runtime failures separately. Windows offscreen failure is not
   an A1 regression proof; use matched native execution when required.

Candidate focused command after test binaries and isolation are verified:

```powershell
.\run_with_conan.ps1 --build-type Release -- ctest --preset conan-release -j 1 -R '^goldendict_(search_menu|webengine_interaction|article_tabs|widgets_facade_preparation|preferences_coordinator_predecision|source_directories|dictionary_groups)_smoke$' --output-on-failure
```

This command is proposed, NOT run. It does not replace the new explicit
replacement-refresh regression. A1 closes only with old-failing/new-passing
evidence for that regression, applicable matrix results and independent exact
candidate review. If the red case cannot run reliably, record the missing event,
fixture or environment condition and keep A1 open. Stop after A1's authorized
delivery boundary; A5 and all architecture/API migrations remain separate.
