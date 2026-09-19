# W3 current test-responsibility inventory

Snapshot: feba6359e2f630cffde9c02a1e20d0765e163485, before W3.1 extraction.
Recounted from source: **53 MainWindow Run*Check definitions**, including the
private RunArticleSearchReloadCheck helper; **62 distinct literal main.cpp flags**
(not all tests); **16 tracked entry/consumer files** matching smoke/test/CTest
dispatch searches. These are inventory counts, not architecture acceptance metrics.

Complete per-method source spans, main callsites/nearest flag, local types,
state dependencies, real called symbols, scheduling and assertion/fault lines are
recorded in the source-derived `mainwindow-inventory.json`; all original method
bodies are preserved in `legacy-methods.txt`. Main flags and all matching
CMake/script line references are in `main-flags.json` and `entry-inventory.json`.
All are under `D:/workspace/goldendict/evidence/a4-test-extraction-w3-1-20260912`;
inventory.py records the reproducible extraction, using C++ string/comment masking
for body boundaries. The following family ledger interprets that evidence rather
than treating every check-like symbol as removable testing.

## MainWindow family ledger

All names below have prefix `Run` and suffix `Check`; `Smoke` remains part of the
name where shown. Exact source positions and full state/assertion lists are in the
per-method inventory. Unless selected, all are **pending, unchanged**. Proposed
destinations are focused `apps/goldendict/tests/` targets, not approved follow-on
implementations. Run current CTest entries through the checkout Conan launcher:
`ctest --preset conan-release -j 1 -R '^<entry>$' --output-on-failure` with the
isolated profile script. Multi-process entries retain their CMake runner.

| Family / exact method names | Original CTest entry or runner | Core assertions and failure branches | State, timing and real production path | Intended destination/status |
| --- | --- | --- | --- | --- |
| HelpMenuSmoke | goldendict_help_menu_smoke / help_menu_smoke.cmake | Menu/action identity, shortcuts, single dispatch, safe URLs/config path and rejected unsafe destinations | Help dialogs, external URL dispatcher, real actions; modal scheduling | help_menu_test; pending |
| ProductShellSmoke | goldendict_product_shell_smoke / product_shell_smoke.cmake | Dock hierarchy/default placement, visibility/layout restoration, translated titles, malformed/offscreen geometry fallback | DockTitleTestTranslator local substitute; real QMainWindow docks, state persistence and event processing | product_shell_test; pending |
| ViewMenuSmoke | goldendict_view_menu_smoke | View action identity, toggles/state/shortcuts, single dispatch and restoration | Real view actions, toolbar/dock state, preferences | view_menu_test; pending |
| HistoryMenuSmoke | goldendict_history_menu_smoke | Menu order, unique commands, import/export/cancel/empty/busy state and restored session | Path providers, real history actions and callbacks | history_menu_test; pending |
| FavoritesMenuSmoke | goldendict_favorites_menu_smoke | Menu identity, add/remove/import/export, empty/busy/invalid/cancel paths, preserved pane/session | Favorites path/confirmation providers; real tree and actions | favorites_menu_test; pending |
| EditMenuSmoke | goldendict_edit_menu_smoke | Menu/action ownership, shortcuts, source/preferences modal single dispatch, cancel/failure preservation | Dialog executor substitutions; real apply callbacks and UI | edit_menu_test; pending |
| HistoryPreferencesSmoke | goldendict_history_preferences_smoke | Preferences commit/cancel/failure, import/export and history policy preservation | Real preferences dialog/apply callback and history paths | history_preferences_test; pending |
| PreferencesCoordinatorPredecisionSmoke | goldendict_preferences_coordinator_predecision_smoke | Injected predecision rejection preserves config bytes/facade/cache; retry and boundary sequence | main-owned injection/boundary trace; real configuration coordinator and Preferences callback | preferences_transaction_test; pending |
| FavoritesPreferencesSmoke | goldendict_favorites_preferences_smoke | Successful/cancelled/rejected preference application preserves Favorites data/session | Real Preferences apply, Favorites pane and provider callbacks | favorites_preferences_test; pending |
| ArticlesPreferencesSmoke | goldendict_articles_preferences_smoke | Article-related preferences propagate; invalid/cancel/failure preserves previous values | Actual dialog, apply callback, article views/settings | articles_preferences_test; pending |
| DictionaryContextPreferencesSmoke | goldendict_dictionary_context_preferences_smoke / dictionary_context_preferences_restart.cmake | Context lookup/control preference behavior and restart persistence | Actual page/menu navigation and persisted preferences; asynchronous article load | dictionary_context_preferences_test; pending |
| SynonymPreferencesSmoke | goldendict_synonym_preferences_smoke | Synonym setting application and cancel/failure preservation | Real facade/service and Preferences wiring | synonym_preferences_test; pending |
| OptionalPartsPreferencesSmoke | goldendict_optional_parts_preferences_smoke | Optional-part rendering preferences and preserved previous behavior on reject/cancel | Actual article loads and preferences | optional_parts_preferences_test; pending |
| ProxyPreferencesSmoke, ProxyPreferencesRestartSmoke | goldendict_proxy_preferences_smoke and proxy_preferences_restart.cmake | Proxy fields, accepted/cancelled/rejected changes and restart values | Actual preferences application; no public proxy needed | proxy_preferences_test; pending |
| NetworkCachePreferencesSmoke, NetworkCachePreferencesRestartSmoke | goldendict_network_cache_preferences_smoke and network_cache_preferences_restart.cmake | Cache policy, path/binding, positive/zero transitions, failure and restart preservation | Actual Network runtime and settings callback; cache fixtures | network_cache_preferences_test; pending |
| HideSingleTabPreferencesSmoke, HideSingleTabRestartSmoke | goldendict_hide_single_tab_preferences_smoke / view_preferences_restart_smoke.cmake | One/multiple tab visibility, preference transitions and restart | Real tab bar and stored preferences | tab_visibility_preferences_test; pending |
| EscapeHidesMainWindowPreferencesSmoke, EscapeHidesMainWindowRestartSmoke | goldendict_escape_hides_main_window_preferences_smoke / escape_hides_main_window_restart.cmake | Default/enabled Escape, focused input consumption, child/modal precedence, cancel/failure/restart | Real key events, window visibility and preferences | escape_preferences_test; pending |
| ArticleClickPreferencesSmoke, ArticleClickRestartSmoke | goldendict_article_click_preferences_smoke / article_click_restart.cmake | Click preference case matrix, input modifiers, accepted/rejected/restart settings | Local Case table; actual article events and navigation | article_click_preferences_test; pending |
| MruTabOrderPreferencesSmoke, MruTabOrderRestartSmoke | goldendict_mru_tab_order_preferences_smoke / view_preferences_restart_smoke.cmake | MRU traversal versus tab order, preference changes and restoration/restart | Real keyboard/tab state, session and preferences | mru_preferences_test; pending |
| SearchMenuSmoke | goldendict_search_menu_smoke | Exact menu/actions, unique Find shortcut, search match/no-match, per-tab restoration, late completion of closed tab | Actual ArticleView loads, 150 ms staged callbacks, reload/search state maps | search_menu_test; pending |
| FileMenuSmoke | goldendict_file_menu_smoke / file_menu_smoke.cmake | File menu identity, output/cancel/failure, rescan/quit routes and state preservation | Output writers/providers and quit dispatcher; real QAction routes | file_menu_test; pending |
| WebEngineInteraction, ArticleSearchReload (private helper), WebEngineSmoke | goldendict_webengine_interaction_smoke; goldendict_webengine_smoke | Real loads/search/scroll/zoom/link behavior and search continuity through reload | ArticlePage/View, loadFinished and timer steps; helper shared within this family | webengine_interaction_test; pending |
| ArticleContextMenu | goldendict_article_context_menu_smoke | Context action availability/dispatch and navigation state | Real page context events and ArticleView callbacks | article_context_menu_test; pending |
| DictionaryContextNavigation | goldendict_dictionary_context_navigation_smoke | Dictionary-context target/group/history behavior | Real facade lookup/tab navigation and asynchronous page events | dictionary_context_navigation_test; pending |
| SystemPrint | goldendict_system_print_smoke | Printer availability/cancel/failure, page setup/preview/print ownership and late output validity | Printer/dialog/PDF dispatch substitutes; real output request lifecycle | system_print_test; pending |
| SuggestionPaneSmoke | goldendict_suggestion_pane_smoke | Suggestions/action behavior, asynchronous identities and stale completion protection | Real suggestion worker, popup controller, facade and tab state | suggestion_pane_test; pending |
| ArticleTabsSmoke, ArticleTabSessionRestartSmoke | goldendict_article_tabs_smoke / article_tabs_smoke.cmake; article_tab_session_restart.cmake | Fore/background tabs, close/placement/history/restoration limits and restart identity | Real tab/session facade, page loads, settings and multiple processes | article_tabs_presentation_test; pending |
| HistorySmoke, HistoryManagementSmoke, HistoryExportSmoke, HistoryImportSmoke | goldendict_history_smoke, goldendict_history_management_smoke, goldendict_history_export_smoke, goldendict_history_import_smoke | Pane population/activation/clear; export/import content and errors | Actual callbacks/store through main composition; paths provided by runner | history_presentation_test; pending |
| FavoritesSmoke, FavoritesCrossFolderMoveSmoke, FavoritesTransferSmoke | goldendict_favorites_smoke, goldendict_favorites_cross_folder_move_smoke, goldendict_favorites_transfer_smoke | Tree add/edit/delete/move/activation and XML transfer, rejection/cancel/preservation | Actual tree gestures, callback/store wiring and paths | favorites_presentation_test; pending |
| DictionaryBrowserSmoke, DictionaryBrowserExportSmoke | goldendict_dictionary_browser_smoke; goldendict_dictionary_browser_export_smoke | Browser visibility/content and real headword export completion | Real browser/service/export controller | dictionary_browser_presentation_test; pending |
| DictionaryGroupsSmoke | goldendict_dictionary_groups_smoke | Selection/create/rename/order/remove, muted/unresolved identities and invalid-save preservation | Real group editor/apply callback, article session and dictionary projections | dictionary_groups_presentation_test; pending |
| DictionaryBarSmoke | goldendict_dictionary_bar_smoke | Catalog identity/accessibility and toolbar hierarchy; reordered/muted group, independent all/group state, all-off empty results/suggestions, hidden unfiltered lookup | Real toolbar host, QAction triggers, group membership, StartLookup/FinishLookup and asynchronous completion | dictionary_bar_test; W3.2 migrated and behavior verified |
| WidgetsFacadePreparationSmoke | goldendict_widgets_facade_preparation_smoke / widgets_facade_preparation_smoke.cmake | Hidden completeness, unchanged active state, failure-step unwind, relay suppression, abandonment/thread/late callback and publication restrictions | Four test-only fault fields; real Prepare/Begin/Publish/Finish and leases; timers and cross-thread abandonment | widgets_facade_preparation_test; pending |
| DictionaryStatusPresentationSmoke | goldendict_dictionary_status_presentation_smoke | Current/candidate status text, tab style/closability, stale-text replacement and reclaimer completion | Real catalog formatter and Widgets candidate maintenance/publication | dictionary_status_presentation_test; pending |
| FullTextDictionaryProjectionSmoke | goldendict_full_text_dictionary_projection_smoke | All supported IDs; unchecked exclusion; hidden fallback; muted/unresolved empty group; no lookup request; composer stays hidden | Actual MainWindow ComposeFullTextQuery/SelectGroup/RefreshDictionaryBar, toolbar actions and event loop; no fault injection or persistent scenario fields | **full_text_dictionary_scope_test; W3.1 selected** |
| FullTextDialogSmoke | goldendict_full_text_dialog_smoke | Dialog lifecycle, query/results/filter/navigation/geometry and rejected/stale/cancelled work | CapturingDesktopFacade local substitute; actual dialog, controller and service; asynchronous scheduling | full_text_dialog_integration_test; pending |
| InspectorGeometrySmoke | goldendict_inspector_geometry_smoke / inspector_geometry_smoke.cmake | Geometry accepted/fallback/persisted restart | Real inspector window and captured geometry callback | inspector_geometry_test; pending |
| SourceDirectoriesSmoke | goldendict_source_directories_smoke | Source dialog/list edits, cancel/failure/success, real reconstruction and session preservation | Actual source apply callback; main also owns fixture/failure assertions | source_directories_test; pending |

## Main-only responsibilities and product boundaries

main.cpp includes `../tests/view_preferences_smoke.h`: its view-preference/restart
helpers remain a documented legacy production-to-test header dependency. Main also
owns IsSmokeInvocation, HasSmokeArgument, HasPreferencesSmokeArgument,
DictionaryRootArgument, per-family fixture seeds, timers/exit-code dispatch,
configuration/restart verification, preferences predecision injection/trace,
coordinator boundary assertions, source-edit failure steps and geometry handlers.
These shared helpers are retained for unselected families; the selected flag is
removed from their conditions without extending their behavior.

Main-only families: article-scheme-registration verifies pre-QApplication scheme
registration; help-presentation verifies actual QtHelp setup (Linux); interface-
language startup/unsupported/Russian verifies installed translations and restart;
configuration-reload-coordinator checks the full real transaction and ordering;
view-preferences-restart verifies persisted flags; inspector restart and article
tab-session prepare/verify own multi-process checks. Their exact flags and source
references are in main-flags.json and entry-inventory.json; all remain pending.
Future destinations must preserve those actual startup/recovery/multi-process
paths, not merely call a helper. Main's source/directories/Preferences/Favorites
test branches also own persistence assertions beyond their MainWindow methods;
future migration must move both halves together.

| Main-owned family | Actual entry / command suffix | Assertions, state and failure branches | Destination/status |
| --- | --- | --- | --- |
| Scheme registration | goldendict_article_scheme_registration_smoke; --article-scheme-registration-smoke | Real pre-QApplication RegisterArticleScheme then ArticleSchemeRegistrationIsValid; invalid registration returns 1 | startup_scheme_test; pending |
| Installed help presentation (Linux) | goldendict_installed_runtime_smoke / installed_runtime_smoke.cmake; --help-presentation-smoke <help-dir> | Real HelpWindow IsReady and ShowIdentifier(Content); absent/invalid collection fails; package helper process preserved | installed help runner; pending |
| Interface language (Linux) | goldendict_interface_language_smoke / interface_language_restart.cmake; --interface-language-startup-smoke, --interface-language-unsupported-smoke, --interface-language-russian-smoke | Real InterfaceTranslations app and Qt catalogs, exact Russian/English strings and unsupported fallback; isolated persisted preferences across processes | interface language startup runner; pending |
| Configuration reload coordinator | goldendict_configuration_reload_coordinator_smoke; --configuration-reload-coordinator-smoke | Real coordinator trace/order/result, desired config and current generation; injected preparation/persistence/maintenance boundaries, prior state preservation | focused configuration coordinator integration test; pending |
| View restart | goldendict_view_preferences_restart_smoke / view_preferences_restart_smoke.cmake; --view-preferences-restart-smoke | ViewPreferencesRestartSmoke in test header; phase/enabled environment input, actual window/dock/tab visibility and persisted restart values after scheduled startup | view preferences runner; pending |
| Inspector restart half | goldendict_inspector_geometry_smoke / inspector_geometry_smoke.cmake; --inspector-geometry-restart-smoke | MainWindow result plus LoadConfiguration inspector_geometry equality; empty values and persistence exceptions fail; close/exit captures and watchdog | inspector runner with existing method family; pending |
| Article session restart half | goldendict_article_tab_session_restart_smoke / article_tab_session_restart.cmake; --article-tab-session-restart-prepare / --article-tab-session-restart-verify | Scheduled true/false phases of real RunArticleTabSessionRestartSmokeCheck; persisted tab session, no loss of its asynchronous assertions; 15-second watchdog is not success | article session runner with existing method family; pending |

Main-owned injection state also includes preferences_predecision_injection,
source_predecision_injection, source_reload_boundaries, group_reload_injection,
group_reload_traces and the Preferences boundary list. source_reload_boundaries
records the real source reload checkpoints and verifies both injected-failure
termination and successful transaction ordering. The group branch's local
SmokeState owns byte snapshots and desired/unchanged/rollback assertions. These are
pending with their complete source/group/Preferences families, not shared product
state to preserve indefinitely. PreparedProductionFacade and StartupRecoverySelection
are production composition/recovery records and are not classified as test doubles.

`--smoke` is the existing loader/package probe returning success before GUI startup
and is used by installation checks and the external-program fixture. Preserve it.
`--dictionary-root` selects source input shared by diagnostics and test runners;
preserve it. `--literal` and the production initial-lookup/single-instance parsing
are product command behavior, not test steps. Runtime composition diagnostics,
startup recovery/quarantine, safe external URL dispatch, printing and Qt scheme
registration are production behavior despite check/diagnostic-like names. None
is removed merely by name. No new promise is made that the inventory's literal
flag set is the entire public command grammar.

## Persistent fields, substitutes and scheduling

Four explicit MainWindow fault fields remain: widgets_cleanup_failure_injected_,
facade_preparation_failure_step_, facade_maintenance_failure_step_, and
facade_final_validation_failure_. They belong to the unselected Widgets transaction
family. The selected projection family has no dedicated persistent scenario field,
substitute class or injected fault step; its local composer/supported-ID snapshots,
action selection and request-count assertion move entirely to the runner.

MainWindow local substitute types found in test methods: DockTitleTestTranslator,
ArticleClick's Case table and FullTextDialog's CapturingDesktopFacade. Lambda-based
substitutes additionally replace dialog execution, import/export path selection,
removal confirmation, printer/PDF dispatch and writers, article save and quit.
Their production default executors/providers are real narrow operation seams,
not automatically test-only fields: retain defaults and migrate only future
test-owned assigned lambdas. Callback fields that connect ordinary application
composition (source/preferences/history apply) remain production responsibilities.
The per-method state/call/scheduling records identify their actual consumers.

## Build, CTest, CI and script entries

apps/goldendict/CMakeLists.txt owns direct executable smoke registrations and
the application QTest source/link targets. Existing W1/W2 targets already reuse
the production presentation source/link closure minus main.cpp. This family
reuses that closure under BUILD_TESTS, retaining the old CTest name.

The tracked CMake runners are article_click_restart, article_tabs_smoke,
dictionary_context_preferences_restart, escape_hides_main_window_restart,
file_menu_smoke, help_menu_smoke, installed_runtime_smoke,
interface_language_restart, network_cache_preferences_restart,
product_shell_smoke, proxy_preferences_restart, view_preferences_restart,
widgets_facade_preparation_smoke, inspector_geometry_smoke and
article_tab_session_restart. Some assemble arguments from variables and therefore
are listed here beyond literal-flag regex hits. None directly invokes the selected
projection flag. scripts/prepare_qt5_acceptance_source.py is frozen-reference
acceptance instrumentation, not a Qt6 migration destination. tools/delivery/preflight.py
mentions CTest evidence as policy data, not an alternate runtime dispatcher.

No tracked CI pipeline YAML/Jenkins file exists at this baseline; tracked YAML
files are Serena, Conan data and OpenSpec configuration. External CI configuration
is not available and is not asserted audited. Existing whole-suite CTest invocation
automatically follows the retained test name. Auxiliary Conan/IDE launch scripts
remain generic launchers, not selected-family orchestration.

BUILD_TESTS=OFF historically still configures several application test targets
because their registrations are unconditional. That legacy condition and the
view_preferences_smoke.h dependency are explicit pending baseline items; this
batch adds no exemption and does not claim all production/test coupling removed.
Acceptance requires this selected runner and its test-owned implementation absent
from production's actual source/link dependency closure in both configurations.

## W3.1 resulting inventory delta

Exactly one definition/declaration, its three main.cpp flag/dispatch sites and its
old product-executable CTest command leave production. The current count becomes
52 Run*Check definitions; no other scenario is counted as migrated. The selected
family's local state and all assertions live in full_text_dictionary_scope_test.cpp.
No dedicated field or substitute existed for this family, so none is claimed
removed. Shared main.cpp fixture values remain for other consumers. The selected
CTest name remains, now referring to the test-only target; the new architecture
probe is an additional build-boundary test, not a replacement behavior assertion.

W3/A4 remains in progress. The remaining 52 definitions, main-only families,
view_preferences_smoke.h dependency, four fault fields and unconditional historical
app test targets remain explicit pending work. No second family is authorized here.

## W3.2 migration-mode classification (base e77627a9)

This classifies the remaining ledger entries; it is not a new audit or approval of
future designs. W3.1 source identities remain applicable because this base is its
reviewed candidate. A means reuse the current real-window composition/target and
bounded dictionary-scope access; B needs finite additional test access or operation
substitution; C needs a separately designed startup/persistence/transaction test
boundary. C does not assert that product ownership must change, nor authorize W4.
All entries below remain pending unless a subsequent verified status says otherwise.

| Mode | Remaining families | Actual boundary consideration |
| --- | --- | --- |
| A: current migration mode | DictionaryBarSmoke | Same real catalog, toolbar host, group selection and request-count observation as W3.1. Real StartLookup/FinishLookup slots and event scheduling must remain; no lookup ownership migration. **W3.2 locked family**. |
| B: finite test access | HelpMenuSmoke, ProductShellSmoke, ViewMenuSmoke | Need scoped dialog/URL/preferences callback substitution, layout/geometry observations or translation fixture ownership; do not force them into dictionary-scope access. |
| B: finite test access | SearchMenuSmoke; WebEngineInteraction/ArticleSearchReload/WebEngineSmoke; ArticleContextMenu; DictionaryContextNavigation; SuggestionPaneSmoke | Real asynchronous page/lookup bindings, per-tab identity and scheduling must remain. More observations than W3.1; no new thread lifetime or query owner may be introduced. |
| B: finite test access | DictionaryBrowserSmoke/DictionaryBrowserExportSmoke | Need bounded real browser/export completion and path-provider access; entire paired family must preserve asynchronous export and errors. |
| C: startup/persistence boundary | HistoryMenuSmoke, FavoritesMenuSmoke, EditMenuSmoke, FileMenuSmoke, SystemPrint | Existing callback/provider substitutions and main-composition persistence/output/quit halves cannot be removed by merely moving a method. Preserve production defaults and design complete runner composition first. |
| C: preferences/restart boundary | HistoryPreferencesSmoke, PreferencesCoordinatorPredecisionSmoke, FavoritesPreferencesSmoke, ArticlesPreferencesSmoke, DictionaryContextPreferencesSmoke, SynonymPreferencesSmoke, OptionalPartsPreferencesSmoke, ProxyPreferencesSmoke/ProxyPreferencesRestartSmoke, NetworkCachePreferencesSmoke/NetworkCachePreferencesRestartSmoke, HideSingleTabPreferencesSmoke/HideSingleTabRestartSmoke, EscapeHidesMainWindowPreferencesSmoke/EscapeHidesMainWindowRestartSmoke, ArticleClickPreferencesSmoke/ArticleClickRestartSmoke, MruTabOrderPreferencesSmoke/MruTabOrderRestartSmoke | Actual main.cpp apply/persistence callbacks and, where present, restart scripts/failure scheduling need a complete startup test boundary. No public contract or transaction change is authorized. |
| C: persistence/restart boundary | ArticleTabsSmoke/ArticleTabSessionRestartSmoke; HistorySmoke/HistoryManagementSmoke/HistoryExportSmoke/HistoryImportSmoke; FavoritesSmoke/FavoritesCrossFolderMoveSmoke/FavoritesTransferSmoke; DictionaryGroupsSmoke; SourceDirectoriesSmoke; InspectorGeometrySmoke | Main-owned stored-state assertions, multi-process phases or real configuration coordinator wiring remain part of each family. |
| C: prepared-resource observation | WidgetsFacadePreparationSmoke, DictionaryStatusPresentationSmoke | Private prepared-resource records, reclaimer and publication-stage/fault observations require a separately bounded test surface. W1/W2 behavior must not be redesigned. |
| C: replacement facade and asynchronous orchestration | FullTextDialogSmoke | CapturingDesktopFacade and request/match-plan schedules plus main-owned persisted geometry checks exceed the dictionary-scope seam. No W4 ownership change is selected. |
| C: main-only startup boundary | Scheme registration; installed help presentation; interface language; configuration reload coordinator; view restart; inspector restart half; article-session restart half | Preserve actual pre-QApplication/startup/installed-runtime/restart phases. They are not helper-only tests. |

The locked W3.2 batch contains only DictionaryBarSmoke. Three is a ceiling, not a
quota. No substitute family may be added silently. The initial implementation
hold for no-argument startup was superseded by the clarification below. W3.1
remains migrated and W3/A4 remains in progress.

W3.2 isolation clarification: the earlier startup gate no longer blocks the locked
family migration. Unsafe ordinary startup remains explicitly unverified; see
w3-2-status.md. No substitute family is selected.

## W3.2 current inventory delta

DictionaryBarSmoke is the only additional migrated family. Together with W3.1
FullTextDictionaryProjectionSmoke, two complete families now belong to test targets.
There are 51 remaining MainWindow Run*Check definitions (the preceding 52-count
paragraphs describe the preserved W3.1 snapshot). No other family was moved.
A/B/C classification above remains the pending/limited-access/dependency register.
Main-only startup/recovery families and legacy OFF test dependencies remain pending.
The shared disabled-online/enabled-external source fixture in main.cpp remains for
source directories, status, facade preparation, coordinator and full-text dialog.

## W3.3 locked batch (base 75de4fa4)

Exactly one complete family was selected: **ViewMenuSmoke** (initially assessed as
mode B; the verification below corrects that assessment). The limit of three is
not a quota. Original method
`MainWindow::RunViewMenuSmokeCheck`, main flag/dispatch `--view-menu-smoke`, and
CTest `goldendict_view_menu_smoke` map to `view_menu_test`. Selection is based on
one real-window UI path, existing public Preferences callback substitution and
synchronous GUI-thread action/event execution. The initial assessment missed the
real main-owned callback restored between substituted stages. No replacement
family is authorized here.

Keep all menu/action/shortcut identity checks, article and word zoom assertions,
menu/toolbutton visibility, dictionary styles/icon sizes, successful Preferences
reapplication, rejected-save preservation, always-on-top, search placement,
exact signal/callback counts and layout restoration. The test uses real
MainWindow/ArticleView/Qt actions and existing Core/Network composition, P1 path
initialization, and W3 presentation target closure. No production algorithm copy.

One new test-target-only access operation may return an ApplicationPreferences
value snapshot: failed-save preservation and zoom/placement state are not fully
observable from widgets alone. It returns no mutable reference and provides no
setter, lifecycle bypass or general window access. Other objects are discovered by
existing QObject names; callback replacement uses SetPreferencesApplyCallback.
The state-version assertion is checked against actual saveState(7) output rather
than copying the private implementation constant into the test.

HelpMenu and ProductShell remain mode B pending: help adds modal/resource/URL
substitution, shell adds translated geometry/layout fixtures and capture output.
They are not folded into this batch merely because they are menus/presentation.
Other B/C families retain their recorded dependencies; W4/W5/W6 remain unselected.
P1 paths/profile lifetime and W1/W2 business behavior are unchanged. No new writable
path is required: owned config/index/Network/WebEngine/temp roots cover this family.
Some assigned Preferences callbacks are test substitutes, but the original smoke
restores the real main-owned callback before menu/toolbar actions. Those stages
depend on real configuration transactions. Ordinary startup remains a separate
real OFF-product regression using the accepted P1 portable layout.

### W3.3 verified disposition: blocked before extraction

ViewMenuSmoke is now **mode C: production Preferences application composition**.
`main_window.cpp:2887` restores the callback installed in `main.cpp:1582`; the
subsequent menubar/name/icon operations execute that callback through
`ApplyDisplayPreferences`. Its Network preparation, persistence request, facade
preparation, coordinator execution and main-owned state updates are not exposed
by the existing W3 presentation target, which excludes main.cpp. A Preferences
snapshot does not provide this behavior. Reimplementing the callback in a test or
replacing it with success would not preserve the original production path.

The unchanged original scenario passes with an isolated persisted configuration;
fresh-profile failures and the seeded success are retained in w3-3-status.md and
external evidence. No product defect repair is inferred from those fixture results.
The locked batch is stopped without a replacement family. No methods, fields,
dispatches or targets were migrated. The two previously migrated families remain
migrated; 51 MainWindow Run*Check definitions remain. All other pending families
retain their existing classifications. A separate bounded decision about reuse of
the production Preferences application boundary is needed before this family can
proceed. W3.3 is not closed; W3/A4 remains in progress.

### W3.3 authorized continuation outcome

The subsequent approval explicitly allowed a source-private shared production
Preferences application boundary. Checkpoint A extracted that implementation with
unchanged ownership and passed the original scene plus affected regressions before
checkpoint B. ViewMenuSmoke now runs in view_menu_test using that same production
installer and transaction code, with original callback-substitution phases retained.
The earlier blocked assessment above remains historical, not the current outcome.

Migrated families: FullTextDictionaryProjectionSmoke (W3.1), DictionaryBarSmoke
(W3.2), ViewMenuSmoke (W3.3 implementation). Original ViewMenu method, declaration
and smoke dispatch are removed; no replacement family was selected. Remaining
MainWindow Run*Check definitions: 50. Other B/C entries remain pending with their
recorded access/startup/persistence/asynchronous dependencies. The new Preferences
boundary does not automatically approve or migrate them. W3/A4 remains in progress;
W3.3 closure depends on its external final independent receipt. No W4/W5/W6 work.

## W3.4 dependency preflight and locked batch

Base 828d1f3a9c07a9207a1796a42cdfb0ac26ab7713 includes the accepted W3.3
ViewMenu delivery and shared private preferences_application. Its independent
receipt remains external; W3.3 is closed within the approved Windows/Qt boundary.

ArticlesPreferencesSmoke moves from C (unavailable Preferences composition) to B
(limited existing dialog-interaction seam). The whole original scene and main's
persisted-configuration assertions can reuse the real shared installer unchanged.
Old entry passed on a fresh serialized owned fixture before scope lock. Exactly
this one family is selected for W3.4; see w3-4-status.md for lifecycle, mapping and
evidence. Migration status is pending until equivalent verification completes.

HistoryPreferencesSmoke remains C: history import and lookup-recording callbacks
are still assembled in main. FavoritesPreferencesSmoke remains C: favorite
mutation/persistence callbacks are still main-owned. Shared Preferences resolves
only part of those dependencies. Neither is selected; no new assembly is approved.
Other families retain their recorded status; this is not a new full audit.

### W3.4 implementation outcome

ArticlesPreferencesSmoke is migrated to articles_preferences_test; original method,
declaration and product dispatch are removed. Four families are now migrated:
FullTextDictionaryProjectionSmoke, DictionaryBarSmoke, ViewMenuSmoke and
ArticlesPreferencesSmoke. Remaining MainWindow Run*Check count is 49, not an
acceptance metric. All other families remain pending, including the concrete
History/Favorites assembly blockers above. W3/A4 stays in progress. W3.4's final
acceptance is bound to its external independent candidate receipt.

## W3.5 preflight queue and concrete blocked boundaries

W3.4 is accepted at b6f9429d (external independent Pass retained). W3.5 locks
DictionaryContextPreferencesSmoke followed by SynonymPreferencesSmoke after two
separate fresh-fixture old-entry passes. Both now qualify as mode A: directly reuse
W3.4 dialog executor and copy-only observer plus W3.3 production Preferences
installer. No additional access is needed. Their common cancel/error/real-save
contract, persisted checks and independent processes are mapped in w3-5-status.md.
No third or replacement family is selected. Other uninspected entries retain their
previous classification; availability is not inferred from a similar name.

### HistoryPreferencesSmoke: remaining dependency, separately decidable

At base b6f9429d, main.cpp:656 owns the loaded history vector, alongside the
configuration/path locals. MainWindow::RunHistoryPreferencesSmokeCheck edits
store_history/maximum_history_entries through the real Preferences callback,
emits LookupSubmitted for "Not recorded" and "Recorded", and emits
ImportHistoryRequested. The missing production paths are main.cpp:987-1018
(LookupSubmitted -> store/max guard -> case-insensitive dedup/newest-first trim ->
Core SaveHistory -> assign same history -> refresh_history) and 1053-1069
(ImportHistoryRequested -> Core ImportHistoryText with current max/group ->
SaveHistory -> assign -> refresh_history). refresh_history at 943 maps entries to
SetHistoryItems. The old dispatch at 1678+ seeds three entries/import file and
checks persisted history and configuration; these are test-owned steps to migrate,
not production logic to place in a shared module.

These QObject connections use window as receiver context, execute synchronously
on its GUI thread and capture main-owned references. Destruction disconnects via
window context; setters/public signals do not expose the main lambda bodies.
InstallPreferencesApplication already handles bounded-history updates when applying
preferences, but does not install lookup recording or import handlers. Copying
those handlers into a runner would duplicate the missing business implementation.

A possible separately approved minimal boundary is a private desktop history
binding installer sharing exactly recording/import implementations and presentation
refresh, with references to configuration/history, selected history path and
MainWindow receiver. Main retains ownership; installer returns connection handles
if explicit teardown is required. It need not include Favorites, history exports,
clear, menus or lookup execution ownership. The accepted architecture.md
General/History contract explicitly leaves recording/import/trim/persistence in
the composition root, so a source-private extraction can preserve that contract;
moving policy to MainWindow/Core or changing persistence semantics would require
an explicit design change. Neither version is implemented or approved by W3.5.

### FavoritesPreferencesSmoke: distinct remaining dependency

At base, main.cpp:657 owns Favorites. RunFavoritesPreferencesSmokeCheck uses
AddFavoriteFolderRequested, add_favorite_action_ -> AddFavoriteRequested, and
remove_favorite_action_ -> Widgets confirmation -> RemoveFavoriteRequested.
main.cpp:1106-1129 adds folders through FavoriteContainerAtPath; 1071-1104 adds
headwords with case-insensitive duplicate suppression; 1020-1038 removes through
RemoveFavoriteAtPath. Each copies the existing tree, uses Core SaveFavorites,
assigns the same owned vector only on success and calls refresh_favorites
(975 -> MakeFavoriteViewItem at 304 -> SetFavoriteItems). Invalid paths refresh;
exceptions show the existing warning. Container traversal also preserves expanded
ancestors. The scene's rejection/acceptance confirmation substitute belongs in the
test; it is distinct from persistence and must not become a save-success substitute.

The handlers borrow main-owned favorites/path/window and run synchronously on the
window's GUI context. Missing entry is an installer for those exact add-folder,
add-headword/remove handlers and tree projection/helpers, not Preferences itself.
A separate source-private favorites binding extraction could retain main ownership
and current atomic-save semantics with explicit references to favorites/path/window.
It does not require History, rename/move/transfer, a generic application context,
or a new facade owner. Shared helpers also serve unmigrated rename/move/transfer
handlers; any future proposal must preserve those consumers. Accepted
architecture.md General/Favorites requires Widgets confirmation and immediate
atomic mutation persistence; no delayed-save or ownership redesign is authorized.
This boundary remains unimplemented and needs separate production-extraction approval.

### Retained interaction storage and exact consumers

After the two locked methods are removed, preferences_dialog_executor_ remains in
production for RunHelpMenuSmokeCheck, RunEditMenuSmokeCheck,
RunHistoryPreferencesSmokeCheck, RunPreferencesCoordinatorPredecisionSmokeCheck,
RunFavoritesPreferencesSmokeCheck, RunOptionalPartsPreferencesSmokeCheck,
RunProxyPreferencesSmokeCheck, RunNetworkCachePreferencesSmokeCheck,
RunNetworkCachePreferencesRestartSmokeCheck, RunHideSingleTabPreferencesSmokeCheck,
RunEscapeHidesMainWindowPreferencesSmokeCheck, RunArticleClickPreferencesSmokeCheck
and RunMruTabOrderPreferencesSmokeCheck; EditPreferences invokes it. Migrated
Articles, DictionaryContext and Synonym runners reuse the test-only setter.
ViewMenu/Articles and these new runners share the copy-only Preferences observer.
No last-consumer cleanup or widened friend is part of this batch.

### W3.5 implementation queue delta

Both locked families are now migrated and pass their individual equivalence gates.
Total migrated families: FullTextDictionaryProjectionSmoke, DictionaryBarSmoke,
ViewMenuSmoke, ArticlesPreferencesSmoke, DictionaryContextPreferencesSmoke and
SynonymPreferencesSmoke. Remaining MainWindow Run*Check definitions: 47 (navigation
only). Other pending families retain their prior classifications and consumers;
HistoryPreferences and FavoritesPreferences remain separately blocked as detailed
above. No additional family was selected. Final W3.5 acceptance requires its
external exact-candidate review; broader W3/A4 remains in progress.

W3.5 review correction: the first candidate's DictionaryContext migration omitted
its existing two-process restart wrapper. Independent Fail is preserved. The corrected
entry reuses dictionary_context_preferences_restart.cmake with the test runner,
shared fresh profile, two actual processes and original 40-second timeout. The
second loads the first's persisted setting/session and preserves real no-op behavior.
No ownership or production assembly change is needed; both locked families remain
selected. Initial single-pass claims are superseded, not treated as acceptance.

## W3.6 History Preferences (base 041f8625)

Locked only HistoryPreferencesSmoke after a fresh original-entry baseline. The
user separately approved minimal private history_application recording/import/
projection assembly. Main retains configuration/history/window ownership. Shared
Preferences remains unchanged. Checkpoint A retains the old test and passes its
full outer entry plus affected consumers; see w3-6-status.md and external evidence.

The original generic row's cancel/failure/export wording is superseded for this
family: actual coverage is real Preferences false/2, trim/preserve, disabled
recording, bounded replacement import, real Preferences true/1, record and
persistent history/configuration assertions. It is one process (20 seconds), not a
restart family. The two-process DictionaryContext wrapper remains unchanged.

HistoryPreferences moves to history_preferences_test with existing executor and
copy-only Preferences access, named widget observation and the original signals.
No new TestAccess or mutable-history backdoor. Its original MainWindow method,
declaration, main fixture/assertion scheduling and smoke option are removed.

The shared executor remains consumed by HelpMenu, EditMenu,
PreferencesCoordinatorPredecision, FavoritesPreferences, OptionalPartsPreferences,
ProxyPreferences, NetworkCachePreferences and its restart, HideSingleTabPreferences,
EscapeHidesMainWindowPreferences, ArticleClickPreferences and MruTabOrderPreferences,
as well as actual EditPreferences. Previously migrated families still use the
finite setter; the production field is not a last-consumer cleanup opportunity.
Main's refresh_history callback remains needed by ClearHistoryRequested and shared
Preferences, now delegates to the same production projection. Export/clear and
all other History scenarios stay at their previous entries.

Favorites remains independently blocked by the add-folder/add-headword/remove
installation and tree projection/traversal boundary recorded under W3.5; none of
that logic is extracted here. HistoryMenu and broader history presentation rows
remain pending; partial availability of recording/import does not prove their
remaining export/clear/menu/quit assembly coverage. No next family is selected.

W3.6 implementation delta: HistoryPreferences joins the six previously migrated families (seven total). Other History families remain pending; Favorites remains separate and blocked by its own assembly. W3/A4 remains in progress. Full acceptance is bound by the external final W3.6 review receipt.
