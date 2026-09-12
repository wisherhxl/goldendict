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
