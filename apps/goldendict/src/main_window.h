// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
#define GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <QFont>
#include <QList>
#include <QMainWindow>
#include <QPointF>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include "goldendict/core/application.h"
#include "goldendict/core/desktop_facade.h"

class ArticlePage;
class ArticleView;
class AudioPlaybackService;
enum class ArticleHighlightNavigationDirection;
class ArticleSchemeHandler;
enum class ArticleLinkDisposition;
class DictionaryBrowser;

namespace goldendict::app {
class FullTextQueryComposer;
class FullTextSearchDialog;
class HelpWindow;
enum class HelpIntent;
struct FullTextResultActivationIntent;
}  // namespace goldendict::app
class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QPoint;
class QComboBox;
class QEvent;
class QKeyEvent;
class QTabWidget;
class QTimer;
class QToolBar;
class QToolButton;
class QUrl;
class QWebEngineView;
class QWebEnginePage;
class QPrinter;
class QShortcut;
class FavoritesTreeWidget;
class SuggestionWorker;
class SourceDirectoriesDialog;
class PreferencesDialog;
class RenderedTextMatchPlanController;
class MainWindow;
struct WidgetsFacadePreparationRecord;
struct WidgetsFacadePreparationResources;
class WidgetsFacadeActivationRelay;
class WidgetsPresentationHost;
class DictionaryBarPresentationHost;

namespace goldendict::widgets {
class WidgetsFacadeBindingRegistry;
}

namespace goldendict::core {
class DesktopFacade;
class LookupRequest;
}  // namespace goldendict::core

struct FavoriteViewItem {
    QString text;
    bool folder = false;
    bool expanded = false;
    QList<int> path;
    std::vector<FavoriteViewItem> children;
};

struct HistoryViewItem {
    QString word;
    std::uint32_t group_id = 0U;
};

class PreparedWidgetsFacadeCandidate final {
   public:
    PreparedWidgetsFacadeCandidate() noexcept;
    ~PreparedWidgetsFacadeCandidate();
    PreparedWidgetsFacadeCandidate(const PreparedWidgetsFacadeCandidate&) =
        delete;
    PreparedWidgetsFacadeCandidate& operator=(
        const PreparedWidgetsFacadeCandidate&) = delete;
    PreparedWidgetsFacadeCandidate(PreparedWidgetsFacadeCandidate&&) noexcept;
    PreparedWidgetsFacadeCandidate& operator=(
        PreparedWidgetsFacadeCandidate&&) noexcept;

    explicit operator bool() const noexcept;
    void Abandon() noexcept;

   private:
    explicit PreparedWidgetsFacadeCandidate(
        std::shared_ptr<WidgetsFacadePreparationRecord> record) noexcept;
    std::shared_ptr<WidgetsFacadePreparationRecord> record_;
    friend class MainWindow;
};

enum class WidgetsCommitOutcome : std::uint8_t {
    kRejectedBeforePublication,
    kMaintainedAbortable,
    kPublished,
    kPublishedWithCleanupFailure,
};

class MaintainedWidgetsCommit final {
   public:
    MaintainedWidgetsCommit() noexcept = default;
    ~MaintainedWidgetsCommit();
    MaintainedWidgetsCommit(const MaintainedWidgetsCommit&) = delete;
    MaintainedWidgetsCommit& operator=(const MaintainedWidgetsCommit&) = delete;
    MaintainedWidgetsCommit(MaintainedWidgetsCommit&& other) noexcept;
    MaintainedWidgetsCommit& operator=(
        MaintainedWidgetsCommit&& other) noexcept;

    explicit operator bool() const noexcept { return owner_ != nullptr; }

   private:
    MainWindow* owner_ = nullptr;
    WidgetsFacadePreparationRecord* record_ = nullptr;
    std::uint64_t generation_ = 0U;
    friend class MainWindow;
};

class PublishedWidgetsCommit final {
   public:
    PublishedWidgetsCommit() noexcept = default;
    ~PublishedWidgetsCommit();
    PublishedWidgetsCommit(const PublishedWidgetsCommit&) = delete;
    PublishedWidgetsCommit& operator=(const PublishedWidgetsCommit&) = delete;
    PublishedWidgetsCommit(PublishedWidgetsCommit&& other) noexcept;
    PublishedWidgetsCommit& operator=(PublishedWidgetsCommit&& other) noexcept;

    explicit operator bool() const noexcept { return owner_ != nullptr; }

   private:
    MainWindow* owner_ = nullptr;
    WidgetsFacadePreparationRecord* record_ = nullptr;
    std::uint64_t generation_ = 0U;
    friend class MainWindow;
};

struct BeginWidgetsMaintenanceResult final {
    WidgetsCommitOutcome outcome =
        WidgetsCommitOutcome::kRejectedBeforePublication;
    MaintainedWidgetsCommit maintained;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT
    friend class PreparedWidgetsFacadeCandidate;
    friend class WidgetsFacadeActivationRelay;
    friend class MaintainedWidgetsCommit;
    friend class PublishedWidgetsCommit;

   public:
    using ForvoCredentialMap = std::map<std::string, std::string>;
    using SourceApplyCallback = std::function<QString(
        const std::vector<std::string>&,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&,
        const std::vector<goldendict::core::MediaWikiSourceConfiguration>&,
        const std::vector<goldendict::core::WebsiteSourceConfiguration>&,
        const std::vector<goldendict::core::ForvoSourceConfiguration>&,
        const ForvoCredentialMap&,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&,
        const std::vector<
            goldendict::core::ExternalProgramSourceConfiguration>&)>;
    using HistoryExportCallback = std::function<QString(const QString&)>;
    using PreferencesApplyCallback =
        std::function<QString(const goldendict::core::ApplicationPreferences&)>;
    explicit MainWindow(const QString& configuration_directory,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    void SetFacade(goldendict::core::DesktopFacade* facade);
    PreparedWidgetsFacadeCandidate PrepareFacadeCandidate(
        std::shared_ptr<goldendict::core::DesktopFacade> facade,
        const goldendict::core::ApplicationPreferences& preferences,
        const std::vector<goldendict::core::DictionaryGroupConfiguration>&
            groups);
    bool IsFacadeCandidateCurrent(
        const PreparedWidgetsFacadeCandidate& candidate) const noexcept;
    BeginWidgetsMaintenanceResult BeginFacadeCandidateMaintenance(
        PreparedWidgetsFacadeCandidate& candidate);
    void AbortMaintainedFacadeCommit(
        MaintainedWidgetsCommit&& maintained) noexcept;
    PublishedWidgetsCommit PublishMaintainedFacadeCommit(
        MaintainedWidgetsCommit&& maintained) noexcept;
    WidgetsCommitOutcome FinishPublishedFacadeCommit(
        PublishedWidgetsCommit&& published) noexcept;
    void SetPreferences(
        const goldendict::core::ApplicationPreferences& preferences);
    void SetPreferencesApplyCallback(PreferencesApplyCallback apply_callback);
    void SetNetworkCacheDirectory(const QString& directory);
    bool RestoreMainWindowGeometry(const std::string& geometry);
    std::string CaptureMainWindowGeometry() const;
    void SetFullTextDialogGeometry(std::string geometry);
    bool RestoreMainWindowState(const std::string& state);
    std::string CaptureMainWindowState() const;
    void SetHistoryWords(const QStringList& words);
    void SetHistoryItems(const std::vector<HistoryViewItem>& items);
    void SetFavoriteItems(const std::vector<FavoriteViewItem>& items,
                          const QList<int>& current_path = {});
    void SetDictionaryGroups(
        const std::vector<goldendict::core::DictionaryGroupConfiguration>&
            groups);
    void SetSourceDirectories(
        const std::vector<std::string>& dictionary_paths,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&
            sound_directories);
    void ActivateFromSingleInstanceLookup();
    void SubmitInitialLookup(const QString& word);
    void SetOnlineSources(
        const std::vector<goldendict::core::MediaWikiSourceConfiguration>&
            mediawiki_sources,
        const std::vector<goldendict::core::WebsiteSourceConfiguration>&
            website_sources,
        const std::vector<goldendict::core::ForvoSourceConfiguration>&
            forvo_sources,
        const ForvoCredentialMap& forvo_credentials,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&
            dict_server_sources,
        const std::vector<goldendict::core::ExternalProgramSourceConfiguration>&
            external_program_sources,
        SourceApplyCallback apply_callback);
    void SetHistoryExportCallback(HistoryExportCallback callback);
    const std::vector<goldendict::core::DictionaryGroupConfiguration>&
    DictionaryGroups() const noexcept;
    void RunWebEngineSmokeCheck(std::function<void(bool)> completion);
    void RunWebEngineInteractionCheck(std::function<void(bool)> completion);
    void RunArticleContextMenuCheck(std::function<void(bool)> completion);
    void RunDictionaryContextNavigationCheck(
        std::function<void(bool)> completion);
    void RunDictionaryContextPreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunSystemPrintCheck(std::function<void(bool)> completion);
    void RunHistorySmokeCheck(std::function<void(bool)> completion);
    void RunHistoryManagementSmokeCheck(std::function<void(bool)> completion);
    void RunHistoryExportSmokeCheck(const QString& path,
                                    std::function<void(bool)> completion);
    void RunHistoryImportSmokeCheck(const QString& path,
                                    std::function<void(bool)> completion);
    void RunHistoryPreferencesSmokeCheck(const QString& import_path,
                                         std::function<void(bool)> completion);
    void RunPreferencesCoordinatorPredecisionSmokeCheck(
        std::function<void(bool)> completion);
    void RunFavoritesPreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunArticlesPreferencesSmokeCheck(std::function<void(bool)> completion);
    void RunSynonymPreferencesSmokeCheck(std::function<void(bool)> completion);
    void RunOptionalPartsPreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunProxyPreferencesSmokeCheck(std::function<void(bool)> completion);
    void RunProxyPreferencesRestartSmokeCheck(
        std::function<void(bool)> completion);
    void RunNetworkCachePreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunNetworkCachePreferencesRestartSmokeCheck(
        std::function<void(bool)> completion);
    void RunHideSingleTabPreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunHideSingleTabRestartSmokeCheck(
        std::function<void(bool)> completion);
    void RunMruTabOrderPreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunMruTabOrderRestartSmokeCheck(std::function<void(bool)> completion);
    void RunEscapeHidesMainWindowPreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunEscapeHidesMainWindowRestartSmokeCheck(
        std::function<void(bool)> completion);
    void RunArticleClickPreferencesSmokeCheck(
        std::function<void(bool)> completion);
    void RunArticleClickRestartSmokeCheck(std::function<void(bool)> completion);
    void RunFavoritesSmokeCheck(std::function<void(bool)> completion);
    void RunFavoritesCrossFolderMoveSmokeCheck(
        std::function<void(bool)> completion);
    void RunFavoritesTransferSmokeCheck(const QString& path,
                                        std::function<void(bool)> completion);
    void RunDictionaryBrowserSmokeCheck(std::function<void(bool)> completion);
    void RunDictionaryBrowserExportSmokeCheck(
        const QString& path, std::function<void(bool)> completion);
    void RunDictionaryGroupsSmokeCheck(
        std::function<void(std::size_t)> before_attempt,
        std::function<void(bool)> completion);
    void RunSourceDirectoriesSmokeCheck(std::function<void(bool)> completion);
    void RunArticleTabsSmokeCheck(std::function<void(bool)> completion);
    void RunSuggestionPaneSmokeCheck(std::function<void(bool)> completion);
    void RunDictionaryBarSmokeCheck(std::function<void(bool)> completion);
    void RunWidgetsFacadePreparationSmokeCheck(
        std::function<void(bool)> completion);
    void RunFullTextDictionaryProjectionSmokeCheck(
        std::function<void(bool)> completion);
    void RunFullTextDialogSmokeCheck(std::function<void(bool)> completion);
    void RunProductShellSmokeCheck(std::function<void(bool)> completion);
    void RunViewMenuSmokeCheck(std::function<void(bool)> completion);
    void RunHistoryMenuSmokeCheck(const QString& path,
                                  std::function<void(bool)> completion);
    void RunFavoritesMenuSmokeCheck(const QString& path,
                                    std::function<void(bool)> completion);
    void RunFileMenuSmokeCheck(const QString& path,
                               std::function<void(bool)> completion);
    void RunEditMenuSmokeCheck(std::function<void(bool)> completion);
    void RunSearchMenuSmokeCheck(std::function<void(bool)> completion);
    void RunHelpMenuSmokeCheck(const QString& help_directory,
                               std::function<void(bool)> completion);
    void RunArticleTabSessionRestartSmokeCheck(
        bool prepare, std::function<void(bool)> completion);

   signals:
    void SourceDirectoriesEdited(
        const std::vector<std::string>& dictionary_paths,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&
            sound_directories);
    void LookupSubmitted(const QString& word, std::uint32_t group_id);
    void AddFavoriteRequested(const QString& word,
                              const QList<int>& parent_path);
    void AddFavoriteFolderRequested(const QString& name,
                                    const QList<int>& parent_path);
    void RenameFavoriteRequested(const QList<int>& path, const QString& name);
    void MoveFavoriteRequested(const QList<int>& path, int offset);
    void MoveFavoriteToRootRequested(const QList<int>& path);
    void MoveFavoriteAcrossFoldersRequested(
        const QList<int>& source_path, const QList<int>& destination_path,
        int destination_index, const QList<QList<int>>& expanded_paths);
    void ImportFavoritesRequested(const QString& path);
    void ExportFavoritesRequested(const QString& path);
    void ExportFavoritesListRequested(const QString& path);
    void RemoveFavoriteRequested(const QList<int>& path);
    void ClearHistoryRequested();
    void ImportHistoryRequested(const QString& path, std::uint32_t group_id);
    void DictionaryGroupsEdited();
    void ArticleTabSessionMutated();
    void FullTextDialogGeometryCaptured(std::string geometry);

   private slots:
    void EditSourceDirectories();
    void StartLookup();
    void FinishLookup();
    void FindInArticle(bool backwards = false);
    void PrintArticle();
    void PreviewArticle();
    void SaveArticleAsPdf();
    void SaveArticle();
    void PageSetup();
    void RescanFiles();
    void ExportFavoritesToList();
    bool ApplyDisplayPreferences(
        const goldendict::core::ApplicationPreferences& preferences);
    void ApplyArticleZoom();
    void ApplyWordsZoom();
    void SetAlwaysOnTop(bool enabled);
    void UpdateNavigationActions();
    void RefreshPronounceAvailability();
    void ZoomArticle(double delta);
    void ShowDictionaryBrowser();
    void ExportHistory();
    void ImportHistory();
    void ClearHistory();
    void CreateFavoriteFolder();
    void RenameFavorite();
    void ImportFavorites();
    void ExportFavorites();
    void EditDictionaryGroups();
    void EditPreferences();

   private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
#if defined(Q_OS_LINUX)
    bool HandleLinuxPrimarySelectionMousePress(QMouseEvent* event,
                                               const QString& selection);
#endif
    void StartLookupInTab(goldendict::core::TabOpenPolicy open_policy,
                          goldendict::core::TabActivationPolicy activation,
                          const QString& internal_url = {},
                          std::optional<std::uint32_t> group_id = std::nullopt);
    void StartNavigationLookup(
        goldendict::core::ArticleTabId tab_id,
        const goldendict::core::TabNavigationState& navigation,
        bool record_history);
    void SyncArticleTabs();
    void RebuildArticleTabs();
    void ActivateArticleTab(int index);
    void CloseArticleTab(int index);
    void CloseOtherArticleTabs(int index);
    void CreateEmptyArticleTab(bool activate);
    void ReconcileMruTabIds(bool rebuild = false);
    void TraverseArticleTabs(bool forward);
    void FinishMruTraversal();
    goldendict::core::TabPlacementPolicy NewTabPlacementPolicy() const;
    void NavigateArticleTab(bool forward);
    void OpenArticleLink(goldendict::core::ArticleTabId tab_id, const QUrl& url,
                         ArticleLinkDisposition disposition);
    void OpenInternalHelpLink(goldendict::core::ArticleTabId tab_id,
                              const QUrl& url,
                              ArticleLinkDisposition disposition);
    void LookupArticleSelection(goldendict::core::ArticleTabId tab_id,
                                const QString& text,
                                ArticleLinkDisposition disposition);
    void StartPrinterRender(ArticleView* view, QPrinter* printer);
    void FinishPrinterRender(ArticleView* view, bool success);
    void StartPdfExport(const QString& path);
    void FinishPdfExport(std::uint64_t request_generation,
                         const QByteArray& pdf_data);
    void InvalidateArticleOutputOwnership(goldendict::core::ArticleTabId tab_id,
                                          ArticleView* view);
    void FinishArticleSave(goldendict::core::ArticleTabId tab_id,
                           const QPointer<ArticleView>& view,
                           const QPointer<QWebEnginePage>& page,
                           std::uint64_t navigation_generation,
                           const QString& path, const QString& html);
    void UpdateFileActions();
    void ShowTabContextMenu(const QPoint& position);
    ArticleView* CreateArticleView(goldendict::core::ArticleTabId tab_id);
    void ReloadCurrentArticle();
    void StartPendingArticleReload(goldendict::core::ArticleTabId tab_id,
                                   ArticleView* view);
    void HandleArticleReloadStarted(goldendict::core::ArticleTabId tab_id,
                                    ArticleView* view);
    void HandleArticleLoadFinished(goldendict::core::ArticleTabId tab_id,
                                   ArticleView* view, bool success);
    void HandleArticleHtmlNavigationFinished(
        goldendict::core::ArticleTabId tab_id, ArticleView* view,
        quint64 navigation_token, bool success);
    void BindPendingArticleScrollRestoration(
        goldendict::core::ArticleTabId tab_id, ArticleView* view,
        quint64 navigation_token);
    void DeferPendingArticleScrollRestoration(
        goldendict::core::ArticleTabId tab_id, ArticleView* view);
    std::optional<QPointF> TakePendingArticleScrollRestoration(
        goldendict::core::ArticleTabId tab_id, ArticleView* view,
        quint64 navigation_token, bool success);
    void PublishArticleHtml(goldendict::core::ArticleTabId tab_id,
                            ArticleView* view, const QString& html,
                            const QUrl& base_url = QUrl());
    ArticleView* ArticleViewForTab(goldendict::core::ArticleTabId tab_id) const;
    goldendict::core::ArticleTabId TabIdAt(int index) const;
    void ShowMessage(const QString& title, const QString& message);
    void RefreshHistoryList();
    void UpdateHistoryActions();
    void UpdateFavoritesActions();
    void SelectGroup(std::uint32_t group_id);
    void RefreshGroupSelector();
    void RefreshDictionaryBar();
    void ApplyPreparedDictionaryAction(const std::string& dictionary_id,
                                       bool checked,
                                       Qt::KeyboardModifiers modifiers);
    void ShowFullTextSearch();
#if defined(Q_OS_LINUX)
    void ShowHelp(goldendict::app::HelpIntent intent);
#endif
    void NavigateToFullTextResult(
        goldendict::app::FullTextResultActivationIntent intent);
    goldendict::core::FullTextQuery ComposeFullTextQuery(
        const goldendict::app::FullTextQueryComposer& composer) const;
    void ApplyDictionaryParticipation();
    std::vector<std::string> ParticipatingDictionaryIds(
        std::uint32_t group_id) const;
    void ApplyDictionaryFilter(goldendict::core::LookupQuery* query) const;
    void ApplyNavigationDictionaryFilter(
        goldendict::core::LookupQuery* query,
        const goldendict::core::TabNavigationState& navigation) const;
    void ApplyDictionaryFilter(goldendict::core::SuggestionQuery* query) const;
    void RefreshResultsNavigation();
    void RefreshSuggestions();
    void DispatchArticleSearch(goldendict::core::ArticleTabId tab_id,
                               ArticleView* view, const QString& text,
                               std::uint64_t generation, bool backwards);
    void FinishArticleSearch(goldendict::core::ArticleTabId tab_id,
                             const QPointer<ArticleView>& view,
                             const QPointer<QWebEnginePage>& page,
                             std::uint64_t navigation_generation,
                             const QString& text,
                             std::uint64_t search_generation, int active_match,
                             int number_of_matches);
    void RunArticleSearchReloadCheck(goldendict::core::ArticleTabId tab_id,
                                     bool passed,
                                     std::function<void(bool)> completion);
    void ExtractRenderedPageText(goldendict::core::ArticleTabId tab_id,
                                 ArticleView* view,
                                 std::uint64_t accepted_query_generation,
                                 std::uint64_t lookup_generation,
                                 std::uint64_t search_generation,
                                 std::uint64_t navigation_generation);
    void SubmitRenderedTextMatchPlan(goldendict::core::ArticleTabId tab_id);
    void FinishRenderedTextMatchPlan(
        std::uint64_t generation,
        goldendict::core::RenderedTextMatchPlanResult result);
    void InvalidateRenderedTextMatchPlan(
        std::optional<goldendict::core::ArticleTabId> tab_id = std::nullopt);
    void NavigateFullTextHighlight(
        goldendict::core::ArticleTabId tab_id,
        ArticleHighlightNavigationDirection direction);
    void RefreshArticleSearch();
    void StartSuggestionLookup();
    void FinishSuggestionLookup(goldendict::core::ArticleTabId tab_id,
                                std::uint64_t generation,
                                goldendict::core::SuggestionResponse response);
    void ActivateSuggestion();
    void StopSuggestionWorker();
    void NavigateToSelectedResult();
    void NavigateToArticleResult(ArticleView* view, int result_index);
    void RefreshDictionaryContext(goldendict::core::ArticleTabId tab_id);
    void StoreLookupResults(goldendict::core::ArticleTabId tab_id,
                            const goldendict::core::LookupResponse& response);
    void ShowDictionaryResultsPane(goldendict::core::ArticleTabId tab_id,
                                   ArticleView* view, std::uint64_t generation);
    QList<int> SelectedFavoriteFolderPath() const;
    bool ConfirmFavoriteRemoval();
    QList<QList<int>> ExpandedFavoriteFolderPaths() const;
    void ApplyDefaultPaneLayout();
    bool HasUsableMainWindowLayout() const;
    bool DispatchSafeExternalUrl(const QUrl& url);
    void ShowAboutDialog();
    void AdvancePresentationMutationEpoch() noexcept;
    void ReclaimAbandonedFacadeCandidates();
    void ReclaimFacadeCandidate(bool force);
    void AbortMaintainedFacadeCommitInternal() noexcept;
    WidgetsCommitOutcome FinishPublishedFacadeCommitInternal() noexcept;
    bool WidgetsInteractionBlocked() const noexcept;

    goldendict::core::DesktopFacade* facade_ = nullptr;
    std::unique_ptr<AudioPlaybackService> audio_playback_service_;
    std::uint64_t facade_preparation_generation_ = 1U;
    std::uint64_t presentation_mutation_epoch_ = 1U;
    bool facade_preparation_shutdown_ = false;
    std::shared_ptr<WidgetsFacadePreparationRecord> facade_preparation_record_;
    std::shared_ptr<WidgetsFacadePreparationRecord> maintained_facade_record_;
    std::unique_ptr<WidgetsFacadePreparationResources> active_facade_resources_;
    std::unique_ptr<WidgetsFacadePreparationResources>
        retired_facade_resources_;
    std::atomic_bool widgets_interaction_gate_closed_ = false;
    bool widgets_maintenance_active_ = false;
    bool widgets_publication_decided_ = false;
    bool widgets_cleanup_failure_injected_ = false;
    QTimer* facade_candidate_reclaimer_ = nullptr;
    QTimer* facade_binding_reclaimer_ = nullptr;
    int facade_preparation_failure_step_ = -1;
    int facade_maintenance_failure_step_ = -1;
    bool facade_final_validation_failure_ = false;
    std::unique_ptr<goldendict::widgets::WidgetsFacadeBindingRegistry>
        facade_binding_registry_;
    WidgetsPresentationHost* group_selector_host_ = nullptr;
    DictionaryBarPresentationHost* dictionary_bar_host_ = nullptr;
    WidgetsPresentationHost* article_tabs_host_ = nullptr;
    std::vector<std::string> dictionary_paths_;
    std::vector<goldendict::core::SoundDirectoryConfiguration>
        sound_directories_;
    std::vector<goldendict::core::MediaWikiSourceConfiguration>
        mediawiki_sources_;
    std::vector<goldendict::core::WebsiteSourceConfiguration> website_sources_;
    std::vector<goldendict::core::ForvoSourceConfiguration> forvo_sources_;
    ForvoCredentialMap forvo_credentials_;
    std::vector<goldendict::core::DictServerSourceConfiguration>
        dict_server_sources_;
    std::vector<goldendict::core::ExternalProgramSourceConfiguration>
        external_program_sources_;
    SourceApplyCallback source_apply_callback_;
    std::function<int(SourceDirectoriesDialog&)> source_dialog_executor_;
    bool source_configuration_busy_ = false;
    goldendict::core::ApplicationPreferences preferences_;
    PreferencesApplyCallback preferences_apply_callback_;
    QString network_cache_directory_;
    std::function<int(PreferencesDialog&)> preferences_dialog_executor_;
    bool preferences_busy_ = false;
    QPushButton* dictionary_sources_button_ = nullptr;
    QLineEdit* query_ = nullptr;
    QComboBox* group_selector_ = nullptr;
    QComboBox* dock_group_selector_ = nullptr;
    QFont query_default_font_;
    QFont group_selector_default_font_;
    QFont dock_group_selector_default_font_;
    QPushButton* edit_groups_button_ = nullptr;
    std::vector<goldendict::core::DictionaryGroupConfiguration> groups_;
    std::vector<QShortcut*> group_shortcuts_;
    QLineEdit* history_filter_ = nullptr;
    QListWidget* history_list_ = nullptr;
    std::vector<HistoryViewItem> history_items_;
    std::uint32_t selected_group_id_ = 0U;
    QPushButton* clear_history_button_ = nullptr;
    QPushButton* export_history_button_ = nullptr;
    QPushButton* import_history_button_ = nullptr;
    QAction* clear_history_action_ = nullptr;
    QAction* export_history_action_ = nullptr;
    QAction* import_history_action_ = nullptr;
    HistoryExportCallback history_export_callback_;
    std::function<QString()> history_export_path_provider_;
    std::function<QString()> history_import_path_provider_;
    bool history_command_busy_ = false;
    FavoritesTreeWidget* favorites_tree_ = nullptr;
    QListWidget* results_list_ = nullptr;
    QListWidget* suggestions_list_ = nullptr;
    QFont suggestions_default_font_;
    QAction* add_favorite_action_ = nullptr;
    QAction* add_favorite_folder_action_ = nullptr;
    QAction* rename_favorite_action_ = nullptr;
    QAction* move_favorite_up_action_ = nullptr;
    QAction* move_favorite_down_action_ = nullptr;
    QAction* move_favorite_to_root_action_ = nullptr;
    QAction* import_favorites_action_ = nullptr;
    QAction* export_favorites_action_ = nullptr;
    QAction* export_favorites_list_action_ = nullptr;
    QAction* remove_favorite_action_ = nullptr;
    std::function<QString()> favorites_export_path_provider_;
    std::function<QString()> favorites_import_path_provider_;
    std::function<bool()> favorite_removal_confirmation_;
    bool favorites_command_busy_ = false;
    QAction* dictionary_browser_action_ = nullptr;
    QToolButton* lookup_button_ = nullptr;
    QToolBar* dictionary_bar_ = nullptr;
    std::map<std::uint32_t, std::vector<std::string>> participating_ids_;
    std::map<std::uint32_t, std::vector<std::string>> dictionary_members_;
    std::map<std::uint32_t, std::vector<std::string>> solo_restore_ids_;
    QLabel* status_ = nullptr;
    QTabWidget* article_tabs_ = nullptr;
    std::vector<goldendict::core::ArticleTabId> mru_tab_ids_;
    std::vector<goldendict::core::ArticleTabId> mru_traversal_ids_;
    bool mru_traversal_active_ = false;
    ArticleView* article_view_ = nullptr;
    ArticlePage* article_page_ = nullptr;
    ArticleSchemeHandler* scheme_handler_ = nullptr;
    QLineEdit* article_search_ = nullptr;
    QLabel* article_search_status_ = nullptr;
    QAction* back_action_ = nullptr;
    QAction* forward_action_ = nullptr;
    QAction* scan_popup_action_ = nullptr;
    QAction* pronounce_action_ = nullptr;
    QAction* reload_action_ = nullptr;
    QAction* zoom_in_action_ = nullptr;
    QAction* zoom_out_action_ = nullptr;
    QAction* zoom_reset_action_ = nullptr;
    QAction* words_zoom_in_action_ = nullptr;
    QAction* words_zoom_out_action_ = nullptr;
    QAction* words_zoom_reset_action_ = nullptr;
    QAction* new_tab_action_ = nullptr;
    QAction* page_setup_action_ = nullptr;
    QAction* print_preview_action_ = nullptr;
    QAction* print_action_ = nullptr;
    QAction* save_article_action_ = nullptr;
    QAction* rescan_files_action_ = nullptr;
    QAction* close_to_tray_action_ = nullptr;
    QAction* quit_action_ = nullptr;
    QAction* toggle_menubar_action_ = nullptr;
    QAction* show_dictionary_bar_names_action_ = nullptr;
    QAction* use_small_toolbar_icons_action_ = nullptr;
    QAction* always_on_top_action_ = nullptr;
    QAction* dictionaries_action_ = nullptr;
    QAction* preferences_action_ = nullptr;
    QAction* search_in_page_action_ = nullptr;
    QAction* full_text_search_action_ = nullptr;
    QPointer<goldendict::app::FullTextSearchDialog> full_text_search_dialog_;
    goldendict::app::FullTextSearchDialog* published_full_text_search_dialog_ =
        nullptr;
    std::string full_text_dialog_geometry_;
    QAction* visit_homepage_action_ = nullptr;
    QAction* show_reference_action_ = nullptr;
#if defined(Q_OS_LINUX)
    QPointer<goldendict::app::HelpWindow> help_window_;
    QPointer<goldendict::app::FullTextSearchDialog>
        help_connected_full_text_dialog_;
    QString help_directory_override_;
#endif
    QAction* visit_forum_action_ = nullptr;
    QAction* open_config_folder_action_ = nullptr;
    QAction* about_action_ = nullptr;
    QString configuration_directory_;
    std::function<bool(const QUrl&)> external_url_dispatcher_;
    QTimer* completion_timer_ = nullptr;
    std::map<goldendict::core::ArticleTabId,
             std::unique_ptr<goldendict::core::LookupRequest>>
        requests_;

    struct DictionaryResultPresentation {
        std::string dictionary_id;
        std::string display_name;
        int first_result_index = 0;
    };

    struct LookupResultPresentation {
        std::uint64_t generation = 0U;
        std::vector<DictionaryResultPresentation> rows;
    };

    std::map<goldendict::core::ArticleTabId, LookupResultPresentation>
        lookup_results_;

    struct ArticleSearchPresentation {
        QString query;
        QString status;
        std::uint64_t generation = 0U;
        std::uint64_t accepted_query_generation = 0U;
        goldendict::core::FullTextQueryMode mode =
            goldendict::core::FullTextQueryMode::kWholeWords;
        bool match_case = false;
        bool ignore_word_order = false;
        std::optional<std::uint32_t> maximum_word_distance;
        bool ignore_diacritics = false;
    };

    std::map<goldendict::core::ArticleTabId, ArticleSearchPresentation>
        article_search_presentations_;

    struct ArticleReloadState {
        std::uint64_t generation = 0U;
        std::optional<std::uint64_t> in_flight_generation;
        bool load_started = false;
        QPointer<ArticleView> view;
    };

    std::map<goldendict::core::ArticleTabId, ArticleReloadState>
        article_reload_states_;

    struct PendingArticleSearchHandoff {
        QString query;
        std::uint64_t lookup_generation = 0U;
        std::uint64_t search_generation = 0U;
        QPointer<ArticleView> view;
        goldendict::core::FullTextQueryMode mode =
            goldendict::core::FullTextQueryMode::kWholeWords;
        bool match_case = false;
        bool ignore_word_order = false;
        std::optional<std::uint32_t> maximum_word_distance;
        bool ignore_diacritics = false;
        std::uint64_t accepted_query_generation = 0U;
    };

    std::map<goldendict::core::ArticleTabId, PendingArticleSearchHandoff>
        pending_article_search_handoffs_;

    struct RenderedPageTextTransport {
        QString text;
        std::uint64_t accepted_query_generation = 0U;
        std::uint64_t lookup_generation = 0U;
        std::uint64_t search_generation = 0U;
        std::uint64_t navigation_generation = 0U;
        QPointer<ArticleView> view;
        QPointer<QWebEnginePage> page;
    };

    std::map<goldendict::core::ArticleTabId, std::uint64_t>
        article_navigation_generations_;
    std::map<goldendict::core::ArticleTabId, RenderedPageTextTransport>
        rendered_page_text_transports_;

    struct RenderedTextMatchPlanIdentity {
        std::uint64_t work_generation = 0U;
        std::uint64_t accepted_query_generation = 0U;
        std::uint64_t lookup_generation = 0U;
        std::uint64_t search_generation = 0U;
        std::uint64_t navigation_generation = 0U;
        goldendict::core::ArticleTabId tab_id = 0U;
        goldendict::core::RenderedTextMatchPlanRequest request;
        QPointer<ArticleView> view;
        QPointer<QWebEnginePage> page;
    };

    struct RenderedTextMatchPlanState {
        RenderedTextMatchPlanIdentity identity;
        goldendict::core::RenderedTextMatchPlanResult result;
        QString application_token;
        bool applied = false;
        int occurrence_count = 0;
        int current_position = -1;
    };

    std::uint64_t rendered_text_match_plan_generation_ = 0U;
    std::optional<RenderedTextMatchPlanIdentity>
        pending_rendered_text_match_plan_;
    std::map<goldendict::core::ArticleTabId, RenderedTextMatchPlanState>
        rendered_text_match_plans_;
    RenderedTextMatchPlanController* rendered_text_match_plan_controller_ =
        nullptr;
    std::unique_ptr<RenderedTextMatchPlanController>
        rendered_text_match_plan_controller_owner_;
    std::unique_ptr<RenderedTextMatchPlanController>
        retired_rendered_text_match_plan_controller_;

    struct PendingArticleScrollRestoration {
        QPointF position;
        QPointer<ArticleView> view;
        QPointer<QWebEnginePage> page;
        quint64 navigation_token = 0U;
    };

    std::map<goldendict::core::ArticleTabId, PendingArticleScrollRestoration>
        pending_article_scroll_restorations_;

    struct SuggestionPresentation {
        QString query;
        std::uint32_t group_id = 0U;
        std::uint64_t generation = 0U;
        std::vector<goldendict::core::HeadwordSuggestion> rows;
    };

    std::map<goldendict::core::ArticleTabId, SuggestionPresentation>
        suggestions_;
    std::uint64_t suggestion_generation_ = 0U;
    SuggestionWorker* suggestion_worker_ = nullptr;
    std::unique_ptr<SuggestionWorker> suggestion_worker_owner_;
    std::unique_ptr<SuggestionWorker> retired_suggestion_worker_;
    DictionaryBrowser* dictionary_browser_ = nullptr;
    std::unique_ptr<QPrinter> printer_;

    struct ArticleOutputRequest {
        std::uint64_t request_generation = 0U;
        std::uint64_t facade_binding_generation = 0U;
        std::uint64_t navigation_generation = 0U;
        goldendict::core::ArticleTabId tab_id = 0U;
        QPointer<ArticleView> view;
        QPointer<QWebEnginePage> page;
        QString path;
    };

    std::uint64_t facade_binding_generation_ = 1U;
    std::uint64_t pdf_request_generation_ = 0U;
    std::optional<ArticleOutputRequest> pending_pdf_request_;
    std::optional<ArticleOutputRequest> pending_print_request_;
    bool print_in_progress_ = false;
    bool restoring_main_window_state_ = false;
    std::function<bool(QPrinter*)> print_dialog_executor_;
    std::function<int(QPrinter*)> page_setup_executor_;
    std::function<void(QPrinter*, const std::function<void()>&)>
        print_preview_executor_;
    std::function<void(ArticleView*, QPrinter*)> print_dispatcher_;
    std::function<void(ArticleView*,
                       const std::function<void(const QByteArray&)>&)>
        pdf_dispatcher_;
    std::function<bool(const QString&, const QByteArray&)> pdf_writer_;
    std::function<bool()> printer_available_;
    std::function<QString()> save_article_path_provider_;
    std::function<bool(const QString&, const QString&)> article_save_writer_;
    std::function<void()> quit_dispatcher_;
    bool save_in_progress_ = false;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
