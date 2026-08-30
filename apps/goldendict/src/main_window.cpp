// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <exception>
#include <limits>
#include <thread>
#include <utility>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointer>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineFindTextResult>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>
#include <QWidget>

#include "article_page.h"
#include "article_scheme_handler.h"
#include "article_view.h"
#include "dictionary_browser.h"
#include "favorites_tree_widget.h"
#include "full_text_dictionary_projection.h"
#include "full_text_query_composer.h"
#include "full_text_search_dialog.h"
#include "goldendict/core/desktop_facade.h"
#include "group_editor.h"
#if defined(Q_OS_LINUX)
#include "help_window.h"
#endif
#include "preferences_dialog.h"
#include "rendered_text_match_plan_controller.h"
#include "source_directories_dialog.h"
#include "suggestion_worker.h"
#include "widgets_facade_binding.h"
#include "widgets_presentation_host.h"

namespace {

constexpr int kMainWindowStateVersion = 7;
constexpr int kPreviousMainWindowStateVersion = 6;
constexpr int kOlderMainWindowStateVersion = 5;
constexpr int kOldestMainWindowStateVersion = 4;
constexpr int kEarlierMainWindowStateVersion = 3;
constexpr int kEarliestMainWindowStateVersion = 2;
constexpr qsizetype kMaximumMainWindowStateBytes = 64 * 1024;
constexpr auto kHistoryPaneName = "historyPane";
constexpr auto kFavoritesPaneName = "favoritesPane";
constexpr auto kResultsPaneName = "dictsPane";
constexpr auto kSearchPaneName = "searchPane";
constexpr auto kPreviousHistoryDockName = "historyDock";
constexpr auto kPreviousFavoritesDockName = "favoritesDock";

std::optional<QPointF> QtPageScrollToCssScroll(const QPointF& position,
                                               qreal zoom_factor) {
    if (!std::isfinite(zoom_factor) || zoom_factor <= 0.0)
        return std::nullopt;
    return QPointF(position.x() / zoom_factor, position.y() / zoom_factor);
}

class EscapeConsumer final : public QObject {
   public:
    bool consumed = false;

   protected:
    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape) {
                consumed = true;
                return true;
            }
        }
        return false;
    }
};

QString EscapeHtml(QString text) {
    return text.replace('&', QStringLiteral("&amp;"))
        .replace('<', QStringLiteral("&lt;"))
        .replace('>', QStringLiteral("&gt;"));
}

}  // namespace

class WidgetsFacadeActivationRelay final : public QObject {
   public:
    enum Connection : std::uint32_t {
        kActions = 1U << 0U,
        kArticlePages = 1U << 1U,
        kArticleViews = 1U << 2U,
        kSuggestions = 1U << 3U,
        kRenderedMatches = 1U << 4U,
        kGroupSelector = 1U << 5U,
        kArticleTabs = 1U << 6U,
        kFullTextOutputs = 1U << 7U,
        kAllConnections = kActions | kArticlePages | kArticleViews |
                          kSuggestions | kRenderedMatches | kGroupSelector |
                          kArticleTabs | kFullTextOutputs,
    };
    enum class Action { kBack, kForward, kNewTab, kFullText, kSave };

    WidgetsFacadeActivationRelay(MainWindow* owner, std::uint64_t generation,
                                 std::uint64_t epoch, QObject* parent)
        : QObject(parent),
          prepared_owner_(owner),
          prepared_generation_(generation),
          prepared_epoch_(epoch) {}

    void MarkConnected(Connection connection) noexcept {
        connection_mask_.fetch_or(connection, std::memory_order_relaxed);
    }

    bool IsComplete() const noexcept {
        return connection_mask_.load(std::memory_order_relaxed) ==
               kAllConnections;
    }

    bool IsEnabled() const noexcept {
        return enabled_.load(std::memory_order_acquire);
    }

    bool CanPublishWithoutAllocation(MainWindow* owner,
                                     std::uint64_t generation,
                                     std::uint64_t epoch) const noexcept {
        return IsComplete() && !IsEnabled() && owner == prepared_owner_ &&
               generation == prepared_generation_ && epoch == prepared_epoch_;
    }

    bool PublishAndEnable(MainWindow* owner, std::uint64_t expected_generation,
                          std::uint64_t expected_epoch,
                          std::uint64_t published_generation,
                          std::uint64_t published_epoch) noexcept {
        if (!CanPublishWithoutAllocation(owner, expected_generation,
                                         expected_epoch) ||
            published_generation == 0U || published_epoch == 0U)
            return false;
        published_owner_.store(owner, std::memory_order_release);
        published_generation_.store(published_generation,
                                    std::memory_order_release);
        published_epoch_.store(published_epoch, std::memory_order_release);
        enabled_.store(true, std::memory_order_release);
        return true;
    }

    void Disable() noexcept {
        enabled_.store(false, std::memory_order_release);
        published_owner_.store(nullptr, std::memory_order_release);
    }

    std::uint64_t SuppressedDeliveries() const noexcept {
        return suppressed_deliveries_.load(std::memory_order_relaxed);
    }

    void SelectGroup(std::uint32_t group_id) {
        Deliver([&](MainWindow& owner) { owner.SelectGroup(group_id); });
    }

    void GroupSelectionChanged(int index) {
        Deliver([&](MainWindow& owner) {
            owner.AdvancePresentationMutationEpoch();
            if (index >= 0) {
                owner.selected_group_id_ =
                    owner.group_selector_->itemData(index).toUInt();
            }
            owner.selected_group_id_ =
                owner.group_selector_->currentData().value<quint32>();
            owner.RefreshDictionaryBar();
            owner.StartSuggestionLookup();
        });
    }

    void ActivateTab(int index) {
        Deliver([&](MainWindow& owner) { owner.ActivateArticleTab(index); });
    }

    void CloseTab(int index) {
        Deliver([&](MainWindow& owner) { owner.CloseArticleTab(index); });
    }

    void TabMoved() {
        Deliver([](MainWindow& owner) {
            owner.AdvancePresentationMutationEpoch();
        });
    }

    void TabContextMenu(const QPoint& position) {
        Deliver([&](MainWindow& owner) { owner.ShowTabContextMenu(position); });
    }

    void DictionaryAction(const std::string& dictionary_id, bool checked,
                          Qt::KeyboardModifiers modifiers) {
        Deliver([&](MainWindow& owner) {
            owner.ApplyPreparedDictionaryAction(dictionary_id, checked,
                                                modifiers);
        });
    }

    void TriggerAction(Action action) {
        Deliver([&](MainWindow& owner) {
            switch (action) {
                case Action::kBack:
                    owner.NavigateArticleTab(false);
                    break;
                case Action::kForward:
                    owner.NavigateArticleTab(true);
                    break;
                case Action::kNewTab:
                    owner.CreateEmptyArticleTab(
                        !owner.preferences_.open_new_tabs_in_background);
                    break;
                case Action::kFullText:
                    owner.ShowFullTextSearch();
                    break;
                case Action::kSave:
                    owner.SaveArticle();
                    break;
            }
        });
    }

    void ArticleLoadStarted(goldendict::core::ArticleTabId tab_id,
                            ArticleView* view) {
        Deliver([&](MainWindow& owner) {
            if (owner.ArticleViewForTab(tab_id) != view)
                return;
            owner.InvalidateArticleOutputOwnership(tab_id, view);
            owner.InvalidateRenderedTextMatchPlan(tab_id);
            ++owner.article_navigation_generations_[tab_id];
            owner.rendered_page_text_transports_.erase(tab_id);
        });
    }

    void PageLookup(goldendict::core::ArticleTabId tab_id,
                    const QString& internal_url,
                    ArticleLinkDisposition disposition) {
        Deliver([&](MainWindow& owner) {
            owner.OpenArticleLink(tab_id, QUrl(internal_url), disposition);
        });
    }

    void ArticleLink(goldendict::core::ArticleTabId tab_id, const QUrl& url,
                     ArticleLinkDisposition disposition) {
        Deliver([&](MainWindow& owner) {
            if (disposition == ArticleLinkDisposition::kNewForegroundTab &&
                owner.preferences_.open_new_tabs_in_background) {
                disposition = ArticleLinkDisposition::kNewBackgroundTab;
            }
            owner.OpenArticleLink(tab_id, url, disposition);
        });
    }

    void SelectionLookup(goldendict::core::ArticleTabId tab_id,
                         const QString& text,
                         ArticleLinkDisposition disposition) {
        Deliver([&](MainWindow& owner) {
            if (disposition == ArticleLinkDisposition::kNewForegroundTab &&
                owner.preferences_.open_new_tabs_in_background) {
                disposition = ArticleLinkDisposition::kNewBackgroundTab;
            }
            owner.LookupArticleSelection(tab_id, text, disposition);
        });
    }

    void SelectionToInput(const QString& text) {
        Deliver([&](MainWindow& owner) { owner.query_->setText(text); });
    }

    void ExternalUrl(const QUrl& url) {
        Deliver([&](MainWindow&) { QDesktopServices::openUrl(url); });
    }

    void DictionaryResult(goldendict::core::ArticleTabId tab_id,
                          ArticleView* view, const QString& dictionary_id,
                          int first_result_index, quint64 generation) {
        Deliver([&](MainWindow& owner) {
            const auto found = owner.lookup_results_.find(tab_id);
            if (found == owner.lookup_results_.end() ||
                found->second.generation != generation ||
                owner.ArticleViewForTab(tab_id) != view) {
                return;
            }
            const auto row = std::find_if(
                found->second.rows.begin(), found->second.rows.end(),
                [&](const auto& item) {
                    return item.dictionary_id == dictionary_id.toStdString() &&
                           item.first_result_index == first_result_index;
                });
            if (row != found->second.rows.end())
                owner.NavigateToArticleResult(view, first_result_index);
        });
    }

    void DictionaryResultsPane(goldendict::core::ArticleTabId tab_id,
                               ArticleView* view, quint64 generation) {
        Deliver([&](MainWindow& owner) {
            owner.ShowDictionaryResultsPane(tab_id, view, generation);
        });
    }

    void NavigationChanged() {
        Deliver([](MainWindow& owner) { owner.UpdateNavigationActions(); });
    }

    void ArticleLoadFinished(goldendict::core::ArticleTabId tab_id,
                             ArticleView* view, bool success) {
        Deliver([&](MainWindow& owner) {
            owner.HandleArticleLoadFinished(tab_id, view, success);
        });
    }

    void ArticleHtmlNavigationFinished(goldendict::core::ArticleTabId tab_id,
                                       ArticleView* view,
                                       quint64 navigation_token, bool success) {
        Deliver([&](MainWindow& owner) {
            owner.HandleArticleHtmlNavigationFinished(
                tab_id, view, navigation_token, success);
        });
    }

    void ArticlePageReplaced(goldendict::core::ArticleTabId tab_id,
                             ArticleView* view) {
        Deliver([&](MainWindow& owner) {
            if (owner.ArticleViewForTab(tab_id) == view) {
                owner.pending_article_scroll_restorations_.erase(tab_id);
                owner.InvalidateArticleOutputOwnership(tab_id, view);
            }
        });
    }

    void PrintFinished(ArticleView* view, bool success) {
        Deliver([&](MainWindow& owner) {
            owner.FinishPrinterRender(view, success);
        });
    }

    void FullTextNavigation(goldendict::core::ArticleTabId tab_id,
                            ArticleHighlightNavigationDirection direction) {
        Deliver([&](MainWindow& owner) {
            owner.NavigateFullTextHighlight(tab_id, direction);
        });
    }

    void ScrollChanged() {
        Deliver([](MainWindow& owner) {
            owner.AdvancePresentationMutationEpoch();
        });
    }

    void SuggestionFinished(goldendict::core::ArticleTabId tab_id,
                            std::uint64_t generation,
                            goldendict::core::SuggestionResponse response) {
        if (!IsEnabled()) {
            Suppress();
            return;
        }
        QMetaObject::invokeMethod(
            this, [this, tab_id, generation,
                   response = std::move(response)]() mutable {
                Deliver([&](MainWindow& owner) {
                    owner.FinishSuggestionLookup(tab_id, generation,
                                                 std::move(response));
                });
            });
    }

    void RenderedMatchFinished(
        std::uint64_t generation,
        goldendict::core::RenderedTextMatchPlanResult result) {
        Deliver([&](MainWindow& owner) {
            owner.FinishRenderedTextMatchPlan(generation, std::move(result));
        });
    }

    void FullTextResult(
        goldendict::app::FullTextResultActivationIntent intent) {
        Deliver([&](MainWindow& owner) {
            owner.NavigateToFullTextResult(std::move(intent));
        });
    }

    void FullTextQueryInvalidated() {
        Deliver([](MainWindow& owner) {
            owner.pending_article_search_handoffs_.clear();
            owner.rendered_page_text_transports_.clear();
            owner.InvalidateRenderedTextMatchPlan();
            for (auto& [tab_id, presentation] :
                 owner.article_search_presentations_) {
                static_cast<void>(tab_id);
                presentation.accepted_query_generation = 0U;
                ++presentation.generation;
            }
        });
    }

    void FullTextGeometry(std::string geometry) {
        Deliver([&](MainWindow& owner) {
            emit owner.FullTextDialogGeometryCaptured(std::move(geometry));
        });
    }

   private:
    bool CurrentOwner(MainWindow** owner) const noexcept {
        if (!IsEnabled())
            return false;
        auto* published = published_owner_.load(std::memory_order_acquire);
        if (published == nullptr || published != prepared_owner_ ||
            published->WidgetsInteractionBlocked() ||
            published->facade_preparation_generation_ !=
                published_generation_.load(std::memory_order_acquire) ||
            published->facade_preparation_shutdown_) {
            return false;
        }
        *owner = published;
        return true;
    }

    void Suppress() noexcept {
        suppressed_deliveries_.fetch_add(1U, std::memory_order_relaxed);
    }

    template <typename Callback>
    void Deliver(Callback&& callback) {
        MainWindow* owner = nullptr;
        if (!CurrentOwner(&owner)) {
            Suppress();
            return;
        }
        callback(*owner);
    }

    MainWindow* prepared_owner_ = nullptr;
    std::uint64_t prepared_generation_ = 0U;
    std::uint64_t prepared_epoch_ = 0U;
    std::atomic<MainWindow*> published_owner_ = nullptr;
    std::atomic<std::uint64_t> published_generation_ = 0U;
    std::atomic<std::uint64_t> published_epoch_ = 0U;
    std::atomic_bool enabled_ = false;
    std::atomic<std::uint32_t> connection_mask_ = 0U;
    std::atomic<std::uint64_t> suppressed_deliveries_ = 0U;
};

struct WidgetsFacadePreparationResources final {
    struct SurfaceState final {
        QPointer<QWidget> widget;
        Qt::FocusPolicy focus_policy = Qt::NoFocus;
        bool transparent_for_mouse = false;
        bool enabled = false;
    };

    std::shared_ptr<goldendict::core::DesktopFacade> facade;
    goldendict::core::DesktopFacade* facade_alias = nullptr;
    goldendict::core::ApplicationPreferences preferences;
    std::vector<goldendict::core::DictionaryGroupConfiguration> groups;
    std::vector<goldendict::core::DictionaryIdentity> catalog;
    goldendict::core::ArticleTabsState tabs;
    QString query_text;
    int query_cursor = 0;
    int query_selection_start = -1;
    int query_selection_length = 0;
    QString article_search_text;
    std::uint32_t selected_group_id = 0U;
    std::map<std::uint32_t, std::vector<std::string>> participating_ids;
    std::map<std::uint32_t, std::vector<std::string>> dictionary_members;
    std::map<std::uint32_t, std::vector<std::string>> solo_restore_ids;
    std::vector<goldendict::core::ArticleTabId> mru_tab_ids;
    std::map<goldendict::core::ArticleTabId, QPointF> scroll_positions;
    bool dictionary_bar_visible = false;
    QPointer<QWidget> focused_widget;
    QPointer<DictionaryBrowser> active_dictionary_browser;
    QPointer<goldendict::app::FullTextSearchDialog> active_full_text_dialog;
    QPointer<QWidget> root;
    QPointer<QComboBox> group_selector;
    QPointer<QWidget> dictionary_bar;
    QPointer<QTabWidget> article_tabs;
    QComboBox* group_selector_alias = nullptr;
    QTabWidget* article_tabs_alias = nullptr;
    ArticleView* article_view_alias = nullptr;
    ArticlePage* article_page_alias = nullptr;
    QPointer<goldendict::app::FullTextSearchDialog> full_text_dialog;
    goldendict::app::FullTextSearchDialog* full_text_dialog_alias = nullptr;
    QPointer<WidgetsFacadeActivationRelay> relay;
    std::unique_ptr<SuggestionWorker> suggestion_worker;
    SuggestionWorker* suggestion_worker_alias = nullptr;
    QPointer<RenderedTextMatchPlanController> match_controller;
    RenderedTextMatchPlanController* match_controller_alias = nullptr;
    WidgetsPresentationHost* group_host = nullptr;
    DictionaryBarPresentationHost* dictionary_host = nullptr;
    WidgetsPresentationHost* article_tabs_host = nullptr;
    goldendict::widgets::WidgetsFacadeBindingRegistry* binding_registry =
        nullptr;
    std::optional<std::uint8_t> binding_slot;
    std::uint8_t binding_slot_alias = 0U;
    std::unique_ptr<goldendict::widgets::WidgetsFacadeBindingDescriptor>
        binding_descriptor;
    std::vector<SurfaceState> surface_states;
    QPointer<QWidget> old_group_selector;
    QPointer<QWidget> old_dictionary_bar;
    QPointer<QTabWidget> old_article_tabs;
    QPointer<QWidget> old_focus;
    bool old_updates_enabled = true;
    bool group_switched = false;
    bool dictionary_switched = false;
    bool article_switched = false;
    bool ownership_staged = false;
    bool published = false;

    ~WidgetsFacadePreparationResources() {
        if (published)
            return;
        if (suggestion_worker != nullptr)
            suggestion_worker->Stop();
        if (match_controller != nullptr)
            match_controller->Stop();
        if (binding_registry != nullptr && binding_slot.has_value())
            binding_registry->Abandon(*binding_slot);
        if (group_host != nullptr)
            group_host->DetachInactive(group_selector);
        if (dictionary_host != nullptr)
            dictionary_host->DetachInactivePage(dictionary_bar);
        if (article_tabs_host != nullptr)
            article_tabs_host->DetachInactive(article_tabs);
        delete group_selector;
        delete dictionary_bar;
        delete article_tabs;
        delete root;
    }
};

struct WidgetsFacadePreparationRecord final {
    enum class State : std::uint8_t {
        kPrepared,
        kMaintaining,
        kMaintained,
        kPublished,
        kCleaning,
        kFinished,
        kAborted,
    };
    std::atomic_bool abandoned = false;
    std::atomic_bool ready = false;
    std::atomic_bool reclaimed = false;
    std::atomic_bool reclaimed_on_owner_thread = false;
    std::atomic<MainWindow*> owner = nullptr;
    std::atomic<State> state = State::kPrepared;
    std::uint64_t generation = 0U;
    std::uint64_t epoch = 0U;
    // Accessed and destroyed only by the GUI-thread owner registry.
    WidgetsFacadePreparationResources* resources = nullptr;
};

PreparedWidgetsFacadeCandidate::PreparedWidgetsFacadeCandidate() noexcept =
    default;

PreparedWidgetsFacadeCandidate::~PreparedWidgetsFacadeCandidate() {
    Abandon();
}

PreparedWidgetsFacadeCandidate::PreparedWidgetsFacadeCandidate(
    std::shared_ptr<WidgetsFacadePreparationRecord> record) noexcept
    : record_(std::move(record)) {}

PreparedWidgetsFacadeCandidate::PreparedWidgetsFacadeCandidate(
    PreparedWidgetsFacadeCandidate&&) noexcept = default;

PreparedWidgetsFacadeCandidate& PreparedWidgetsFacadeCandidate::operator=(
    PreparedWidgetsFacadeCandidate&& other) noexcept {
    if (this != &other) {
        Abandon();
        record_ = std::move(other.record_);
    }
    return *this;
}

PreparedWidgetsFacadeCandidate::operator bool() const noexcept {
    return record_ != nullptr &&
           record_->ready.load(std::memory_order_acquire) &&
           !record_->abandoned.load(std::memory_order_acquire) &&
           !record_->reclaimed.load(std::memory_order_acquire);
}

void PreparedWidgetsFacadeCandidate::Abandon() noexcept {
    if (record_ != nullptr) {
        record_->abandoned.store(true, std::memory_order_release);
        auto* owner = record_->owner.load(std::memory_order_acquire);
        if (owner != nullptr && qApp != nullptr &&
            QThread::currentThread() == qApp->thread() &&
            owner->facade_preparation_record_ == record_)
            owner->ReclaimAbandonedFacadeCandidates();
        record_.reset();
    }
}

MaintainedWidgetsCommit::~MaintainedWidgetsCommit() {
    if (owner_ == nullptr)
        return;
    if (QThread::currentThread() != owner_->thread())
        std::terminate();
    owner_->AbortMaintainedFacadeCommit(std::move(*this));
}

MaintainedWidgetsCommit::MaintainedWidgetsCommit(
    MaintainedWidgetsCommit&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)),
      record_(std::exchange(other.record_, nullptr)),
      generation_(std::exchange(other.generation_, 0U)) {}

MaintainedWidgetsCommit& MaintainedWidgetsCommit::operator=(
    MaintainedWidgetsCommit&& other) noexcept {
    if (this != &other) {
        if (owner_ != nullptr)
            std::terminate();
        owner_ = std::exchange(other.owner_, nullptr);
        record_ = std::exchange(other.record_, nullptr);
        generation_ = std::exchange(other.generation_, 0U);
    }
    return *this;
}

PublishedWidgetsCommit::~PublishedWidgetsCommit() {
    if (owner_ == nullptr)
        return;
    if (QThread::currentThread() != owner_->thread())
        std::terminate();
    owner_->FinishPublishedFacadeCommit(std::move(*this));
}

PublishedWidgetsCommit::PublishedWidgetsCommit(
    PublishedWidgetsCommit&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)),
      record_(std::exchange(other.record_, nullptr)),
      generation_(std::exchange(other.generation_, 0U)) {}

PublishedWidgetsCommit& PublishedWidgetsCommit::operator=(
    PublishedWidgetsCommit&& other) noexcept {
    if (this != &other) {
        if (owner_ != nullptr)
            std::terminate();
        owner_ = std::exchange(other.owner_, nullptr);
        record_ = std::exchange(other.record_, nullptr);
        generation_ = std::exchange(other.generation_, 0U);
    }
    return *this;
}

MainWindow::MainWindow(const QString& configuration_directory, QWidget* parent)
    : QMainWindow(parent),
      configuration_directory_(QDir::cleanPath(configuration_directory)),
      external_url_dispatcher_(
          [](const QUrl& url) { return QDesktopServices::openUrl(url); }) {
    setWindowTitle(QStringLiteral("GoldenDict"));
    resize(960, 640);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    auto* controls = new QHBoxLayout();
    dictionary_sources_button_ =
        new QPushButton(QStringLiteral("Dictionary Sources..."), central);
    dictionary_sources_button_->setObjectName(
        QStringLiteral("dictionarySourcesButton"));
    group_selector_ = new QComboBox(central);
    group_selector_->setObjectName(QStringLiteral("groupSelector"));
    group_selector_->setToolTip(
        QStringLiteral("Choose a dictionary group (Alt+G)"));
    group_selector_->setMaxVisibleItems(30);
    edit_groups_button_ =
        new QPushButton(QStringLiteral("Edit Groups..."), central);
    edit_groups_button_->setObjectName(QStringLiteral("editGroupsButton"));
    query_ = new QLineEdit(central);
    query_->setObjectName(QStringLiteral("translateLine"));
    query_->setPlaceholderText(QStringLiteral("Enter a word"));
    query_->setMinimumWidth(200);
    lookup_button_ = new QToolButton(central);
    lookup_button_->setObjectName(QStringLiteral("lookupButton"));
    lookup_button_->setText(QStringLiteral("Lookup"));
    lookup_button_->setPopupMode(QToolButton::MenuButtonPopup);
    auto* lookup_menu = new QMenu(lookup_button_);
    auto* lookup_new_tab =
        lookup_menu->addAction(QStringLiteral("Lookup in New Tab"));
    auto* lookup_background_tab =
        lookup_menu->addAction(QStringLiteral("Lookup in Background Tab"));
    lookup_button_->setMenu(lookup_menu);
    controls->addWidget(dictionary_sources_button_);
    controls->addWidget(edit_groups_button_);
    controls->addStretch();
    layout->addLayout(controls);

    status_ = new QLabel(QStringLiteral("No dictionary configured"), central);
    layout->addWidget(status_);
    article_tabs_ = new QTabWidget(central);
    article_tabs_->setObjectName(QStringLiteral("articleTabs"));
    article_tabs_->setTabsClosable(true);
    article_tabs_->setDocumentMode(true);
    article_tabs_->setMovable(false);
    article_tabs_->setTabBarAutoHide(preferences_.hide_single_tab);
    article_tabs_->tabBar()->installEventFilter(this);
    article_tabs_->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    auto* add_tab_button = new QToolButton(article_tabs_);
    add_tab_button->setObjectName(QStringLiteral("addArticleTabButton"));
    add_tab_button->setText(QStringLiteral("+"));
    add_tab_button->setToolTip(QStringLiteral("New Tab"));
    auto* add_tab_menu = new QMenu(add_tab_button);
    new_tab_action_ = new QAction(QStringLiteral("&New Tab"), this);
    new_tab_action_->setObjectName(QStringLiteral("newTab"));
    new_tab_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    new_tab_action_->setShortcutContext(Qt::WidgetShortcut);
    new_tab_action_->setMenuRole(QAction::NoRole);
    add_tab_menu->addAction(new_tab_action_);
    auto* add_background_tab =
        add_tab_menu->addAction(QStringLiteral("New Background Tab"));
    add_tab_button->setMenu(add_tab_menu);
    add_tab_button->setPopupMode(QToolButton::MenuButtonPopup);
    article_tabs_->setCornerWidget(add_tab_button, Qt::TopLeftCorner);
    article_tabs_host_ = new WidgetsPresentationHost(central);
    article_tabs_host_->setObjectName(
        QStringLiteral("widgetsArticleTabsPresentationHost"));
    article_tabs_host_->InstallActive(article_tabs_);
    layout->addWidget(article_tabs_host_, 1);

    auto* history_dock = new QDockWidget(QStringLiteral("&History Pane"), this);
    history_dock->setObjectName(QString::fromLatin1(kHistoryPaneName));
    auto* history_widget = new QWidget(history_dock);
    auto* history_layout = new QVBoxLayout(history_widget);
    history_layout->setContentsMargins(4, 4, 4, 4);
    history_filter_ = new QLineEdit(history_widget);
    history_filter_->setObjectName(QStringLiteral("historyFilter"));
    history_filter_->setPlaceholderText(QStringLiteral("Filter history"));
    history_filter_->setClearButtonEnabled(true);
    history_list_ = new QListWidget(history_widget);
    history_list_->setObjectName(QStringLiteral("historyList"));
    history_list_->setAlternatingRowColors(true);
    clear_history_button_ =
        new QPushButton(QStringLiteral("Clear History"), history_widget);
    clear_history_button_->setObjectName(QStringLiteral("clearHistoryButton"));
    export_history_button_ =
        new QPushButton(QStringLiteral("Export History..."), history_widget);
    export_history_button_->setObjectName(
        QStringLiteral("exportHistoryButton"));
    import_history_button_ =
        new QPushButton(QStringLiteral("Import History..."), history_widget);
    import_history_button_->setObjectName(
        QStringLiteral("importHistoryButton"));
    auto* history_buttons = new QHBoxLayout();
    history_buttons->addWidget(import_history_button_);
    history_buttons->addWidget(export_history_button_);
    history_buttons->addWidget(clear_history_button_);
    history_layout->addWidget(history_filter_);
    history_layout->addWidget(history_list_, 1);
    history_layout->addLayout(history_buttons);
    history_dock->setWidget(history_widget);

    auto* favorites_dock =
        new QDockWidget(QStringLiteral("Favor&ites Pane"), this);
    favorites_dock->setObjectName(QString::fromLatin1(kFavoritesPaneName));
    favorites_tree_ = new FavoritesTreeWidget(favorites_dock);
    favorites_tree_->setObjectName(QStringLiteral("favoritesTree"));
    favorites_tree_->setHeaderHidden(true);
    favorites_tree_->SetMoveRequest([this](const QList<int>& source_path,
                                           const QList<int>& destination_path,
                                           int destination_index) {
        emit MoveFavoriteAcrossFoldersRequested(source_path, destination_path,
                                                destination_index,
                                                ExpandedFavoriteFolderPaths());
    });
    favorites_dock->setWidget(favorites_tree_);

    auto* results_dock =
        new QDockWidget(QStringLiteral("&Results Navigation Pane"), this);
    results_dock->setObjectName(QString::fromLatin1(kResultsPaneName));
    results_list_ = new QListWidget(results_dock);
    results_list_->setObjectName(QStringLiteral("dictsList"));
    results_list_->setAlternatingRowColors(true);
    results_dock->setWidget(results_list_);

    auto* search_dock = new QDockWidget(QStringLiteral("&Search Pane"), this);
    search_dock->setObjectName(QString::fromLatin1(kSearchPaneName));
    auto* search_widget = new QWidget(search_dock);
    auto* search_layout = new QVBoxLayout(search_widget);
    search_layout->setContentsMargins(2, 1, 2, 1);
    suggestions_list_ = new QListWidget(search_widget);
    suggestions_list_->setObjectName(QStringLiteral("wordList"));
    suggestions_list_->setAlternatingRowColors(true);
    search_layout->addWidget(suggestions_list_, 1);
    search_dock->setWidget(search_widget);
    ApplyDefaultPaneLayout();

    auto* nav_toolbar = addToolBar(QStringLiteral("&Navigation"));
    nav_toolbar->setObjectName(QStringLiteral("navToolbar"));
    nav_toolbar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    back_action_ = nav_toolbar->addAction(QStringLiteral("Back"));
    back_action_->setShortcut(QKeySequence::Back);
    nav_toolbar->widgetForAction(back_action_)
        ->setObjectName(QStringLiteral("backButton"));
    forward_action_ = nav_toolbar->addAction(QStringLiteral("Forward"));
    forward_action_->setShortcut(QKeySequence::Forward);
    nav_toolbar->widgetForAction(forward_action_)
        ->setObjectName(QStringLiteral("forwardButton"));
    auto* lookup_controls = new QWidget(nav_toolbar);
    auto* lookup_layout = new QHBoxLayout(lookup_controls);
    lookup_layout->setContentsMargins(0, 0, 0, 0);
    lookup_layout->setSpacing(0);
    group_selector_->setParent(lookup_controls);
    query_->setParent(lookup_controls);
    lookup_button_->setParent(lookup_controls);
    group_selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    group_selector_host_ = new WidgetsPresentationHost(lookup_controls);
    group_selector_host_->setObjectName(
        QStringLiteral("widgetsGroupPresentationHost"));
    group_selector_host_->InstallActive(group_selector_);
    group_selector_host_->PreserveFirstShownWidth();
    lookup_layout->addWidget(group_selector_host_);
    lookup_layout->addWidget(query_, 1);
    lookup_layout->addWidget(lookup_button_);
    lookup_controls->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
    nav_toolbar->addWidget(lookup_controls);
    setTabOrder(group_selector_, query_);
    setTabOrder(query_, lookup_button_);
    setTabOrder(lookup_button_, suggestions_list_);
    setTabOrder(suggestions_list_, results_list_);
    query_->installEventFilter(this);
    suggestions_list_->installEventFilter(this);

    auto* focus_query_action = new QAction(this);
    focus_query_action->setShortcuts({QKeySequence(QStringLiteral("Alt+D")),
                                      QKeySequence(QStringLiteral("Ctrl+L"))});
    focus_query_action->setShortcutContext(Qt::WindowShortcut);
    addAction(focus_query_action);
    connect(focus_query_action, &QAction::triggered, query_, [this]() {
        query_->setFocus();
        query_->selectAll();
    });

    auto* article_toolbar = addToolBar(QStringLiteral("Article"));
    article_toolbar->setObjectName(QStringLiteral("articleToolbar"));
    insertToolBar(article_toolbar, nav_toolbar);
    reload_action_ = article_toolbar->addAction(QStringLiteral("Reload"));
    article_toolbar->addSeparator();
    add_favorite_action_ =
        article_toolbar->addAction(QStringLiteral("Add to Favorites"));
    add_favorite_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    add_favorite_folder_action_ =
        article_toolbar->addAction(QStringLiteral("New Favorite Folder"));
    rename_favorite_action_ =
        article_toolbar->addAction(QStringLiteral("Rename Favorite"));
    rename_favorite_action_->setEnabled(false);
    move_favorite_up_action_ =
        article_toolbar->addAction(QStringLiteral("Move Favorite Up"));
    move_favorite_up_action_->setEnabled(false);
    move_favorite_down_action_ =
        article_toolbar->addAction(QStringLiteral("Move Favorite Down"));
    move_favorite_down_action_->setEnabled(false);
    move_favorite_to_root_action_ =
        article_toolbar->addAction(QStringLiteral("Move Favorite to Root"));
    move_favorite_to_root_action_->setEnabled(false);
    import_favorites_action_ =
        article_toolbar->addAction(QStringLiteral("Import Favorites"));
    export_favorites_action_ =
        article_toolbar->addAction(QStringLiteral("Export Favorites"));
    remove_favorite_action_ =
        article_toolbar->addAction(QStringLiteral("Remove Favorite"));
    remove_favorite_action_->setShortcut(QKeySequence::Delete);
    remove_favorite_action_->setEnabled(false);
    dictionary_browser_action_ =
        article_toolbar->addAction(QStringLiteral("Dictionaries"));
    article_toolbar->addSeparator();
    article_search_ = new QLineEdit(article_toolbar);
    article_search_->setPlaceholderText(QStringLiteral("Find in article"));
    article_search_->setClearButtonEnabled(true);
    article_search_->setMaximumWidth(240);
    article_toolbar->addWidget(article_search_);
    auto* previous_action =
        article_toolbar->addAction(QStringLiteral("Previous"));
    auto* next_action = article_toolbar->addAction(QStringLiteral("Next"));
    article_search_status_ = new QLabel(article_toolbar);
    article_search_status_->setMinimumWidth(72);
    article_toolbar->addWidget(article_search_status_);
    article_toolbar->addSeparator();
    auto* zoom_out_action =
        article_toolbar->addAction(QStringLiteral("Zoom Out"));
    zoom_out_action->setShortcut(QKeySequence::ZoomOut);
    auto* zoom_reset_action =
        article_toolbar->addAction(QStringLiteral("Reset Zoom"));
    zoom_reset_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    auto* zoom_in_action =
        article_toolbar->addAction(QStringLiteral("Zoom In"));
    zoom_in_action->setShortcut(QKeySequence::ZoomIn);
    article_toolbar->addSeparator();
    auto* copy_action = article_toolbar->addAction(QStringLiteral("Copy"));
    copy_action->setShortcut(QKeySequence::Copy);
    save_article_action_ =
        article_toolbar->addAction(QStringLiteral("&Save Article"));
    save_article_action_->setObjectName(QStringLiteral("saveArticle"));
    save_article_action_->setShortcut(QKeySequence(Qt::Key_F2));
    save_article_action_->setMenuRole(QAction::NoRole);
    print_action_ = article_toolbar->addAction(QStringLiteral("&Print"));
    print_action_->setObjectName(QStringLiteral("print"));
    print_action_->setShortcut(QKeySequence::Print);
    print_action_->setMenuRole(QAction::NoRole);
    print_preview_action_ =
        article_toolbar->addAction(QStringLiteral("Print Pre&view"));
    print_preview_action_->setObjectName(QStringLiteral("printPreview"));
    print_preview_action_->setMenuRole(QAction::NoRole);
    auto* print_pdf_action =
        article_toolbar->addAction(QStringLiteral("Print PDF"));
    dictionary_bar_ = addToolBar(QStringLiteral("&Dictionary Bar"));
    dictionary_bar_->setObjectName(QStringLiteral("dictionaryBar"));
    dictionary_bar_->setAllowedAreas(Qt::AllToolBarAreas);
    dictionary_bar_->setMinimumWidth(0);
    dictionary_bar_->setSizePolicy(QSizePolicy::Ignored,
                                   QSizePolicy::Preferred);
    dictionary_bar_host_ = new DictionaryBarPresentationHost(dictionary_bar_);
    dictionary_bar_host_->setObjectName(
        QStringLiteral("widgetsDictionaryPresentationHost"));
    dictionary_bar_->addWidget(dictionary_bar_host_);

    auto* app_menu_bar = menuBar();
    app_menu_bar->setObjectName(QStringLiteral("menubar"));
    auto* file_menu = app_menu_bar->addMenu(QStringLiteral("&File"));
    file_menu->setObjectName(QStringLiteral("menuFile"));
    file_menu->addAction(new_tab_action_);
    file_menu->addSeparator();
    file_menu->addAction(print_preview_action_);
    file_menu->addAction(print_action_);
    file_menu->addSeparator();
    file_menu->addAction(save_article_action_);
    file_menu->addSeparator();
    quit_action_ = new QAction(QStringLiteral("&Quit"), this);
    quit_action_->setObjectName(QStringLiteral("quit"));
    quit_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quit_action_->setMenuRole(QAction::QuitRole);
    file_menu->addAction(quit_action_);
    auto* view_menu = app_menu_bar->addMenu(QStringLiteral("&View"));
    view_menu->setObjectName(QStringLiteral("menuView"));
    search_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_S));
    results_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_R));
    favorites_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_I));
    history_dock->toggleViewAction()->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_H));
    history_dock->toggleViewAction()->setObjectName(
        QStringLiteral("showHideHistory"));
    history_dock->toggleViewAction()->setMenuRole(QAction::NoRole);
    view_menu->addAction(search_dock->toggleViewAction());
    view_menu->addAction(results_dock->toggleViewAction());
    view_menu->addAction(favorites_dock->toggleViewAction());
    view_menu->addAction(history_dock->toggleViewAction());
    view_menu->addSeparator();
    view_menu->addAction(dictionary_bar_->toggleViewAction());
    view_menu->addAction(nav_toolbar->toggleViewAction());
    auto* edit_menu = app_menu_bar->addMenu(QStringLiteral("&Edit"));
    edit_menu->setObjectName(QStringLiteral("menu_Edit"));
    dictionaries_action_ =
        new QAction(QStringLiteral("&Dictionaries..."), this);
    dictionaries_action_->setObjectName(QStringLiteral("dictionaries"));
    dictionaries_action_->setShortcut(QKeySequence(Qt::Key_F3));
    dictionaries_action_->setMenuRole(QAction::NoRole);
    edit_menu->addAction(dictionaries_action_);
    dictionary_sources_button_->addAction(dictionaries_action_);
    preferences_action_ = new QAction(QStringLiteral("&Preferences..."), this);
    preferences_action_->setObjectName(QStringLiteral("preferences"));
    preferences_action_->setShortcut(QKeySequence(Qt::Key_F4));
    preferences_action_->setMenuRole(QAction::PreferencesRole);
    edit_menu->addAction(preferences_action_);
    auto* search_menu = app_menu_bar->addMenu(QStringLiteral("Search"));
    search_menu->setObjectName(QStringLiteral("menuSearch"));
    search_in_page_action_ =
        new QAction(QStringLiteral("Search in page"), this);
    search_in_page_action_->setObjectName(QStringLiteral("searchInPageAction"));
    search_in_page_action_->setShortcut(QKeySequence::Find);
    search_in_page_action_->setShortcutContext(Qt::WindowShortcut);
    search_in_page_action_->setMenuRole(QAction::TextHeuristicRole);
    search_menu->addAction(search_in_page_action_);
    full_text_search_action_ =
        new QAction(QStringLiteral("Full-text search"), this);
    full_text_search_action_->setObjectName(
        QStringLiteral("fullTextSearchAction"));
    full_text_search_action_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    full_text_search_action_->setShortcutContext(
        Qt::WidgetWithChildrenShortcut);
    full_text_search_action_->setMenuRole(QAction::TextHeuristicRole);
    full_text_search_action_->setEnabled(false);
    search_menu->addAction(full_text_search_action_);
    auto* history_menu = app_menu_bar->addMenu(QStringLiteral("H&istory"));
    history_menu->setObjectName(QStringLiteral("menuHistory"));
    history_menu->addAction(history_dock->toggleViewAction());
    export_history_action_ = new QAction(QStringLiteral("&Export"), this);
    export_history_action_->setObjectName(QStringLiteral("exportHistory"));
    export_history_action_->setMenuRole(QAction::NoRole);
    history_menu->addAction(export_history_action_);
    import_history_action_ = new QAction(QStringLiteral("&Import"), this);
    import_history_action_->setObjectName(QStringLiteral("importHistory"));
    import_history_action_->setMenuRole(QAction::NoRole);
    history_menu->addAction(import_history_action_);
    history_menu->addSeparator();
    clear_history_action_ = new QAction(QStringLiteral("&Clear"), this);
    clear_history_action_->setObjectName(QStringLiteral("clearHistory"));
    clear_history_action_->setMenuRole(QAction::NoRole);
    history_menu->addAction(clear_history_action_);
    UpdateHistoryActions();
    auto* favorites_menu = app_menu_bar->addMenu(QStringLiteral("Favo&rites"));
    favorites_menu->setObjectName(QStringLiteral("menuFavorites"));
    favorites_dock->toggleViewAction()->setObjectName(
        QStringLiteral("showHideFavorites"));
    favorites_dock->toggleViewAction()->setMenuRole(QAction::TextHeuristicRole);
    favorites_menu->addAction(favorites_dock->toggleViewAction());
    export_favorites_action_->setText(QStringLiteral("&Export"));
    export_favorites_action_->setObjectName(QStringLiteral("exportFavorites"));
    export_favorites_action_->setMenuRole(QAction::TextHeuristicRole);
    favorites_menu->addAction(export_favorites_action_);
    import_favorites_action_->setText(QStringLiteral("&Import"));
    import_favorites_action_->setObjectName(QStringLiteral("importFavorites"));
    import_favorites_action_->setMenuRole(QAction::TextHeuristicRole);
    favorites_menu->addAction(import_favorites_action_);
    favorites_menu->addSeparator();
    add_favorite_action_->setText(QStringLiteral("&Add"));
    add_favorite_action_->setObjectName(QStringLiteral("actionAddToFavorites"));
    add_favorite_action_->setMenuRole(QAction::TextHeuristicRole);
    favorites_menu->addAction(add_favorite_action_);
    UpdateFavoritesActions();
    auto* help_menu = app_menu_bar->addMenu(QStringLiteral("&Help"));
    help_menu->setObjectName(QStringLiteral("menu_Help"));
#if defined(Q_OS_LINUX)
    show_reference_action_ =
        new QAction(QStringLiteral("GoldenDict reference"), this);
    show_reference_action_->setObjectName(QStringLiteral("showReference"));
    show_reference_action_->setShortcut(QKeySequence(Qt::Key_F1));
    show_reference_action_->setShortcutContext(Qt::WindowShortcut);
    show_reference_action_->setMenuRole(QAction::NoRole);
    help_menu->addAction(show_reference_action_);
    help_menu->addSeparator();
#endif
    visit_homepage_action_ = new QAction(QStringLiteral("&Homepage"), this);
    visit_homepage_action_->setObjectName(QStringLiteral("visitHomepage"));
    visit_homepage_action_->setMenuRole(QAction::NoRole);
    help_menu->addAction(visit_homepage_action_);
    help_menu->addSeparator();
    open_config_folder_action_ =
        new QAction(QStringLiteral("&Configuration Folder"), this);
    open_config_folder_action_->setObjectName(
        QStringLiteral("openConfigFolder"));
    open_config_folder_action_->setMenuRole(QAction::NoRole);
    help_menu->addAction(open_config_folder_action_);
    help_menu->addSeparator();
    about_action_ = new QAction(QStringLiteral("&About"), this);
    about_action_->setObjectName(QStringLiteral("about"));
    about_action_->setToolTip(QStringLiteral("About GoldenDict"));
    about_action_->setMenuRole(QAction::AboutRole);
    help_menu->addAction(about_action_);
    setCentralWidget(central);

    scheme_handler_ = new ArticleSchemeHandler(this);
    QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
        QByteArrayLiteral("goldendict"), scheme_handler_);
    completion_timer_ = new QTimer(this);
    completion_timer_->setInterval(15);
    facade_candidate_reclaimer_ = new QTimer(this);
    facade_candidate_reclaimer_->setInterval(25);
    facade_candidate_reclaimer_->setSingleShot(false);
    connect(facade_candidate_reclaimer_, &QTimer::timeout, this,
            &MainWindow::ReclaimAbandonedFacadeCandidates);
    facade_binding_registry_ =
        std::make_unique<goldendict::widgets::WidgetsFacadeBindingRegistry>();
    scheme_handler_->SetBindingRegistry(facade_binding_registry_.get());
    facade_binding_reclaimer_ = new QTimer(this);
    facade_binding_reclaimer_->setInterval(10);
    connect(facade_binding_reclaimer_, &QTimer::timeout, this, [this]() {
        facade_binding_registry_->ReclaimRetired();
        if (!facade_binding_registry_->NeedsReclaim())
            facade_binding_reclaimer_->stop();
    });

    connect(dictionary_sources_button_, &QPushButton::clicked,
            dictionaries_action_, &QAction::trigger);
    connect(dictionaries_action_, &QAction::enabledChanged,
            dictionary_sources_button_, &QWidget::setEnabled);
    connect(dictionaries_action_, &QAction::triggered, this,
            &MainWindow::EditSourceDirectories);
    connect(preferences_action_, &QAction::triggered, this,
            &MainWindow::EditPreferences);
    connect(group_selector_, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                if (index >= 0) {
                    selected_group_id_ =
                        group_selector_->itemData(index).toUInt();
                }
            });
    connect(edit_groups_button_, &QPushButton::clicked, this,
            &MainWindow::EditDictionaryGroups);
    connect(lookup_button_, &QToolButton::clicked, this,
            &MainWindow::StartLookup);
    connect(lookup_new_tab, &QAction::triggered, this, [this]() {
        StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                         goldendict::core::TabActivationPolicy::kActivate);
    });
    connect(lookup_background_tab, &QAction::triggered, this, [this]() {
        StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                         goldendict::core::TabActivationPolicy::kKeepActive);
    });
    connect(add_tab_button, &QToolButton::clicked, new_tab_action_,
            &QAction::trigger);
    connect(new_tab_action_, &QAction::triggered, this, [this]() {
        CreateEmptyArticleTab(!preferences_.open_new_tabs_in_background);
    });
    connect(add_background_tab, &QAction::triggered, this,
            [this]() { CreateEmptyArticleTab(false); });
    connect(article_tabs_, &QTabWidget::currentChanged, this,
            &MainWindow::ActivateArticleTab);
    connect(article_tabs_, &QTabWidget::tabCloseRequested, this,
            &MainWindow::CloseArticleTab);
    connect(article_tabs_->tabBar(), &QWidget::customContextMenuRequested, this,
            &MainWindow::ShowTabContextMenu);
    connect(query_, &QLineEdit::returnPressed, this, &MainWindow::StartLookup);
    connect(query_, &QLineEdit::textChanged, this,
            &MainWindow::StartSuggestionLookup);
    connect(query_, &QLineEdit::textChanged, this,
            &MainWindow::AdvancePresentationMutationEpoch);
    connect(query_, &QLineEdit::selectionChanged, this,
            &MainWindow::AdvancePresentationMutationEpoch);
    connect(group_selector_, &QComboBox::currentIndexChanged, this, [this]() {
        AdvancePresentationMutationEpoch();
        selected_group_id_ = group_selector_->currentData().value<quint32>();
        RefreshDictionaryBar();
        StartSuggestionLookup();
    });
    connect(dictionary_bar_, &QToolBar::visibilityChanged, this,
            [this](bool) { ApplyDictionaryParticipation(); });
    connect(suggestions_list_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem*) { ActivateSuggestion(); });
    connect(results_list_, &QListWidget::itemSelectionChanged, this,
            &MainWindow::NavigateToSelectedResult);
    connect(results_list_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem*) { NavigateToSelectedResult(); });
    connect(completion_timer_, &QTimer::timeout, this,
            &MainWindow::FinishLookup);
    connect(history_list_, &QListWidget::itemActivated, this,
            [this](const QListWidgetItem* item) {
                if (item == nullptr) {
                    return;
                }
                query_->setText(item->text());
                StartLookupInTab(
                    goldendict::core::TabOpenPolicy::kCurrentTab,
                    goldendict::core::TabActivationPolicy::kActivate, {},
                    item->data(Qt::UserRole).value<quint32>());
            });
    connect(history_filter_, &QLineEdit::textChanged, this,
            &MainWindow::RefreshHistoryList);
    connect(clear_history_button_, &QPushButton::clicked, clear_history_action_,
            &QAction::trigger);
    connect(export_history_button_, &QPushButton::clicked,
            export_history_action_, &QAction::trigger);
    connect(import_history_button_, &QPushButton::clicked,
            import_history_action_, &QAction::trigger);
    connect(clear_history_action_, &QAction::triggered, this,
            &MainWindow::ClearHistory);
    connect(export_history_action_, &QAction::triggered, this,
            &MainWindow::ExportHistory);
    connect(import_history_action_, &QAction::triggered, this,
            &MainWindow::ImportHistory);
    connect(favorites_tree_, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem* item, int) {
                if (item == nullptr || item->data(0, Qt::UserRole).toBool()) {
                    return;
                }
                query_->setText(item->text(0));
                StartLookup();
            });
    connect(add_favorite_action_, &QAction::triggered, this, [this]() {
        if (favorites_command_busy_) {
            return;
        }
        const QString word = query_->text().trimmed();
        if (!word.isEmpty()) {
            favorites_command_busy_ = true;
            UpdateFavoritesActions();
            emit AddFavoriteRequested(word, SelectedFavoriteFolderPath());
            favorites_command_busy_ = false;
            UpdateFavoritesActions();
        }
    });
    connect(add_favorite_folder_action_, &QAction::triggered, this,
            &MainWindow::CreateFavoriteFolder);
    connect(favorites_tree_, &QTreeWidget::itemSelectionChanged, this,
            [this]() { UpdateFavoritesActions(); });
    connect(rename_favorite_action_, &QAction::triggered, this,
            &MainWindow::RenameFavorite);
    connect(move_favorite_up_action_, &QAction::triggered, this, [this]() {
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr) {
            emit MoveFavoriteRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>(), -1);
        }
    });
    connect(move_favorite_down_action_, &QAction::triggered, this, [this]() {
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr) {
            emit MoveFavoriteRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>(), 1);
        }
    });
    connect(move_favorite_to_root_action_, &QAction::triggered, this, [this]() {
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr && item->parent() != nullptr) {
            emit MoveFavoriteToRootRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>());
        }
    });
    connect(import_favorites_action_, &QAction::triggered, this,
            &MainWindow::ImportFavorites);
    connect(export_favorites_action_, &QAction::triggered, this,
            &MainWindow::ExportFavorites);
    connect(remove_favorite_action_, &QAction::triggered, this, [this]() {
        if (favorites_command_busy_) {
            return;
        }
        const auto* item = favorites_tree_->currentItem();
        if (item != nullptr && ConfirmFavoriteRemoval()) {
            favorites_command_busy_ = true;
            UpdateFavoritesActions();
            emit RemoveFavoriteRequested(
                item->data(0, Qt::UserRole + 1).value<QList<int>>());
            favorites_command_busy_ = false;
            UpdateFavoritesActions();
        }
    });
    connect(query_, &QLineEdit::textChanged, this,
            [this]() { UpdateFavoritesActions(); });
    connect(dictionary_browser_action_, &QAction::triggered, this,
            &MainWindow::ShowDictionaryBrowser);
    connect(back_action_, &QAction::triggered, this,
            [this]() { NavigateArticleTab(false); });
    connect(forward_action_, &QAction::triggered, this,
            [this]() { NavigateArticleTab(true); });
    connect(reload_action_, &QAction::triggered, this,
            [this]() { ReloadCurrentArticle(); });
    connect(article_search_, &QLineEdit::returnPressed, this,
            [this]() { FindInArticle(false); });
    connect(article_search_, &QLineEdit::textChanged, this,
            [this](const QString& text) {
                const auto tab_id = TabIdAt(article_tabs_->currentIndex());
                if (tab_id == 0U)
                    return;
                auto& presentation = article_search_presentations_[tab_id];
                presentation.query = text;
                ++presentation.generation;
                if (text.isEmpty()) {
                    if (auto* view = ArticleViewForTab(tab_id); view != nullptr)
                        view->findText(QString());
                    presentation.status.clear();
                    article_search_status_->clear();
                }
            });
    connect(article_search_, &QLineEdit::textChanged, this,
            &MainWindow::AdvancePresentationMutationEpoch);
    connect(article_search_, &QLineEdit::selectionChanged, this,
            &MainWindow::AdvancePresentationMutationEpoch);
    connect(previous_action, &QAction::triggered, this,
            [this]() { FindInArticle(true); });
    connect(next_action, &QAction::triggered, this,
            [this]() { FindInArticle(false); });
    connect(zoom_out_action, &QAction::triggered, this,
            [this]() { ZoomArticle(-0.1); });
    connect(zoom_reset_action, &QAction::triggered, this,
            [this]() { article_view_->setZoomFactor(1.0); });
    connect(zoom_in_action, &QAction::triggered, this,
            [this]() { ZoomArticle(0.1); });
    connect(copy_action, &QAction::triggered, this, [this]() {
        if (article_page_ != nullptr)
            article_page_->triggerAction(QWebEnginePage::Copy);
    });
    connect(save_article_action_, &QAction::triggered, this,
            &MainWindow::SaveArticle);
    connect(print_action_, &QAction::triggered, this,
            &MainWindow::PrintArticle);
    connect(print_preview_action_, &QAction::triggered, this,
            &MainWindow::PreviewArticle);
    connect(quit_action_, &QAction::triggered, this, [this]() {
        if (quit_dispatcher_)
            quit_dispatcher_();
        else
            QApplication::quit();
    });
    connect(print_pdf_action, &QAction::triggered, this,
            &MainWindow::SaveArticleAsPdf);
    connect(search_in_page_action_, &QAction::triggered, this, [this]() {
        article_search_->setFocus();
        article_search_->selectAll();
    });
    connect(full_text_search_action_, &QAction::triggered, this,
            &MainWindow::ShowFullTextSearch);
#if defined(Q_OS_LINUX)
    connect(show_reference_action_, &QAction::triggered, this,
            [this]() { ShowHelp(goldendict::app::HelpIntent::kReference); });
#endif
    connect(visit_homepage_action_, &QAction::triggered, this, [this]() {
        DispatchSafeExternalUrl(
            QUrl(QStringLiteral("https://goldendict.org/")));
    });
    connect(open_config_folder_action_, &QAction::triggered, this, [this]() {
        DispatchSafeExternalUrl(QUrl::fromLocalFile(configuration_directory_));
    });
    connect(about_action_, &QAction::triggered, this,
            &MainWindow::ShowAboutDialog);
    suggestion_worker_owner_ = std::make_unique<SuggestionWorker>(
        [this](goldendict::core::ArticleTabId tab_id, std::uint64_t generation,
               goldendict::core::SuggestionResponse response) {
            QMetaObject::invokeMethod(
                this, [this, tab_id, generation,
                       response = std::move(response)]() mutable {
                    FinishSuggestionLookup(tab_id, generation,
                                           std::move(response));
                });
        });
    suggestion_worker_ = suggestion_worker_owner_.get();
    rendered_text_match_plan_controller_owner_ =
        std::make_unique<RenderedTextMatchPlanController>(
            [this](std::uint64_t generation,
                   goldendict::core::RenderedTextMatchPlanResult result) {
                FinishRenderedTextMatchPlan(generation, std::move(result));
            },
            this);
    rendered_text_match_plan_controller_ =
        rendered_text_match_plan_controller_owner_.get();
    connect(this, &MainWindow::ArticleTabSessionMutated, this,
            &MainWindow::AdvancePresentationMutationEpoch);
    connect(article_tabs_->tabBar(), &QTabBar::tabMoved, this,
            [this](int, int) { AdvancePresentationMutationEpoch(); });
    connect(
        qApp, &QApplication::focusChanged, this,
        [this](QWidget* old, QWidget* current) {
            const auto belongs = [this](QWidget* widget) {
                return widget != nullptr &&
                       (widget == this || isAncestorOf(widget) ||
                        (full_text_search_dialog_ != nullptr &&
                         (widget == full_text_search_dialog_ ||
                          full_text_search_dialog_->isAncestorOf(widget))) ||
                        (dictionary_browser_ != nullptr &&
                         (widget == dictionary_browser_ ||
                          dictionary_browser_->isAncestorOf(widget))));
            };
            if (belongs(old) || belongs(current))
                AdvancePresentationMutationEpoch();
        });
    UpdateNavigationActions();
    UpdateFileActions();
    qApp->installEventFilter(this);
}

bool MainWindow::DispatchSafeExternalUrl(const QUrl& url) {
    const QUrl homepage(QStringLiteral("https://goldendict.org/"));
    const bool is_homepage =
        url == homepage && url.scheme() == QStringLiteral("https") &&
        url.host() == QStringLiteral("goldendict.org") &&
        url.userName().isEmpty() && url.password().isEmpty() &&
        url.port() == -1 && url.query().isEmpty() && url.fragment().isEmpty();
    const bool is_configuration_directory =
        !configuration_directory_.isEmpty() &&
        QDir::isAbsolutePath(configuration_directory_) &&
        url == QUrl::fromLocalFile(configuration_directory_);
    if ((!is_homepage && !is_configuration_directory) ||
        !external_url_dispatcher_) {
        return false;
    }
    return external_url_dispatcher_(url);
}

void MainWindow::ShowAboutDialog() {
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("aboutDialog"));
    dialog.setWindowTitle(QStringLiteral("About GoldenDict"));
    dialog.setWindowModality(Qt::WindowModal);
    auto* layout = new QVBoxLayout(&dialog);
    auto* product = new QLabel(QStringLiteral("GoldenDict"), &dialog);
    product->setObjectName(QStringLiteral("aboutProduct"));
    layout->addWidget(product);
    auto* version = new QLabel(
        QStringLiteral("Version %1").arg(QApplication::applicationVersion()),
        &dialog);
    version->setObjectName(QStringLiteral("aboutVersion"));
    layout->addWidget(version);
    auto* qt_version = new QLabel(
        QStringLiteral("Built with Qt %1").arg(QString::fromLatin1(qVersion())),
        &dialog);
    qt_version->setObjectName(QStringLiteral("aboutQtVersion"));
    layout->addWidget(qt_version);
    auto* license =
        new QLabel(QStringLiteral("Licensed under GPL-3.0-or-later"), &dialog);
    license->setObjectName(QStringLiteral("aboutLicense"));
    layout->addWidget(license);
    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::RunHelpMenuSmokeCheck(const QString& help_directory,
                                       std::function<void(bool)> completion) {
    auto* help_menu = findChild<QMenu*>(QStringLiteral("menu_Help"));
    if (help_menu == nullptr) {
        completion(false);
        return;
    }
    const auto actions = help_menu->actions();
    bool passed =
        menuBar()->actions().size() == 7 &&
        menuBar()->actions().back()->menu() == help_menu &&
        findChildren<QMenu*>(QStringLiteral("menu_Help")).size() == 1 &&
        help_menu->title() == QStringLiteral("&Help") &&
#if defined(Q_OS_LINUX)
        actions.size() == 7 && actions[0] == show_reference_action_ &&
        actions[1]->isSeparator() && actions[2] == visit_homepage_action_ &&
        actions[3]->isSeparator() && actions[4] == open_config_folder_action_ &&
        actions[5]->isSeparator() && actions[6] == about_action_ &&
        show_reference_action_->text() ==
            QStringLiteral("GoldenDict reference") &&
        show_reference_action_->shortcut() == QKeySequence(Qt::Key_F1) &&
        show_reference_action_->shortcutContext() == Qt::WindowShortcut &&
#else
        actions.size() == 5 && actions[0] == visit_homepage_action_ &&
        actions[1]->isSeparator() && actions[2] == open_config_folder_action_ &&
        actions[3]->isSeparator() && actions[4] == about_action_ &&
#endif
        visit_homepage_action_->text() == QStringLiteral("&Homepage") &&
        visit_homepage_action_->menuRole() == QAction::NoRole &&
        visit_homepage_action_->shortcut().isEmpty() &&
        visit_homepage_action_->isEnabled() &&
        open_config_folder_action_->text() ==
            QStringLiteral("&Configuration Folder") &&
        open_config_folder_action_->menuRole() == QAction::NoRole &&
        open_config_folder_action_->shortcut().isEmpty() &&
        open_config_folder_action_->isEnabled() &&
        about_action_->text() == QStringLiteral("&About") &&
        about_action_->menuRole() == QAction::AboutRole &&
        about_action_->shortcut().isEmpty() && about_action_->isEnabled() &&
        findChildren<QAction*>(QStringLiteral("visitHomepage")).size() == 1 &&
        findChildren<QAction*>(QStringLiteral("openConfigFolder")).size() ==
            1 &&
        findChildren<QAction*>(QStringLiteral("about")).size() == 1 &&
#if defined(Q_OS_LINUX)
        findChildren<QAction*>(QStringLiteral("showReference")).size() == 1 &&
#else
        findChildren<QAction*>(QStringLiteral("showReference")).isEmpty() &&
#endif
        findChildren<QAction*>(QStringLiteral("visitForum")).isEmpty();

    const auto all_actions = findChildren<QAction*>();
    passed = passed &&
             std::count_if(all_actions.cbegin(), all_actions.cend(),
                           [](const QAction* action) {
                               return action->menuRole() == QAction::AboutRole;
                           }) == 1 &&
             std::count_if(all_actions.cbegin(), all_actions.cend(),
                           [](const QAction* action) {
                               return action->shortcuts().contains(
                                   QKeySequence(Qt::Key_F1));
                           }) ==
#if defined(Q_OS_LINUX)
                 1;
#else
                 0;
#endif

#if defined(Q_OS_LINUX)
    help_directory_override_ = help_directory;
    const std::string initial_layout = CaptureMainWindowState();
    const QString initial_query = query_->text();
    const int initial_tab_count = article_tabs_->count();
    const int initial_tab_index = article_tabs_->currentIndex();
    show_reference_action_->trigger();
    auto* first_help_window = help_window_.data();
    show_reference_action_->trigger();
    full_text_search_action_->trigger();
    auto* full_text_help_action =
        full_text_search_dialog_ == nullptr
            ? nullptr
            : full_text_search_dialog_->findChild<QAction*>(
                  QStringLiteral("fullTextHelpAction"));
    if (full_text_help_action != nullptr)
        full_text_help_action->trigger();
    auto* help_browser = first_help_window == nullptr
                             ? nullptr
                             : first_help_window->findChild<QTextBrowser*>(
                                   QStringLiteral("helpBrowser"));
    passed = passed && first_help_window != nullptr &&
             first_help_window == help_window_.data() &&
             first_help_window->isVisible() && first_help_window->IsReady() &&
             full_text_help_action != nullptr && help_browser != nullptr &&
             help_browser->source().path().endsWith(
                 QStringLiteral("/fulltextsearch.html"));
    ShowDictionaryBrowser();
    auto* dictionary_help_action =
        dictionary_browser_ == nullptr
            ? nullptr
            : dictionary_browser_->findChild<QAction*>(
                  QStringLiteral("dictionaryBrowserHelpAction"));
    if (dictionary_help_action != nullptr)
        dictionary_help_action->trigger();
    passed = passed && dictionary_help_action != nullptr &&
             first_help_window == help_window_.data() &&
             help_browser->source().path().endsWith(
                 QStringLiteral("/headwords.html"));
    preferences_dialog_executor_ = [&passed, this, first_help_window,
                                    help_browser](PreferencesDialog& dialog) {
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        auto* help_action =
            dialog.findChild<QAction*>(QStringLiteral("preferencesHelpAction"));
        auto* help_button = buttons == nullptr
                                ? nullptr
                                : buttons->button(QDialogButtonBox::Help);
        auto* help_language =
            dialog.findChild<QComboBox*>(QStringLiteral("helpLanguage"));
        auto* interface_language =
            dialog.findChild<QComboBox*>(QStringLiteral("interfaceLanguage"));
        passed =
            passed && help_button != nullptr && help_action != nullptr &&
            interface_language != nullptr && interface_language->count() == 3 &&
            interface_language->itemText(0) == QStringLiteral("Default") &&
            interface_language->itemData(0).toString().isEmpty() &&
            interface_language->itemText(1) == QStringLiteral("English") &&
            interface_language->itemData(1).toString() ==
                QStringLiteral("en_US") &&
            interface_language->itemText(2) == QStringLiteral("Russian") &&
            interface_language->itemData(2).toString() ==
                QStringLiteral("ru_RU") &&
            help_language != nullptr && help_language->count() == 3 &&
            help_language->itemText(0) == QStringLiteral("Default") &&
            help_language->itemData(0).toString().isEmpty() &&
            help_language->itemText(1) == QStringLiteral("English") &&
            help_language->itemData(1).toString() == QStringLiteral("en_US") &&
            help_language->itemText(2) == QStringLiteral("Russian") &&
            help_language->itemData(2).toString() == QStringLiteral("ru_RU") &&
            help_button->objectName() ==
                QStringLiteral("preferencesHelpButton") &&
            help_action->shortcut() == QKeySequence(Qt::Key_F1) &&
            help_action->shortcutContext() == Qt::WidgetWithChildrenShortcut;
        if (help_button != nullptr)
            help_button->click();
        passed = passed && first_help_window == help_window_.data() &&
                 help_browser->source().path().endsWith(
                     QStringLiteral("/options.html"));
        if (help_action != nullptr)
            help_action->trigger();
        passed = passed && first_help_window == help_window_.data() &&
                 help_browser->source().path().endsWith(
                     QStringLiteral("/options.html"));
        if (help_language != nullptr)
            help_language->setCurrentIndex(
                help_language->currentIndex() == 2 ? 1 : 2);
        if (interface_language != nullptr)
            interface_language->setCurrentIndex(2);
        auto* ok = buttons == nullptr ? nullptr
                                      : buttons->button(QDialogButtonBox::Ok);
        if (ok != nullptr)
            ok->click();
        passed =
            passed && ok != nullptr && dialog.result() == QDialog::Accepted;
        return dialog.result();
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    passed = passed && help_window_ == nullptr &&
             preferences_.interface_language == "ru_RU" &&
             QCoreApplication::translate("GoldenDictInterfaceTranslations",
                                         "Interface translation smoke") ==
                 QStringLiteral("Interface translation smoke") &&
             (preferences_.help_language == "en_US" ||
              preferences_.help_language == "ru_RU");
    show_reference_action_->trigger();
    first_help_window = help_window_.data();
    help_browser = first_help_window == nullptr
                       ? nullptr
                       : first_help_window->findChild<QTextBrowser*>(
                             QStringLiteral("helpBrowser"));
    passed = passed && first_help_window != nullptr &&
             help_browser != nullptr &&
             first_help_window->CollectionPath().endsWith(
                 preferences_.help_language == "ru_RU"
                     ? QStringLiteral("/gdhelp_ru.qch")
                     : QStringLiteral("/gdhelp_en.qch"));
    source_dialog_executor_ = [&passed, this, first_help_window,
                               help_browser](SourceDirectoriesDialog& dialog) {
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("sourceDirectoriesButtonBox"));
        auto* help_action = dialog.findChild<QAction*>(
            QStringLiteral("sourceDirectoriesHelpAction"));
        auto* help_button = buttons == nullptr
                                ? nullptr
                                : buttons->button(QDialogButtonBox::Help);
        passed =
            passed && help_button != nullptr && help_action != nullptr &&
            help_button->objectName() ==
                QStringLiteral("sourceDirectoriesHelpButton") &&
            help_action->shortcut() == QKeySequence(Qt::Key_F1) &&
            help_action->shortcutContext() == Qt::WidgetWithChildrenShortcut;
        if (help_button != nullptr)
            help_button->click();
        passed = passed && first_help_window == help_window_.data() &&
                 help_browser->source().path().endsWith(
                     QStringLiteral("/dicts.html"));
        if (help_action != nullptr)
            help_action->trigger();
        passed = passed && first_help_window == help_window_.data() &&
                 help_browser->source().path().endsWith(
                     QStringLiteral("/dicts.html"));
        dialog.reject();
        return dialog.result();
    };
    EditSourceDirectories();
    source_dialog_executor_ = {};
#else
    Q_UNUSED(help_directory);
#endif
#if !defined(Q_OS_LINUX)
    const std::string initial_layout = CaptureMainWindowState();
    const QString initial_query = query_->text();
    const int initial_tab_count = article_tabs_->count();
    const int initial_tab_index = article_tabs_->currentIndex();
#endif
    QList<QUrl> dispatched_urls;
    external_url_dispatcher_ = [&dispatched_urls](const QUrl& url) {
        dispatched_urls.push_back(url);
        return true;
    };
    int homepage_triggers = 0;
    int folder_triggers = 0;
    const auto homepage_connection = connect(
        visit_homepage_action_, &QAction::triggered, this,
        [&homepage_triggers]() { ++homepage_triggers; }, Qt::DirectConnection);
    const auto folder_connection = connect(
        open_config_folder_action_, &QAction::triggered, this,
        [&folder_triggers]() { ++folder_triggers; }, Qt::DirectConnection);
    visit_homepage_action_->trigger();
    open_config_folder_action_->trigger();
    disconnect(homepage_connection);
    disconnect(folder_connection);
    passed = passed && homepage_triggers == 1 && folder_triggers == 1 &&
             dispatched_urls ==
                 QList<QUrl>{QUrl(QStringLiteral("https://goldendict.org/")),
                             QUrl::fromLocalFile(configuration_directory_)};
    const qsizetype safe_dispatch_count = dispatched_urls.size();
    for (const QUrl& unsafe :
         {QUrl(QStringLiteral("http://goldendict.org/")),
          QUrl(QStringLiteral("https://user@goldendict.org/")),
          QUrl(QStringLiteral("https://goldendict.org/forum/")),
          QUrl(QStringLiteral("javascript:alert(1)")),
          QUrl::fromLocalFile(QDir(configuration_directory_)
                                  .filePath(QStringLiteral("other")))}) {
        passed = passed && !DispatchSafeExternalUrl(unsafe);
    }
    passed = passed && dispatched_urls.size() == safe_dispatch_count;

    QTimer::singleShot(0, this, [this, &passed]() {
        auto* dialog = findChild<QDialog*>(QStringLiteral("aboutDialog"));
        auto* product =
            dialog == nullptr
                ? nullptr
                : dialog->findChild<QLabel*>(QStringLiteral("aboutProduct"));
        auto* version =
            dialog == nullptr
                ? nullptr
                : dialog->findChild<QLabel*>(QStringLiteral("aboutVersion"));
        auto* qt_version =
            dialog == nullptr
                ? nullptr
                : dialog->findChild<QLabel*>(QStringLiteral("aboutQtVersion"));
        auto* license =
            dialog == nullptr
                ? nullptr
                : dialog->findChild<QLabel*>(QStringLiteral("aboutLicense"));
        passed =
            passed && dialog != nullptr && dialog->parentWidget() == this &&
            dialog->isModal() && product != nullptr &&
            product->text() == QStringLiteral("GoldenDict") &&
            version != nullptr &&
            version->text() == QStringLiteral("Version %1")
                                   .arg(QApplication::applicationVersion()) &&
            qt_version != nullptr &&
            qt_version->text() == QStringLiteral("Built with Qt %1")
                                      .arg(QString::fromLatin1(qVersion())) &&
            license != nullptr &&
            license->text() ==
                QStringLiteral("Licensed under GPL-3.0-or-later");
        if (dialog != nullptr)
            dialog->reject();
    });
    about_action_->trigger();
    external_url_dispatcher_ = [](const QUrl& url) {
        return QDesktopServices::openUrl(url);
    };
    passed = passed && initial_layout == CaptureMainWindowState() &&
             query_->text() == initial_query &&
             article_tabs_->count() == initial_tab_count &&
             article_tabs_->currentIndex() == initial_tab_index &&
             centralWidget() != nullptr && centralWidget()->isEnabled();
    completion(passed);
}

void MainWindow::RunViewMenuSmokeCheck(std::function<void(bool)> completion) {
    auto* app_menu_bar = findChild<QMenuBar*>(QStringLiteral("menubar"));
    auto* view_menu = findChild<QMenu*>(QStringLiteral("menuView"));
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* favorites_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    auto* dictionary_bar =
        findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
    if (app_menu_bar == nullptr || view_menu == nullptr ||
        search_dock == nullptr || results_dock == nullptr ||
        favorites_dock == nullptr || history_dock == nullptr ||
        nav_toolbar == nullptr || dictionary_bar == nullptr) {
        completion(false);
        return;
    }

    const QList<QAction*> expected_actions = {
        search_dock->toggleViewAction(),
        results_dock->toggleViewAction(),
        favorites_dock->toggleViewAction(),
        history_dock->toggleViewAction(),
        nullptr,
        dictionary_bar->toggleViewAction(),
        nav_toolbar->toggleViewAction(),
    };
    const auto actions = view_menu->actions();
    bool passed =
        app_menu_bar == menuBar() &&
        findChildren<QMenuBar*>(QStringLiteral("menubar")).size() == 1 &&
        findChildren<QMenu*>(QStringLiteral("menuView")).size() == 1 &&
        app_menu_bar->actions().size() == 7 &&
        app_menu_bar->actions()[1]->menu() == view_menu &&
        view_menu->title() == QStringLiteral("&View") &&
        actions.size() == expected_actions.size();
    for (qsizetype index = 0; passed && index < actions.size(); ++index) {
        if (expected_actions[index] == nullptr) {
            passed = actions[index]->isSeparator();
        } else {
            QString accessible_text = actions[index]->text();
            accessible_text.remove('&');
            passed = actions[index] == expected_actions[index] &&
                     !actions[index]->isSeparator() &&
                     actions[index]->isCheckable() &&
                     actions[index]->isEnabled() &&
                     actions[index]->menuRole() != QAction::PreferencesRole &&
                     actions[index]->menuRole() != QAction::AboutRole &&
                     actions[index]->menuRole() != QAction::QuitRole &&
                     !accessible_text.trimmed().isEmpty();
        }
    }
    passed = passed &&
             search_dock->toggleViewAction()->shortcut() ==
                 QKeySequence(Qt::CTRL | Qt::Key_S) &&
             results_dock->toggleViewAction()->shortcut() ==
                 QKeySequence(Qt::CTRL | Qt::Key_R) &&
             favorites_dock->toggleViewAction()->shortcut() ==
                 QKeySequence(Qt::CTRL | Qt::Key_I) &&
             history_dock->toggleViewAction()->shortcut() ==
                 QKeySequence(Qt::CTRL | Qt::Key_H) &&
             dictionary_bar->toggleViewAction()->shortcut().isEmpty() &&
             nav_toolbar->toggleViewAction()->shortcut().isEmpty();
    const auto all_actions = findChildren<QAction*>();
    for (const auto& shortcut : {QKeySequence(Qt::CTRL | Qt::Key_S),
                                 QKeySequence(Qt::CTRL | Qt::Key_R),
                                 QKeySequence(Qt::CTRL | Qt::Key_I),
                                 QKeySequence(Qt::CTRL | Qt::Key_H)}) {
        passed =
            passed &&
            std::count_if(all_actions.cbegin(), all_actions.cend(),
                          [&shortcut](const QAction* action) {
                              return action->shortcuts().contains(shortcut);
                          }) == 1;
    }

    const std::string initial_state = CaptureMainWindowState();
    const QList<QPair<QWidget*, QAction*>> exposed_widgets = {
        {search_dock, search_dock->toggleViewAction()},
        {results_dock, results_dock->toggleViewAction()},
        {favorites_dock, favorites_dock->toggleViewAction()},
        {history_dock, history_dock->toggleViewAction()},
        {dictionary_bar, dictionary_bar->toggleViewAction()},
        {nav_toolbar, nav_toolbar->toggleViewAction()},
    };
    for (const auto& [widget, action] : exposed_widgets) {
        int toggles = 0;
        const auto connection = connect(
            action, &QAction::toggled, this, [&toggles](bool) { ++toggles; },
            Qt::DirectConnection);
        passed = passed && widget->isVisible() && action->isChecked();
        action->trigger();
        passed = passed && !widget->isVisible() && !action->isChecked() &&
                 toggles == 1;
        action->trigger();
        passed = passed && widget->isVisible() && action->isChecked() &&
                 toggles == 2;
        widget->hide();
        passed = passed && !action->isChecked() && toggles == 3;
        widget->show();
        passed = passed && action->isChecked() && toggles == 4;
        disconnect(connection);
    }
    passed = passed && CaptureMainWindowState() == initial_state &&
             centralWidget() != nullptr && article_tabs_ != nullptr &&
             article_tabs_->isVisible() && article_tabs_->size().width() > 0 &&
             article_tabs_->size().height() > 0 && kMainWindowStateVersion == 7;
    if (!passed) {
        qWarning() << "view menu smoke check failed" << actions.size()
                   << app_menu_bar->actions().size();
    }
    completion(passed);
}

void MainWindow::RunHistoryMenuSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    auto* history_menu = findChild<QMenu*>(QStringLiteral("menuHistory"));
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    if (history_menu == nullptr || history_dock == nullptr) {
        completion(false);
        return;
    }
    const auto actions = history_menu->actions();
    bool passed =
        menuBar()->actions().size() == 7 &&
        menuBar()->actions()[4]->menu() == history_menu &&
        history_menu->title() == QStringLiteral("H&istory") &&
        actions.size() == 5 && actions[0] == history_dock->toggleViewAction() &&
        actions[1] == export_history_action_ &&
        actions[2] == import_history_action_ && actions[3]->isSeparator() &&
        actions[4] == clear_history_action_ &&
        actions[0]->objectName() == QStringLiteral("showHideHistory") &&
        actions[1]->objectName() == QStringLiteral("exportHistory") &&
        actions[2]->objectName() == QStringLiteral("importHistory") &&
        actions[4]->objectName() == QStringLiteral("clearHistory") &&
        actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_H) &&
        actions[1]->shortcut().isEmpty() && actions[2]->shortcut().isEmpty() &&
        actions[4]->shortcut().isEmpty();
    for (const auto* action :
         {actions[0], actions[1], actions[2], actions[4]}) {
        passed = passed && action->menuRole() == QAction::NoRole;
    }

    SetDictionaryGroups({{7U, "History Menu Group", "", {}}});
    SelectGroup(7U);
    passed = passed && export_history_action_->isEnabled() &&
             import_history_action_->isEnabled() &&
             clear_history_action_->isEnabled();
    const std::string state = CaptureMainWindowState();
    actions[0]->trigger();
    actions[0]->trigger();
    passed = passed && history_dock->isVisible() && actions[0]->isChecked() &&
             CaptureMainWindowState() == state;

    int export_triggers = 0;
    const auto export_connection = connect(
        export_history_action_, &QAction::triggered, this,
        [&export_triggers]() { ++export_triggers; }, Qt::DirectConnection);
    history_export_path_provider_ = [path]() {
        return path;
    };
    export_history_button_->click();
    QFile exported(path);
    const bool export_opened = exported.open(QIODevice::ReadOnly);
    passed =
        passed && export_triggers == 1 && export_opened &&
        exported.readAll() == QByteArray::fromHex("efbbbf") + "Alpha\nBeta\n";
    disconnect(export_connection);

    int import_requests = 0;
    const auto import_connection = connect(
        this, &MainWindow::ImportHistoryRequested, this,
        [&import_requests, &path](const QString& requested,
                                  std::uint32_t group_id) {
            if (requested == path && group_id == 7U) {
                ++import_requests;
            }
        },
        Qt::DirectConnection);
    history_import_path_provider_ = [path]() {
        return path;
    };
    import_history_action_->trigger();
    passed = passed && import_requests == 1 && history_items_.size() == 2 &&
             history_items_[0].group_id == 7U;
    history_import_path_provider_ = []() {
        return QString();
    };
    import_history_button_->click();
    passed = passed && import_requests == 1;
    disconnect(import_connection);

    history_command_busy_ = true;
    UpdateHistoryActions();
    passed = passed && !export_history_action_->isEnabled() &&
             !import_history_action_->isEnabled() &&
             !clear_history_action_->isEnabled();
    history_command_busy_ = false;
    UpdateHistoryActions();
    clear_history_action_->trigger();
    passed = passed && history_items_.empty() &&
             !export_history_action_->isEnabled() &&
             import_history_action_->isEnabled() &&
             !clear_history_action_->isEnabled() &&
             centralWidget() != nullptr && article_tabs_->isVisible() &&
             kMainWindowStateVersion == 7;
    history_export_path_provider_ = {};
    history_import_path_provider_ = {};
    completion(passed);
}

void MainWindow::RunFavoritesMenuSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    auto* favorites_menu = findChild<QMenu*>(QStringLiteral("menuFavorites"));
    auto* favorites_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    if (favorites_menu == nullptr || favorites_dock == nullptr ||
        article_toolbar == nullptr) {
        completion(false);
        return;
    }
    const auto actions = favorites_menu->actions();
    bool passed =
        findChildren<QMenu*>(QStringLiteral("menuFavorites")).size() == 1 &&
        menuBar()->actions().size() == 7 &&
        menuBar()->actions()[5]->menu() == favorites_menu &&
        favorites_menu->title() == QStringLiteral("Favo&rites") &&
        actions.size() == 5 &&
        actions[0] == favorites_dock->toggleViewAction() &&
        actions[1] == export_favorites_action_ &&
        actions[2] == import_favorites_action_ && actions[3]->isSeparator() &&
        actions[4] == add_favorite_action_ &&
        actions[0]->objectName() == QStringLiteral("showHideFavorites") &&
        actions[1]->objectName() == QStringLiteral("exportFavorites") &&
        actions[2]->objectName() == QStringLiteral("importFavorites") &&
        actions[4]->objectName() == QStringLiteral("actionAddToFavorites") &&
        actions[1]->text() == QStringLiteral("&Export") &&
        actions[2]->text() == QStringLiteral("&Import") &&
        actions[4]->text() == QStringLiteral("&Add") &&
        findChildren<QAction*>(QStringLiteral("ExportFavoritesToList"))
            .isEmpty() &&
        actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_I) &&
        actions[1]->shortcut().isEmpty() && actions[2]->shortcut().isEmpty() &&
        actions[4]->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_E) &&
        article_toolbar->actions().contains(export_favorites_action_) &&
        article_toolbar->actions().contains(import_favorites_action_) &&
        article_toolbar->actions().contains(add_favorite_action_);
    for (const auto* action :
         {actions[0], actions[1], actions[2], actions[4]}) {
        passed = passed && action->menuRole() == QAction::TextHeuristicRole;
    }
    int favorites_shortcuts = 0;
    int add_shortcuts = 0;
    for (const auto* action : findChildren<QAction*>()) {
        favorites_shortcuts +=
            action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_I);
        add_shortcuts +=
            action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_E);
    }
    passed = passed && favorites_shortcuts == 1 && add_shortcuts == 1;

    while (favorites_tree_->topLevelItemCount() > 0) {
        const auto* item = favorites_tree_->topLevelItem(0);
        emit RemoveFavoriteRequested(
            item->data(0, Qt::UserRole + 1).value<QList<int>>());
    }
    UpdateFavoritesActions();

    const std::string initial_state = CaptureMainWindowState();
    actions[0]->trigger();
    const bool hidden =
        !favorites_dock->isVisible() && !actions[0]->isChecked();
    actions[0]->trigger();
    passed = passed && hidden && favorites_dock->isVisible() &&
             actions[0]->isChecked() &&
             CaptureMainWindowState() == initial_state;

    query_->clear();
    passed = passed && !add_favorite_action_->isEnabled() &&
             !export_favorites_action_->isEnabled() &&
             import_favorites_action_->isEnabled();
    emit AddFavoriteFolderRequested(QStringLiteral("Menu Folder"), {});
    auto* folder = favorites_tree_->topLevelItem(0);
    passed = passed && folder != nullptr &&
             folder->text(0) == QStringLiteral("Menu Folder") &&
             folder->data(0, Qt::UserRole).toBool();
    if (folder == nullptr) {
        completion(false);
        return;
    }
    favorites_tree_->setCurrentItem(folder);
    query_->setText(QStringLiteral("Menu Favorite"));
    int add_requests = 0;
    const auto add_connection = connect(
        this, &MainWindow::AddFavoriteRequested, this,
        [&add_requests](const QString& word, const QList<int>& parent_path) {
            if (word == QStringLiteral("Menu Favorite") &&
                parent_path == QList<int>{0}) {
                ++add_requests;
            }
        },
        Qt::DirectConnection);
    add_favorite_action_->trigger();
    disconnect(add_connection);
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && add_requests == 1 && folder != nullptr &&
             folder->childCount() == 1 &&
             folder->child(0)->text(0) == QStringLiteral("Menu Favorite") &&
             export_favorites_action_->isEnabled();

    int export_requests = 0;
    const auto export_connection = connect(
        this, &MainWindow::ExportFavoritesRequested, this,
        [&export_requests, &path](const QString& requested) {
            if (requested == path) {
                ++export_requests;
            }
        },
        Qt::DirectConnection);
    favorites_export_path_provider_ = [path]() {
        return path;
    };
    export_favorites_action_->trigger();
    QFile exported(path);
    const bool export_opened = exported.open(QIODevice::ReadOnly);
    const QByteArray exported_data =
        export_opened ? exported.readAll() : QByteArray();
    passed = passed && export_requests == 1 && export_opened &&
             exported_data.contains("Menu Folder") &&
             exported_data.contains("Menu Favorite");
    favorites_export_path_provider_ = []() {
        return QString();
    };
    export_favorites_action_->trigger();
    passed = passed && export_requests == 1;
    disconnect(export_connection);

    folder = favorites_tree_->topLevelItem(0);
    favorites_tree_->setCurrentItem(folder->child(0));
    int remove_requests = 0;
    const auto remove_connection = connect(
        this, &MainWindow::RemoveFavoriteRequested, this,
        [&remove_requests](const QList<int>& item_path) {
            if (item_path == QList<int>({0, 0})) {
                ++remove_requests;
            }
        },
        Qt::DirectConnection);
    favorite_removal_confirmation_ = []() {
        return true;
    };
    remove_favorite_action_->trigger();
    favorite_removal_confirmation_ = {};
    disconnect(remove_connection);
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && remove_requests == 1 && folder != nullptr &&
             folder->childCount() == 0;

    int import_requests = 0;
    const auto import_connection = connect(
        this, &MainWindow::ImportFavoritesRequested, this,
        [&import_requests, &path](const QString& requested) {
            if (requested == path) {
                ++import_requests;
            }
        },
        Qt::DirectConnection);
    favorites_import_path_provider_ = []() {
        return QString();
    };
    import_favorites_action_->trigger();
    passed = passed && import_requests == 0 &&
             favorites_tree_->topLevelItem(0)->childCount() == 0;
    favorites_import_path_provider_ = [path]() {
        return path;
    };
    import_favorites_action_->trigger();
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && import_requests == 1 && folder != nullptr &&
             folder->childCount() == 1 &&
             folder->child(0)->text(0) == QStringLiteral("Menu Favorite");
    disconnect(import_connection);

    const QString malformed_path = path + QStringLiteral(".malformed.xml");
    QFile malformed(malformed_path);
    if (malformed.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        malformed.write("<root><folder></root>");
        malformed.close();
    } else {
        passed = false;
    }
    const auto dismiss_warning = []() {
        QTimer::singleShot(0, []() {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* message = qobject_cast<QMessageBox*>(widget)) {
                    message->accept();
                }
            }
        });
    };
    favorites_import_path_provider_ = [malformed_path]() {
        return malformed_path;
    };
    dismiss_warning();
    import_favorites_action_->trigger();
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && folder != nullptr && folder->childCount() == 1 &&
             query_->text() == QStringLiteral("Menu Favorite");

    const QString failed_export_path = path + QStringLiteral(".directory");
    QDir().mkpath(failed_export_path);
    favorites_export_path_provider_ = [failed_export_path]() {
        return failed_export_path;
    };
    dismiss_warning();
    export_favorites_action_->trigger();
    passed = passed && QFileInfo(failed_export_path).isDir() &&
             favorites_tree_->topLevelItem(0)->childCount() == 1;

    favorites_command_busy_ = true;
    UpdateFavoritesActions();
    passed = passed && !add_favorite_action_->isEnabled() &&
             !remove_favorite_action_->isEnabled() &&
             !import_favorites_action_->isEnabled() &&
             !export_favorites_action_->isEnabled();
    favorites_command_busy_ = false;
    UpdateFavoritesActions();
    passed = passed && add_favorite_action_->isEnabled() &&
             import_favorites_action_->isEnabled() &&
             export_favorites_action_->isEnabled() &&
             centralWidget() != nullptr && article_tabs_->isVisible() &&
             query_->text() == QStringLiteral("Menu Favorite") &&
             kMainWindowStateVersion == 7;
    favorites_export_path_provider_ = {};
    favorites_import_path_provider_ = {};
    completion(passed);
}

void MainWindow::RunEditMenuSmokeCheck(std::function<void(bool)> completion) {
    auto* edit_menu = findChild<QMenu*>(QStringLiteral("menu_Edit"));
    if (edit_menu == nullptr || dictionary_sources_button_ == nullptr ||
        dictionaries_action_ == nullptr || preferences_action_ == nullptr ||
        facade_ == nullptr) {
        completion(false);
        return;
    }

    const auto actions = edit_menu->actions();
    bool passed =
        menuBar()->actions().size() == 7 &&
        menuBar()->actions()[0]->menu()->objectName() ==
            QStringLiteral("menuFile") &&
        menuBar()->actions()[1]->menu()->objectName() ==
            QStringLiteral("menuView") &&
        menuBar()->actions()[2]->menu() == edit_menu &&
        menuBar()->actions()[3]->menu()->objectName() ==
            QStringLiteral("menuSearch") &&
        menuBar()->actions()[4]->menu()->objectName() ==
            QStringLiteral("menuHistory") &&
        findChildren<QMenu*>(QStringLiteral("menu_Edit")).size() == 1 &&
        edit_menu->title() == QStringLiteral("&Edit") && actions.size() == 2 &&
        actions[0] == dictionaries_action_ &&
        dictionaries_action_->objectName() == QStringLiteral("dictionaries") &&
        dictionaries_action_->text() == QStringLiteral("&Dictionaries...") &&
        dictionaries_action_->shortcut() == QKeySequence(Qt::Key_F3) &&
        dictionaries_action_->menuRole() == QAction::NoRole &&
        dictionary_sources_button_->actions().size() == 1 &&
        dictionary_sources_button_->actions().front() == dictionaries_action_ &&
        actions[1] == preferences_action_ &&
        preferences_action_->objectName() == QStringLiteral("preferences") &&
        preferences_action_->text() == QStringLiteral("&Preferences...") &&
        preferences_action_->shortcut() == QKeySequence(Qt::Key_F4) &&
        preferences_action_->menuRole() == QAction::PreferencesRole;

    const auto all_actions = findChildren<QAction*>();
    passed = passed && std::count_if(all_actions.cbegin(), all_actions.cend(),
                                     [](const QAction* action) {
                                         return action->shortcuts().contains(
                                             QKeySequence(Qt::Key_F3));
                                     }) == 1;
    passed = passed && std::count_if(all_actions.cbegin(), all_actions.cend(),
                                     [](const QAction* action) {
                                         return action->shortcuts().contains(
                                             QKeySequence(Qt::Key_F4));
                                     }) == 1;

    const auto initial_paths = dictionary_paths_;
    const auto initial_sounds = sound_directories_;
    const auto initial_wikis = mediawiki_sources_;
    const auto initial_websites = website_sources_;
    const auto initial_forvo = forvo_sources_;
    const auto initial_dicts = dict_server_sources_;
    const auto initial_programs = external_program_sources_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    int triggers = 0;
    int dialogs = 0;
    const auto trigger_connection = connect(
        dictionaries_action_, &QAction::triggered, this,
        [&triggers]() { ++triggers; }, Qt::DirectConnection);

    source_dialog_executor_ = [this, &dialogs,
                               &passed](SourceDirectoriesDialog& dialog) {
        ++dialogs;
        passed = passed && source_configuration_busy_ &&
                 !dictionaries_action_->isEnabled() &&
                 !dictionary_sources_button_->isEnabled();
        dictionaries_action_->trigger();
        dialog.reject();
        return dialog.result();
    };
    dictionary_sources_button_->click();
    passed = passed && triggers == 1 && dialogs == 1 &&
             dictionaries_action_->isEnabled() &&
             dictionary_sources_button_->isEnabled() &&
             !source_configuration_busy_;

    const auto original_apply_callback = source_apply_callback_;
    source_apply_callback_ = [](const auto&, const auto&, const auto&,
                                const auto&, const auto&, const auto&,
                                const auto&, const auto&) {
        return QStringLiteral("forced edit menu apply failure");
    };
    source_dialog_executor_ = [&dialogs,
                               &passed](SourceDirectoriesDialog& dialog) {
        ++dialogs;
        auto* buttons = dialog.findChild<QDialogButtonBox*>();
        passed = passed && buttons != nullptr;
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Apply)->click();
        auto* error =
            dialog.findChild<QLabel*>(QStringLiteral("sourceValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    dictionaries_action_->trigger();
    passed = passed && triggers == 2 && dialogs == 2 &&
             dictionaries_action_->isEnabled() &&
             dictionary_sources_button_->isEnabled();

    source_apply_callback_ = [](const auto&, const auto&, const auto&,
                                const auto&, const auto&, const auto&,
                                const auto&, const auto&) {
        return QString{};
    };
    source_dialog_executor_ = [&dialogs,
                               &passed](SourceDirectoriesDialog& dialog) {
        ++dialogs;
        auto* buttons = dialog.findChild<QDialogButtonBox*>();
        passed = passed && buttons != nullptr;
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Apply)->click();
        passed = passed && dialog.result() == QDialog::Accepted;
        return dialog.result();
    };
    dictionaries_action_->trigger();
    passed = passed && triggers == 3 && dialogs == 3 &&
             dictionaries_action_->isEnabled() &&
             dictionary_sources_button_->isEnabled() &&
             !source_configuration_busy_;

    source_dialog_executor_ = {};
    source_apply_callback_ = original_apply_callback;
    disconnect(trigger_connection);

    const auto initial_preferences = preferences_;
    int preference_triggers = 0;
    int preference_dialogs = 0;
    const auto preference_trigger_connection = connect(
        preferences_action_, &QAction::triggered, this,
        [&preference_triggers]() { ++preference_triggers; },
        Qt::DirectConnection);
    preferences_dialog_executor_ =
        [this, &passed, &preference_dialogs](PreferencesDialog& dialog) {
            ++preference_dialogs;
            passed = passed && preferences_busy_ &&
                     !preferences_action_->isEnabled();
            preferences_action_->trigger();
            dialog.reject();
            return dialog.result();
        };
    preferences_action_->trigger();
    passed = passed && preference_triggers == 1 && preference_dialogs == 1 &&
             preferences_ == initial_preferences &&
             preferences_action_->isEnabled() && !preferences_busy_;

    const auto original_preferences_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced preferences failure");
    };
    preferences_dialog_executor_ = [&passed, &preference_dialogs](
                                       PreferencesDialog& dialog) {
        ++preference_dialogs;
        auto* background = dialog.findChild<QCheckBox*>(
            QStringLiteral("newTabsOpenInBackground"));
        auto* store_history =
            dialog.findChild<QCheckBox*>(QStringLiteral("storeHistory"));
        auto* maximum_history =
            dialog.findChild<QSpinBox*>(QStringLiteral("historyMaxSizeField"));
        auto* confirm_favorites = dialog.findChild<QCheckBox*>(
            QStringLiteral("confirmFavoritesDeletion"));
        auto* limit_phrase = dialog.findChild<QCheckBox*>(
            QStringLiteral("limitInputPhraseLength"));
        auto* phrase_limit = dialog.findChild<QSpinBox*>(
            QStringLiteral("inputPhraseLengthLimit"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && background != nullptr && store_history != nullptr &&
                 maximum_history != nullptr && confirm_favorites != nullptr &&
                 limit_phrase != nullptr && phrase_limit != nullptr &&
                 maximum_history->minimum() == 0 &&
                 maximum_history->maximum() == 99999 &&
                 maximum_history->isEnabled() &&
                 limit_phrase->text() ==
                     QStringLiteral("Ignore input phrases longer than") &&
                 phrase_limit->minimum() == 1 &&
                 phrase_limit->maximum() == 1000000 &&
                 phrase_limit->singleStep() == 10 &&
                 phrase_limit->isEnabled() == limit_phrase->isChecked() &&
                 buttons != nullptr;
        if (background != nullptr)
            background->setChecked(!background->isChecked());
        if (confirm_favorites != nullptr)
            confirm_favorites->setChecked(!confirm_favorites->isChecked());
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preference_triggers == 2 && preference_dialogs == 2 &&
             preferences_ == initial_preferences;

    preferences_apply_callback_ = original_preferences_callback;
    preferences_dialog_executor_ = [&passed, &preference_dialogs](
                                       PreferencesDialog& dialog) {
        ++preference_dialogs;
        auto* background = dialog.findChild<QCheckBox*>(
            QStringLiteral("newTabsOpenInBackground"));
        auto* after_current = dialog.findChild<QCheckBox*>(
            QStringLiteral("newTabsOpenAfterCurrentOne"));
        auto* store_history =
            dialog.findChild<QCheckBox*>(QStringLiteral("storeHistory"));
        auto* maximum_history =
            dialog.findChild<QSpinBox*>(QStringLiteral("historyMaxSizeField"));
        auto* confirm_favorites = dialog.findChild<QCheckBox*>(
            QStringLiteral("confirmFavoritesDeletion"));
        auto* limit_phrase = dialog.findChild<QCheckBox*>(
            QStringLiteral("limitInputPhraseLength"));
        auto* phrase_limit = dialog.findChild<QSpinBox*>(
            QStringLiteral("inputPhraseLengthLimit"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && background != nullptr && after_current != nullptr &&
                 store_history != nullptr && maximum_history != nullptr &&
                 confirm_favorites != nullptr && limit_phrase != nullptr &&
                 phrase_limit != nullptr && buttons != nullptr;
        if (background != nullptr)
            background->setChecked(!background->isChecked());
        if (after_current != nullptr)
            after_current->setChecked(!after_current->isChecked());
        if (store_history != nullptr)
            store_history->setChecked(false);
        if (maximum_history != nullptr) {
            passed = passed && maximum_history->isEnabled();
            maximum_history->setValue(0);
        }
        if (confirm_favorites != nullptr)
            confirm_favorites->setChecked(false);
        if (limit_phrase != nullptr)
            limit_phrase->setChecked(true);
        if (phrase_limit != nullptr) {
            passed = passed && phrase_limit->isEnabled();
            phrase_limit->setValue(2);
        }
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        passed = passed && dialog.result() == QDialog::Accepted;
        return dialog.result();
    };
    preferences_action_->trigger();
    auto expected_preferences = initial_preferences;
    expected_preferences.open_new_tabs_in_background =
        !expected_preferences.open_new_tabs_in_background;
    expected_preferences.open_new_tabs_after_current =
        !expected_preferences.open_new_tabs_after_current;
    expected_preferences.store_history = false;
    expected_preferences.maximum_history_entries = 0U;
    expected_preferences.confirm_favorites_deletion = false;
    expected_preferences.limit_input_phrase_length = true;
    expected_preferences.input_phrase_length_limit = 2U;
    expected_preferences.proxy_type = goldendict::core::ProxyType::kHttpConnect;
    passed = passed && preference_triggers == 3 && preference_dialogs == 3 &&
             preferences_ == expected_preferences &&
             facade_->ExportArticleTabSession() == initial_session;
    const auto before_rejected_phrase = facade_->ExportArticleTabSession();
    int rejected_submissions = 0;
    const auto rejected_submission_connection =
        connect(this, &MainWindow::LookupSubmitted, this,
                [&rejected_submissions]() { ++rejected_submissions; });
    query_->setText(QString::fromUtf8(u8"a😀́"));
    StartLookup();
    passed = passed && rejected_submissions == 0 &&
             facade_->ExportArticleTabSession() == before_rejected_phrase &&
             status_->text() == QStringLiteral(
                                    "Input phrase exceeds the configured "
                                    "2-symbol limit") &&
             suggestions_list_->count() == 0;
    disconnect(rejected_submission_connection);
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_preferences_callback;
    disconnect(preference_trigger_connection);
    passed = passed && dictionary_paths_ == initial_paths &&
             sound_directories_ == initial_sounds &&
             mediawiki_sources_ == initial_wikis &&
             website_sources_ == initial_websites &&
             forvo_sources_ == initial_forvo &&
             dict_server_sources_ == initial_dicts &&
             external_program_sources_ == initial_programs &&
             facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state &&
             centralWidget() != nullptr && article_tabs_->isVisible() &&
             article_tabs_->size().width() > 0 &&
             article_tabs_->size().height() > 0 && kMainWindowStateVersion == 7;
    completion(passed);
}

void MainWindow::RunHistoryPreferencesSmokeCheck(
    const QString& import_path, std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || history_list_ == nullptr ||
        facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_session = facade_->ExportArticleTabSession();
    bool passed = history_list_->count() == 3;
    const auto apply_history_preferences = [this, &passed](bool store,
                                                           int maximum) {
        preferences_dialog_executor_ = [&passed, store,
                                        maximum](PreferencesDialog& dialog) {
            auto* store_history =
                dialog.findChild<QCheckBox*>(QStringLiteral("storeHistory"));
            auto* maximum_history = dialog.findChild<QSpinBox*>(
                QStringLiteral("historyMaxSizeField"));
            auto* buttons = dialog.findChild<QDialogButtonBox*>(
                QStringLiteral("preferencesButtonBox"));
            passed = passed && store_history != nullptr &&
                     maximum_history != nullptr && buttons != nullptr;
            if (store_history != nullptr)
                store_history->setChecked(store);
            if (maximum_history != nullptr)
                maximum_history->setValue(maximum);
            if (buttons != nullptr)
                buttons->button(QDialogButtonBox::Ok)->click();
            passed = passed && dialog.result() == QDialog::Accepted;
            return dialog.result();
        };
        preferences_action_->trigger();
        preferences_dialog_executor_ = {};
    };

    apply_history_preferences(false, 2);
    passed = passed && !preferences_.store_history &&
             preferences_.maximum_history_entries == 2U &&
             history_list_->count() == 2 &&
             history_list_->item(0)->text() == QStringLiteral("Newest") &&
             history_list_->item(1)->text() == QStringLiteral("Middle");
    emit LookupSubmitted(QStringLiteral("Not recorded"), 9U);
    passed = passed && history_list_->count() == 2 &&
             history_list_->item(0)->text() == QStringLiteral("Newest");

    emit ImportHistoryRequested(import_path, 7U);
    passed = passed && history_list_->count() == 2 &&
             history_list_->item(0)->text() == QStringLiteral("Imported one") &&
             history_list_->item(1)->text() == QStringLiteral("Imported two");

    apply_history_preferences(true, 1);
    emit LookupSubmitted(QStringLiteral("Recorded"), 11U);
    passed = passed && preferences_.store_history &&
             preferences_.maximum_history_entries == 1U &&
             history_list_->count() == 1 &&
             history_list_->item(0)->text() == QStringLiteral("Recorded") &&
             facade_->ExportArticleTabSession() == initial_session;
    completion(passed);
}

void MainWindow::RunPreferencesCoordinatorPredecisionSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    auto* const initial_facade = facade_;
    bool passed = true;
    for (int attempt = 0; attempt < 8; ++attempt) {
        preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
            auto* background = dialog.findChild<QCheckBox*>(
                QStringLiteral("newTabsOpenInBackground"));
            auto* buttons = dialog.findChild<QDialogButtonBox*>(
                QStringLiteral("preferencesButtonBox"));
            passed = passed && background != nullptr && buttons != nullptr;
            if (background != nullptr)
                background->setChecked(!background->isChecked());
            if (buttons != nullptr)
                buttons->button(QDialogButtonBox::Ok)->click();
            auto* error = dialog.findChild<QLabel*>(
                QStringLiteral("preferencesValidationError"));
            passed = passed && dialog.result() != QDialog::Accepted &&
                     error != nullptr && !error->isHidden() &&
                     !error->text().isEmpty();
            dialog.reject();
            return dialog.result();
        };
        preferences_action_->trigger();
        const bool attempt_passed =
            preferences_ == initial_preferences && facade_ == initial_facade &&
            facade_->ExportArticleTabSession() == initial_session &&
            preferences_action_->isEnabled() && !preferences_busy_;
        if (!attempt_passed)
            qWarning() << "Preferences predecision smoke attempt failed"
                       << attempt;
        passed = passed && attempt_passed;
    }
    preferences_dialog_executor_ = {};
    completion(passed);
}

void MainWindow::RunFavoritesPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || favorites_tree_ == nullptr ||
        add_favorite_action_ == nullptr || remove_favorite_action_ == nullptr ||
        facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    bool passed = favorites_tree_->topLevelItemCount() == 0 &&
                  preferences_.confirm_favorites_deletion;

    emit AddFavoriteFolderRequested(QStringLiteral("Preference Folder"), {});
    auto* folder = favorites_tree_->topLevelItem(0);
    passed = passed && folder != nullptr && folder->childCount() == 0;
    if (folder == nullptr) {
        completion(false);
        return;
    }
    favorites_tree_->setCurrentItem(folder);
    folder->setExpanded(true);
    query_->setText(QStringLiteral("Preference Favorite"));
    add_favorite_action_->trigger();
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && folder != nullptr && folder->childCount() == 1 &&
             folder->isExpanded();

    int confirmations = 0;
    favorite_removal_confirmation_ = [&confirmations]() {
        ++confirmations;
        return false;
    };
    favorites_tree_->setCurrentItem(folder->child(0));
    remove_favorite_action_->trigger();
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && confirmations == 1 && folder != nullptr &&
             folder->childCount() == 1 &&
             favorites_tree_->currentItem() == folder->child(0);

    const auto apply_confirmation = [this, &passed](bool enabled) {
        preferences_dialog_executor_ = [&passed,
                                        enabled](PreferencesDialog& dialog) {
            auto* group =
                dialog.findChild<QGroupBox*>(QStringLiteral("favoritesBox"));
            auto* confirmation = dialog.findChild<QCheckBox*>(
                QStringLiteral("confirmFavoritesDeletion"));
            auto* interval = dialog.findChild<QSpinBox*>(
                QStringLiteral("favoritesSaveIntervalField"));
            auto* buttons = dialog.findChild<QDialogButtonBox*>(
                QStringLiteral("preferencesButtonBox"));
            passed =
                passed && group != nullptr && confirmation != nullptr &&
                interval == nullptr && buttons != nullptr &&
                confirmation->text() ==
                    QStringLiteral("Confirmation for items deletion") &&
                confirmation->toolTip() ==
                    QStringLiteral(
                        "Turn this option on to confirm every operation of "
                        "items deletion");
            if (confirmation != nullptr)
                confirmation->setChecked(enabled);
            if (buttons != nullptr)
                buttons->button(QDialogButtonBox::Ok)->click();
            passed = passed && dialog.result() == QDialog::Accepted;
            return dialog.result();
        };
        preferences_action_->trigger();
        preferences_dialog_executor_ = {};
    };

    apply_confirmation(false);
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && !preferences_.confirm_favorites_deletion &&
             folder != nullptr && folder->isExpanded() &&
             favorites_tree_->currentItem() == folder->child(0);
    remove_favorite_action_->trigger();
    folder = favorites_tree_->topLevelItem(0);
    passed = passed && confirmations == 1 && folder != nullptr &&
             folder->childCount() == 0;

    apply_confirmation(true);
    folder = favorites_tree_->topLevelItem(0);
    favorites_tree_->setCurrentItem(folder);
    query_->setText(QStringLiteral("Subtree Favorite"));
    add_favorite_action_->trigger();
    folder = favorites_tree_->topLevelItem(0);
    favorites_tree_->setCurrentItem(folder);
    remove_favorite_action_->trigger();
    passed = passed && confirmations == 2 &&
             favorites_tree_->topLevelItemCount() == 1;

    favorite_removal_confirmation_ = [&confirmations]() {
        ++confirmations;
        return true;
    };
    remove_favorite_action_->trigger();
    favorite_removal_confirmation_ = {};
    passed = passed && confirmations == 3 &&
             favorites_tree_->topLevelItemCount() == 0 &&
             facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state;
    completion(passed);
}

void MainWindow::RunArticlesPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    bool passed = true;

    preferences_dialog_executor_ = [&passed, initial_preferences](
                                       PreferencesDialog& dialog) {
        auto* group = dialog.findChild<QGroupBox*>(
            QStringLiteral("preferencesArticlesGroup"));
        auto* collapse =
            dialog.findChild<QCheckBox*>(QStringLiteral("collapseBigArticles"));
        auto* limit =
            dialog.findChild<QSpinBox*>(QStringLiteral("articleSizeLimit"));
        auto* label =
            dialog.findChild<QLabel*>(QStringLiteral("articleSizeLimitLabel"));
        auto* ignore =
            dialog.findChild<QCheckBox*>(QStringLiteral("ignoreDiacritics"));
        passed =
            passed && group != nullptr && collapse != nullptr &&
            limit != nullptr && label != nullptr && ignore != nullptr &&
            group->title() == QStringLiteral("Articles") &&
            collapse->text() == QStringLiteral("Collapse articles more than") &&
            collapse->toolTip() ==
                QStringLiteral(
                    "Select this option to automatic collapse big articles") &&
            collapse->isChecked() ==
                initial_preferences.collapse_large_articles &&
            limit->minimum() == 1 && limit->maximum() == 100000 &&
            limit->singleStep() == 50 &&
            limit->value() ==
                static_cast<int>(initial_preferences.article_size_limit) &&
            limit->isEnabled() == initial_preferences.collapse_large_articles &&
            label->text() == QStringLiteral("symbols") &&
            ignore->text() == QStringLiteral("Ignore diacritics") &&
            ignore->toolTip() ==
                QStringLiteral(
                    "Turn this option on to ignore diacritics while searching "
                    "articles") &&
            ignore->isChecked() == initial_preferences.ignore_diacritics &&
            dialog.findChild<QWidget*>(QStringLiteral("displayStyle")) ==
                nullptr;
        collapse->setChecked(true);
        ignore->setChecked(true);
        limit->setValue(3450);
        passed = passed && limit->isEnabled();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences &&
             facade_->ExportArticleTabSession() == initial_session;

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced article preferences failure");
    };
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* collapse =
            dialog.findChild<QCheckBox*>(QStringLiteral("collapseBigArticles"));
        auto* limit =
            dialog.findChild<QSpinBox*>(QStringLiteral("articleSizeLimit"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        auto* ignore =
            dialog.findChild<QCheckBox*>(QStringLiteral("ignoreDiacritics"));
        passed = passed && collapse != nullptr && limit != nullptr &&
                 ignore != nullptr && buttons != nullptr;
        collapse->setChecked(true);
        ignore->setChecked(true);
        limit->setValue(3450);
        buttons->button(QDialogButtonBox::Ok)->click();
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences &&
             facade_->ExportArticleTabSession() == initial_session;

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* collapse =
            dialog.findChild<QCheckBox*>(QStringLiteral("collapseBigArticles"));
        auto* limit =
            dialog.findChild<QSpinBox*>(QStringLiteral("articleSizeLimit"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        auto* ignore =
            dialog.findChild<QCheckBox*>(QStringLiteral("ignoreDiacritics"));
        passed = passed && collapse != nullptr && limit != nullptr &&
                 ignore != nullptr && buttons != nullptr;
        collapse->setChecked(true);
        ignore->setChecked(true);
        limit->setValue(3450);
        buttons->button(QDialogButtonBox::Ok)->click();
        passed = passed && dialog.result() == QDialog::Accepted;
        return dialog.result();
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    passed = passed && preferences_.collapse_large_articles &&
             preferences_.ignore_diacritics &&
             preferences_.article_size_limit == 3450U &&
             facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state &&
             centralWidget() != nullptr && article_tabs_->isVisible();
    completion(passed);
}

void MainWindow::RunDictionaryContextPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    bool passed = true;
    const auto inspect = [&passed, initial_preferences](
                             PreferencesDialog& dialog, int value,
                             bool accept) {
        auto* label = dialog.findChild<QLabel*>(
            QStringLiteral("maxDictsInContextMenuLabel"));
        auto* limit = dialog.findChild<QSpinBox*>(
            QStringLiteral("maxDictsInContextMenu"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && label != nullptr && limit != nullptr &&
                 buttons != nullptr &&
                 label->text() ==
                     QStringLiteral("Context menu dictionaries limit:") &&
                 label->toolTip() ==
                     QStringLiteral(
                         "Adjust this value to avoid huge context menus.") &&
                 limit->minimum() == 0 && limit->maximum() == 9999 &&
                 limit->singleStep() == 1 &&
                 limit->value() ==
                     static_cast<int>(
                         initial_preferences.maximum_dictionary_references);
        if (limit != nullptr)
            limit->setValue(value);
        if (accept && buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        else
            dialog.reject();
        return dialog.result();
    };

    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, 0, false);
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences;

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced dictionary context preference failure");
    };
    preferences_dialog_executor_ = [&inspect,
                                    &passed](PreferencesDialog& dialog) {
        const int result = inspect(dialog, 0, true);
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && result != QDialog::Accepted && error != nullptr &&
                 !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences;

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, 0, true);
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    passed = passed && preferences_.maximum_dictionary_references == 0U &&
             facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state;
    completion(passed);
}

void MainWindow::RunSynonymPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    bool passed = true;

    preferences_dialog_executor_ = [&passed, initial_preferences](
                                       PreferencesDialog& dialog) {
        auto* checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("synonymSearchEnabled"));
        passed =
            passed && checkbox != nullptr &&
            checkbox->text() == QStringLiteral("Extra search via synonyms") &&
            checkbox->toolTip() ==
                QStringLiteral(
                    "Turn this option on to enable extra articles search "
                    "via synonym lists from Stardict, Babylon and GLS "
                    "dictionaries") &&
            checkbox->isChecked() == initial_preferences.synonym_search_enabled;
        if (checkbox != nullptr)
            checkbox->setChecked(!initial_preferences.synonym_search_enabled);
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences;

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced synonym preferences failure");
    };
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("synonymSearchEnabled"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && checkbox != nullptr && buttons != nullptr;
        if (checkbox != nullptr)
            checkbox->setChecked(false);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences;

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("synonymSearchEnabled"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && checkbox != nullptr && buttons != nullptr;
        if (checkbox != nullptr)
            checkbox->setChecked(false);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        passed = passed && dialog.result() == QDialog::Accepted;
        return dialog.result();
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    passed = passed && !preferences_.synonym_search_enabled &&
             facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state;
    completion(passed);
}

void MainWindow::RunOptionalPartsPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    bool passed = true;
    const auto inspect = [&passed](PreferencesDialog& dialog, bool checked,
                                   bool new_value, bool accept) {
        auto* checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("alwaysExpandOptionalParts"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed =
            passed && checkbox != nullptr && buttons != nullptr &&
            checkbox->text() == QStringLiteral("Expand optional &parts") &&
            checkbox->toolTip() == QStringLiteral(
                                       "Turn this option on to always expand "
                                       "optional parts of articles") &&
            checkbox->isChecked() == checked;
        if (checkbox != nullptr)
            checkbox->setChecked(new_value);
        if (accept && buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        else
            dialog.reject();
        return dialog.result();
    };

    preferences_dialog_executor_ = [&inspect, initial_preferences](
                                       PreferencesDialog& dialog) {
        return inspect(dialog, initial_preferences.always_expand_optional_parts,
                       !initial_preferences.always_expand_optional_parts,
                       false);
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences;

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced optional parts preferences failure");
    };
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("alwaysExpandOptionalParts"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && checkbox != nullptr && buttons != nullptr;
        if (checkbox != nullptr)
            checkbox->setChecked(true);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences;

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ = [&inspect, initial_preferences](
                                       PreferencesDialog& dialog) {
        return inspect(dialog, initial_preferences.always_expand_optional_parts,
                       true, true);
    };
    preferences_action_->trigger();
    passed = passed && preferences_.always_expand_optional_parts;

    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, true, true, false);
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    passed = passed && preferences_.always_expand_optional_parts &&
             facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state &&
             centralWidget() != nullptr && article_tabs_->isVisible();
    completion(passed);
}

void MainWindow::RunProxyPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial = preferences_;
    const auto session = facade_->ExportArticleTabSession();
    const std::string state = CaptureMainWindowState();
    bool passed = initial.proxy_mode == goldendict::core::ProxyMode::kDisabled;
    const auto inspect = [&passed](PreferencesDialog& dialog, bool accept) {
        auto* group =
            dialog.findChild<QGroupBox*>(QStringLiteral("useProxyServer"));
        auto* type = dialog.findChild<QComboBox*>(QStringLiteral("proxyType"));
        auto* host = dialog.findChild<QLineEdit*>(QStringLiteral("proxyHost"));
        auto* port = dialog.findChild<QSpinBox*>(QStringLiteral("proxyPort"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed =
            passed && group != nullptr && type != nullptr && host != nullptr &&
            port != nullptr && buttons != nullptr &&
            group->title() == QStringLiteral("Use proxy server") &&
            group->toolTip() == QStringLiteral(
                                    "Enable if you wish to use a proxy server\n"
                                    "for all program's network requests.") &&
            type->count() == 1 &&
            type->currentText() == QStringLiteral("HTTP Transp.");
        if (group != nullptr)
            group->setChecked(true);
        if (host != nullptr)
            host->setText(QStringLiteral("proxy.example"));
        if (port != nullptr)
            port->setValue(3128);
        if (accept && buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        else
            dialog.reject();
        return dialog.result();
    };
    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, false);
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial;

    const auto callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced proxy recomposition failure");
    };
    preferences_dialog_executor_ = [&inspect,
                                    &passed](PreferencesDialog& dialog) {
        inspect(dialog, true);
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial;

    preferences_apply_callback_ = callback;
    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, true);
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = callback;
    passed =
        passed &&
        preferences_.proxy_mode == goldendict::core::ProxyMode::kManual &&
        preferences_.proxy_type == goldendict::core::ProxyType::kHttpConnect &&
        preferences_.proxy_host == "proxy.example" &&
        preferences_.proxy_port == 3128U &&
        facade_->ExportArticleTabSession() == session &&
        CaptureMainWindowState() == state;
    completion(passed);
}

void MainWindow::RunProxyPreferencesRestartSmokeCheck(
    std::function<void(bool)> completion) {
    completion(
        preferences_.proxy_mode == goldendict::core::ProxyMode::kManual &&
        preferences_.proxy_type == goldendict::core::ProxyType::kHttpConnect &&
        preferences_.proxy_host == "proxy.example" &&
        preferences_.proxy_port == 3128U);
}

void MainWindow::RunNetworkCachePreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial = preferences_;
    const auto session = facade_->ExportArticleTabSession();
    const std::string state = CaptureMainWindowState();
    bool passed = initial.maximum_network_cache_megabytes == 50U &&
                  initial.clear_network_cache_on_exit;
    const auto inspect = [this, &passed](PreferencesDialog& dialog,
                                         bool accept) {
        auto* label =
            dialog.findChild<QLabel*>(QStringLiteral("networkCacheSizeLabel"));
        auto* size =
            dialog.findChild<QSpinBox*>(QStringLiteral("maxNetworkCacheSize"));
        auto* clear = dialog.findChild<QCheckBox*>(
            QStringLiteral("clearNetworkCacheOnExit"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed =
            passed && label != nullptr && size != nullptr && clear != nullptr &&
            buttons != nullptr &&
            label->text() == QStringLiteral("Maximum network cache size:") &&
            size->minimum() == 0 && size->maximum() == 2000 &&
            size->value() == 50 &&
#ifdef Q_OS_WIN
            size->suffix() == QStringLiteral(" MB") &&
#else
            size->suffix() == QStringLiteral(" MiB") &&
#endif
            size->toolTip() ==
                QStringLiteral(
                    "Maximum disk space occupied by GoldenDict's network "
                    "cache in\n%1\nIf set to 0 the network disk cache will "
                    "be disabled.")
                    .arg(network_cache_directory_) &&
            clear->text() == QStringLiteral("Clear network cache on exit") &&
            clear->toolTip() ==
                QStringLiteral(
                    "When this option is enabled, GoldenDict\nclears its "
                    "network cache from disk during exit.");
        if (size != nullptr) {
            size->setValue(0);
            passed = passed && clear != nullptr && !clear->isEnabled();
            size->setValue(64);
            passed = passed && clear != nullptr && clear->isEnabled();
        }
        if (clear != nullptr)
            clear->setChecked(false);
        if (accept && buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        else
            dialog.reject();
        return dialog.result();
    };
    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, false);
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial;

    const auto callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced network cache apply failure");
    };
    preferences_dialog_executor_ = [&inspect,
                                    &passed](PreferencesDialog& dialog) {
        inspect(dialog, true);
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial;

    preferences_apply_callback_ = callback;
    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, true);
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = callback;
    passed = passed && preferences_.maximum_network_cache_megabytes == 64U &&
             !preferences_.clear_network_cache_on_exit &&
             facade_->ExportArticleTabSession() == session &&
             CaptureMainWindowState() == state;
    completion(passed);
}

void MainWindow::RunNetworkCachePreferencesRestartSmokeCheck(
    std::function<void(bool)> completion) {
    bool passed = preferences_.maximum_network_cache_megabytes == 64U &&
                  !preferences_.clear_network_cache_on_exit;
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* size =
            dialog.findChild<QSpinBox*>(QStringLiteral("maxNetworkCacheSize"));
        auto* clear = dialog.findChild<QCheckBox*>(
            QStringLiteral("clearNetworkCacheOnExit"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && size != nullptr && clear != nullptr &&
                 buttons != nullptr && size->value() == 64 &&
                 !clear->isChecked();
        if (size != nullptr)
            size->setValue(32);
        if (clear != nullptr)
            clear->setChecked(true);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        return dialog.result();
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    passed = passed && preferences_.maximum_network_cache_megabytes == 32U &&
             preferences_.clear_network_cache_on_exit;
    completion(passed);
}

void MainWindow::RunHideSingleTabPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr ||
        article_tabs_->count() != 1) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    bool passed = article_tabs_->tabBar()->isVisible() ==
                  !initial_preferences.hide_single_tab;
    const auto inspect = [&passed](PreferencesDialog& dialog, bool checked,
                                   bool accept) {
        auto* checkbox =
            dialog.findChild<QCheckBox*>(QStringLiteral("hideSingleTab"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && checkbox != nullptr && buttons != nullptr &&
                 checkbox->text() == QStringLiteral("Hide single tab") &&
                 checkbox->toolTip() ==
                     QStringLiteral(
                         "Select this option if you don't want to see the main "
                         "tab bar when only a single tab is opened.") &&
                 checkbox->isChecked() == checked;
        if (checkbox != nullptr)
            checkbox->setChecked(accept || !checked);
        if (accept && buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        else
            dialog.reject();
        return dialog.result();
    };

    preferences_dialog_executor_ =
        [&inspect, initial_preferences](PreferencesDialog& dialog) {
            return inspect(dialog, initial_preferences.hide_single_tab, false);
        };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences &&
             article_tabs_->tabBar()->isVisible() ==
                 !initial_preferences.hide_single_tab;

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced hide single tab preferences failure");
    };
    preferences_dialog_executor_ =
        [&passed, initial_preferences](PreferencesDialog& dialog) {
            auto* checkbox =
                dialog.findChild<QCheckBox*>(QStringLiteral("hideSingleTab"));
            auto* buttons = dialog.findChild<QDialogButtonBox*>(
                QStringLiteral("preferencesButtonBox"));
            passed = passed && checkbox != nullptr && buttons != nullptr;
            if (checkbox != nullptr)
                checkbox->setChecked(!initial_preferences.hide_single_tab);
            if (buttons != nullptr)
                buttons->button(QDialogButtonBox::Ok)->click();
            auto* error = dialog.findChild<QLabel*>(
                QStringLiteral("preferencesValidationError"));
            passed = passed && dialog.result() != QDialog::Accepted &&
                     error != nullptr && !error->isHidden();
            dialog.reject();
            return dialog.result();
        };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences &&
             article_tabs_->tabBar()->isVisible() ==
                 !initial_preferences.hide_single_tab;

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ =
        [&inspect, initial_preferences](PreferencesDialog& dialog) {
            return inspect(dialog, initial_preferences.hide_single_tab, true);
        };
    preferences_action_->trigger();
    passed = passed && preferences_.hide_single_tab &&
             !article_tabs_->tabBar()->isVisible();
    CreateEmptyArticleTab(false);
    passed = passed && article_tabs_->count() == 2 &&
             article_tabs_->tabBar()->isVisible();
    CloseArticleTab(1);
    passed = passed && article_tabs_->count() == 1 &&
             !article_tabs_->tabBar()->isVisible();

    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    passed = passed && facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state;
    completion(passed);
}

void MainWindow::RunHideSingleTabRestartSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr || article_tabs_->count() != 1 ||
        !preferences_.hide_single_tab || article_tabs_->tabBar()->isVisible()) {
        completion(false);
        return;
    }
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    CreateEmptyArticleTab(false);
    bool passed =
        article_tabs_->count() == 2 && article_tabs_->tabBar()->isVisible();
    CloseArticleTab(1);
    passed = passed && article_tabs_->count() == 1 &&
             !article_tabs_->tabBar()->isVisible() &&
             facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state;
    completion(passed);
}

void MainWindow::RunEscapeHidesMainWindowPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr ||
        query_ == nullptr || article_tabs_->currentWidget() == nullptr) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    bool passed = !initial_preferences.escape_hides_main_window;
    const auto send_escape = [](QWidget* target) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(target, &press);
        QApplication::processEvents();
    };
    const auto show_window = [this]() {
        show();
        raise();
        activateWindow();
        QApplication::processEvents();
    };

    show_window();
    query_->setFocus();
    send_escape(query_);
    passed = passed && isVisible();

    EscapeConsumer consumer;
    query_->installEventFilter(&consumer);
    send_escape(query_);
    query_->removeEventFilter(&consumer);
    passed = passed && consumer.consumed && isVisible();

    QDialog modal(this);
    modal.setModal(true);
    modal.show();
    QApplication::processEvents();
    send_escape(&modal);
    passed = passed && modal.result() == QDialog::Rejected && isVisible();

    const auto inspect = [&passed](PreferencesDialog& dialog, bool checked,
                                   bool accept) {
        auto* checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("escKeyHidesMainWindow"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed =
            passed && checkbox != nullptr && buttons != nullptr &&
            checkbox->text() == QStringLiteral("ESC key hides main window") &&
            checkbox->toolTip() ==
                QStringLiteral(
                    "Normally, pressing ESC key moves focus to the "
                    "translation line.\nWith this on however, it will "
                    "hide the main window.") &&
            checkbox->isChecked() == checked;
        if (checkbox != nullptr)
            checkbox->setChecked(!checked);
        if (accept && buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        else
            dialog.reject();
        return dialog.result();
    };

    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, false, false);
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences && isVisible();

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced ESC preference failure");
    };
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* checkbox = dialog.findChild<QCheckBox*>(
            QStringLiteral("escKeyHidesMainWindow"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && checkbox != nullptr && buttons != nullptr;
        if (checkbox != nullptr)
            checkbox->setChecked(true);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == initial_preferences && isVisible();

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ = [&inspect](PreferencesDialog& dialog) {
        return inspect(dialog, false, true);
    };
    preferences_action_->trigger();
    passed = passed && preferences_.escape_hides_main_window && isVisible();

    query_->setFocus();
    send_escape(query_);
    passed = passed && !isVisible();
    ActivateFromSingleInstanceLookup();
    QApplication::processEvents();
    passed = passed && isVisible() && !isMinimized();

    setWindowState(Qt::WindowMaximized | Qt::WindowMinimized);
    QApplication::processEvents();
    ActivateFromSingleInstanceLookup();
    QApplication::processEvents();
    passed = passed && isVisible() && !isMinimized() && isMaximized();
    showNormal();
    show_window();
    auto* article = qobject_cast<QWidget*>(article_tabs_->currentWidget());
    article->setFocus();
    send_escape(article);
    passed = passed && !isVisible();
    show_window();

    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    passed = passed && facade_->ExportArticleTabSession() == initial_session &&
             CaptureMainWindowState() == initial_state;
    completion(passed);
}

void MainWindow::RunEscapeHidesMainWindowRestartSmokeCheck(
    std::function<void(bool)> completion) {
    if (!preferences_.escape_hides_main_window || query_ == nullptr ||
        article_tabs_->currentWidget() == nullptr) {
        completion(false);
        return;
    }
    const auto send_escape = [](QWidget* target) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(target, &press);
        QApplication::processEvents();
    };
    query_->setFocus();
    send_escape(query_);
    bool passed = !isVisible();
    show();
    QApplication::processEvents();
    auto* article = qobject_cast<QWidget*>(article_tabs_->currentWidget());
    article->setFocus();
    send_escape(article);
    passed = passed && !isVisible();
    completion(passed);
}

void MainWindow::RunArticleClickPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    auto* view = qobject_cast<ArticleView*>(article_tabs_->currentWidget());
    if (preferences_action_ == nullptr || facade_ == nullptr ||
        view == nullptr) {
        completion(false);
        return;
    }
    const auto initial_preferences = preferences_;
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    auto passed = std::make_shared<bool>(
        initial_preferences.double_click_translates &&
        !initial_preferences.select_word_by_single_click);
    const auto inspect = [passed](PreferencesDialog& dialog, bool accept) {
        auto* translate = dialog.findChild<QCheckBox*>(
            QStringLiteral("doubleClickTranslates"));
        auto* select =
            dialog.findChild<QCheckBox*>(QStringLiteral("selectBySingleClick"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        *passed =
            *passed && translate != nullptr && select != nullptr &&
            buttons != nullptr &&
            translate->text() ==
                QStringLiteral("Double-click translates the word clicked") &&
            translate->toolTip().isEmpty() &&
            select->text() == QStringLiteral("Select word by single click") &&
            select->toolTip() ==
                QStringLiteral(
                    "Turn this option on if you want to select words by "
                    "single mouse click");
        if (translate != nullptr)
            translate->setChecked(true);
        if (select != nullptr)
            select->setChecked(true);
        if (accept && buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        else
            dialog.reject();
        return dialog.result();
    };
    preferences_dialog_executor_ = [inspect](PreferencesDialog& dialog) {
        return inspect(dialog, false);
    };
    preferences_action_->trigger();
    *passed = *passed && preferences_ == initial_preferences;

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced article click preferences failure");
    };
    preferences_dialog_executor_ = [passed](PreferencesDialog& dialog) {
        auto* translate = dialog.findChild<QCheckBox*>(
            QStringLiteral("doubleClickTranslates"));
        auto* select =
            dialog.findChild<QCheckBox*>(QStringLiteral("selectBySingleClick"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        if (translate != nullptr)
            translate->setChecked(false);
        if (select != nullptr)
            select->setChecked(true);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        *passed = *passed && dialog.result() != QDialog::Accepted &&
                  error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    *passed = *passed && preferences_ == initial_preferences;

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ = [inspect](PreferencesDialog& dialog) {
        return inspect(dialog, true);
    };
    preferences_action_->trigger();
    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    *passed = *passed && preferences_.double_click_translates &&
              preferences_.select_word_by_single_click;
    view = new ArticleView(this);
    view->resize(640, 480);
    view->show();

    auto lookup_count = std::make_shared<int>(0);
    connect(view, &ArticleView::SelectionLookupRequested, view,
            [lookup_count](const QString& word, ArticleLinkDisposition) {
                if (word == QStringLiteral("alpha"))
                    ++*lookup_count;
            });
    connect(
        view, &ArticleView::loadFinished, this,
        [this, view, passed, lookup_count, initial_session, initial_state,
         completion = std::move(completion)](bool success) mutable {
            if (!success) {
                completion(false);
                return;
            }
            struct Case {
                bool translate_enabled;
                bool select_enabled;
                QString target;
                bool translate;
                bool expect_selection;
                int expected_lookups;
            };
            auto cases = std::make_shared<QList<Case>>(QList<Case>{
                {false, false, QStringLiteral("word"), false, false, 0},
                {false, false, QStringLiteral("word"), true, false, 0},
                {true, false, QStringLiteral("word"), false, false, 0},
                {true, false, QStringLiteral("word"), true, true, 1},
                {false, true, QStringLiteral("word"), false, true, 1},
                {false, true, QStringLiteral("word"), true, false, 1},
                {true, true, QStringLiteral("word"), false, true, 1},
                {true, true, QStringLiteral("word"), true, true, 2},
                {true, true, QStringLiteral("link"), false, false, 2},
                {true, true, QStringLiteral("link"), true, false, 2},
                {true, true, QStringLiteral("input"), false, false, 2},
                {true, true, QStringLiteral("input"), true, false, 2},
                {true, true, QStringLiteral("long"), true, false, 2},
            });
            auto index = std::make_shared<int>(0);
            auto run = std::make_shared<std::function<void()>>();
            *run = [this, view, passed, lookup_count, cases, index, run,
                    initial_session, initial_state,
                    completion = std::move(completion)]() mutable {
                if (*index >= cases->size()) {
                    view->SetClickPreferences(true, true);
                    view->page()->runJavaScript(
                        QStringLiteral(
                            "(() => {const r=document.getElementById('word')"
                            ".getBoundingClientRect(); return "
                            "[r.left+r.width/2,r.top+r.height/2];})()"),
                        [this, view, passed, lookup_count, initial_session,
                         initial_state, completion = std::move(completion)](
                            const QVariant& value) mutable {
                            const QVariantList point = value.toList();
                            if (point.size() != 2) {
                                view->deleteLater();
                                completion(false);
                                return;
                            }
                            view->TriggerWordQueryForTest(
                                QPointF(point[0].toDouble(),
                                        point[1].toDouble()),
                                true);
                            view->setPage(new QWebEnginePage(view));
                            QTimer::singleShot(
                                50, view,
                                [this, view, passed, lookup_count,
                                 initial_session, initial_state,
                                 completion = std::move(completion)]() mutable {
                                    *passed = *passed && *lookup_count == 2;
                                    SetPreferences(preferences_);
                                    *passed =
                                        *passed &&
                                        facade_->ExportArticleTabSession() ==
                                            initial_session &&
                                        CaptureMainWindowState() ==
                                            initial_state;
                                    view->deleteLater();
                                    completion(*passed);
                                });
                        });
                    return;
                }
                const Case current = cases->at((*index)++);
                view->SetClickPreferences(current.translate_enabled,
                                          current.select_enabled);
                view->page()->runJavaScript(
                    QStringLiteral(
                        "(() => {window.getSelection().removeAllRanges();"
                        "const r=document.getElementById('%1')"
                        ".getBoundingClientRect(); return [r.left+r.width/2,"
                        "r.top+r.height/2];})()")
                        .arg(current.target),
                    [view, passed, lookup_count, current,
                     run](const QVariant& value) {
                        const QVariantList point = value.toList();
                        if (point.size() != 2) {
                            *passed = false;
                            (*run)();
                            return;
                        }
                        view->TriggerWordQueryForTest(
                            QPointF(point[0].toDouble(), point[1].toDouble()),
                            current.translate);
                        if (current.translate && current.translate_enabled &&
                            current.select_enabled &&
                            current.target == QStringLiteral("word")) {
                            view->TriggerWordQueryForTest(
                                QPointF(point[0].toDouble(),
                                        point[1].toDouble()),
                                true);
                        }
                        QTimer::singleShot(
                            50, view,
                            [view, passed, lookup_count, current, run]() {
                                const bool selected =
                                    view->page()->selectedText() ==
                                    QStringLiteral("alpha");
                                *passed =
                                    *passed &&
                                    selected == current.expect_selection &&
                                    *lookup_count == current.expected_lookups;
                                (*run)();
                            });
                    });
            };
            (*run)();
        },
        Qt::SingleShotConnection);
    view->setHtml(QStringLiteral(
        "<!doctype html><html><body>"
        "<span id='word'>alpha</span> "
        "<a id='link' href='goldendict://lookup/beta'><span>beta</span></a> "
        "<input id='input' value='gamma'>"
        "<span id='long'>abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
        "abcdefgh</span>"
        "</body></html>"));
}

void MainWindow::RunArticleClickRestartSmokeCheck(
    std::function<void(bool)> completion) {
    completion(preferences_.double_click_translates &&
               preferences_.select_word_by_single_click);
}

void MainWindow::RunMruTabOrderPreferencesSmokeCheck(
    std::function<void(bool)> completion) {
    if (preferences_action_ == nullptr || facade_ == nullptr ||
        query_ == nullptr) {
        completion(false);
        return;
    }
    goldendict::core::TabNavigationState empty;
    empty.title = "(untitled)";
    const goldendict::core::ArticleTabSession initial_session = {
        {{10U, {empty}, 0U}, {20U, {empty}, 0U}, {30U, {empty}, 0U}}, 10U};
    if (!facade_->RestoreArticleTabSession(initial_session)) {
        completion(false);
        return;
    }
    ReconcileMruTabIds(true);
    SyncArticleTabs();
    bool passed = mru_tab_ids_ ==
                  (std::vector<goldendict::core::ArticleTabId>{10U, 20U, 30U});
    const std::string initial_state = CaptureMainWindowState();
    const auto send_key = [this](int key, Qt::KeyboardModifiers modifiers,
                                 QEvent::Type type = QEvent::KeyPress) {
        QKeyEvent event(type, key, modifiers);
        QApplication::sendEvent(query_, &event);
    };
    auto active_id = [this]() {
        return facade_->GetArticleTabsState().active_tab_id;
    };

    auto disabled = preferences_;
    disabled.mru_tab_order = false;
    SetPreferences(disabled);
    send_key(Qt::Key_Tab, Qt::ControlModifier);
    passed = passed && active_id() == 20U;
    send_key(Qt::Key_Control, Qt::NoModifier, QEvent::KeyRelease);
    send_key(Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier);
    passed = passed && active_id() == 10U;
    send_key(Qt::Key_Control, Qt::NoModifier, QEvent::KeyRelease);

    const auto preferences_before_dialog = preferences_;
    const auto session_before_dialog = facade_->ExportArticleTabSession();
    const auto mru_before_dialog = mru_tab_ids_;

    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* checkbox =
            dialog.findChild<QCheckBox*>(QStringLiteral("mruTabOrder"));
        passed = passed && checkbox != nullptr &&
                 checkbox->text() ==
                     QStringLiteral("Ctrl-Tab navigates tabs in MRU order") &&
                 checkbox->toolTip().isEmpty() && !checkbox->isChecked();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == preferences_before_dialog &&
             facade_->ExportArticleTabSession() == session_before_dialog &&
             mru_tab_ids_ == mru_before_dialog;

    const auto original_callback = preferences_apply_callback_;
    preferences_apply_callback_ = [](const auto&) {
        return QStringLiteral("forced MRU preferences failure");
    };
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* checkbox =
            dialog.findChild<QCheckBox*>(QStringLiteral("mruTabOrder"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && checkbox != nullptr && buttons != nullptr;
        if (checkbox != nullptr)
            checkbox->setChecked(true);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        auto* error = dialog.findChild<QLabel*>(
            QStringLiteral("preferencesValidationError"));
        passed = passed && dialog.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        dialog.reject();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_ == preferences_before_dialog &&
             facade_->ExportArticleTabSession() == session_before_dialog &&
             mru_tab_ids_ == mru_before_dialog;

    preferences_apply_callback_ = original_callback;
    preferences_dialog_executor_ = [&passed](PreferencesDialog& dialog) {
        auto* checkbox =
            dialog.findChild<QCheckBox*>(QStringLiteral("mruTabOrder"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>(
            QStringLiteral("preferencesButtonBox"));
        passed = passed && checkbox != nullptr && buttons != nullptr;
        if (checkbox != nullptr)
            checkbox->setChecked(true);
        if (buttons != nullptr)
            buttons->button(QDialogButtonBox::Ok)->click();
        return dialog.result();
    };
    preferences_action_->trigger();
    passed = passed && preferences_.mru_tab_order &&
             facade_->ExportArticleTabSession() == session_before_dialog;

    ActivateArticleTab(2);
    ActivateArticleTab(0);
    passed = passed && active_id() == 10U;

    send_key(Qt::Key_Tab, Qt::ControlModifier);
    passed = passed && active_id() == 30U;
    send_key(Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier);
    passed = passed && active_id() == 10U;
    send_key(Qt::Key_Tab, Qt::ControlModifier);
    passed = passed && active_id() == 30U;
    send_key(Qt::Key_Control, Qt::NoModifier, QEvent::KeyRelease);

    CloseArticleTab(0);
    CreateEmptyArticleTab(false);
    const auto changed = facade_->GetArticleTabsState();
    passed = passed && changed.tabs.size() == 3U && changed.tabs[0].id == 20U &&
             changed.tabs[1].id == 30U && changed.active_tab_id == 30U;
    send_key(Qt::Key_Tab, Qt::ControlModifier);
    send_key(Qt::Key_Control, Qt::NoModifier, QEvent::KeyRelease);
    passed = passed && active_id() == 20U;

    preferences_dialog_executor_ = {};
    preferences_apply_callback_ = original_callback;
    passed = passed && CaptureMainWindowState() == initial_state;
    completion(passed);
}

void MainWindow::RunMruTabOrderRestartSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr || !preferences_.mru_tab_order) {
        completion(false);
        return;
    }
    const auto session = facade_->ExportArticleTabSession();
    ReconcileMruTabIds(true);
    SyncArticleTabs();
    bool passed = facade_->ExportArticleTabSession() == session &&
                  mru_tab_ids_.size() == session.tabs.size() &&
                  !mru_tab_ids_.empty() &&
                  mru_tab_ids_.front() == session.active_tab_id;
    TraverseArticleTabs(true);
    FinishMruTraversal();
    passed = passed && facade_->ExportArticleTabSession().tabs == session.tabs;
    completion(passed);
}

void MainWindow::RunSearchMenuSmokeCheck(std::function<void(bool)> completion) {
    auto* search_menu = findChild<QMenu*>(QStringLiteral("menuSearch"));
    if (search_menu == nullptr || search_in_page_action_ == nullptr ||
        article_search_ == nullptr || article_search_status_ == nullptr ||
        facade_ == nullptr || article_view_ == nullptr) {
        completion(false);
        return;
    }

    const auto actions = search_menu->actions();
    const auto all_actions = findChildren<QAction*>();
    auto passed = std::make_shared<bool>(
        menuBar()->actions().size() == 7 &&
        menuBar()->actions()[0]->menu()->objectName() ==
            QStringLiteral("menuFile") &&
        menuBar()->actions()[1]->menu()->objectName() ==
            QStringLiteral("menuView") &&
        menuBar()->actions()[2]->menu()->objectName() ==
            QStringLiteral("menu_Edit") &&
        menuBar()->actions()[3]->menu() == search_menu &&
        menuBar()->actions()[4]->menu()->objectName() ==
            QStringLiteral("menuHistory") &&
        findChildren<QMenu*>(QStringLiteral("menuSearch")).size() == 1 &&
        search_menu->title() == QStringLiteral("Search") &&
        actions.size() == 2 && actions[0] == search_in_page_action_ &&
        actions[1] == full_text_search_action_ && !actions[0]->isSeparator() &&
        search_in_page_action_->objectName() ==
            QStringLiteral("searchInPageAction") &&
        search_in_page_action_->text() == QStringLiteral("Search in page") &&
        search_in_page_action_->shortcut() == QKeySequence::Find &&
        search_in_page_action_->shortcutContext() == Qt::WindowShortcut &&
        search_in_page_action_->menuRole() == QAction::TextHeuristicRole &&
        search_in_page_action_->isEnabled() &&
        full_text_search_action_->objectName() ==
            QStringLiteral("fullTextSearchAction") &&
        full_text_search_action_->text() ==
            QStringLiteral("Full-text search") &&
        full_text_search_action_->shortcut() ==
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F) &&
        full_text_search_action_->shortcutContext() ==
            Qt::WidgetWithChildrenShortcut &&
        full_text_search_action_->menuRole() == QAction::TextHeuristicRole &&
        full_text_search_action_->isEnabled() &&
        std::count_if(all_actions.cbegin(), all_actions.cend(),
                      [](const QAction* action) {
                          return action->shortcuts().contains(
                              QKeySequence::Find);
                      }) == 1 &&
        centralWidget() != nullptr && article_tabs_->isVisible() &&
        kMainWindowStateVersion == 7);
    const auto initial_session = facade_->ExportArticleTabSession();
    const std::string initial_state = CaptureMainWindowState();
    const auto first_tab_id = TabIdAt(article_tabs_->currentIndex());
    auto* first_view = ArticleViewForTab(first_tab_id);
    auto triggers = std::make_shared<int>(0);
    const auto trigger_connection = connect(
        search_in_page_action_, &QAction::triggered, this,
        [triggers]() { ++*triggers; }, Qt::DirectConnection);

    connect(
        first_view, &ArticleView::loadFinished, this,
        [this, completion = std::move(completion), passed, initial_session,
         initial_state, first_tab_id, first_view, trigger_connection,
         triggers](bool loaded) mutable {
            *passed = *passed && loaded;
            article_search_->setText(QStringLiteral("alpha"));
            search_in_page_action_->trigger();
            *passed =
                *passed && *triggers == 1 &&
                article_search_->focusPolicy() != Qt::NoFocus &&
                article_search_->selectedText() == QStringLiteral("alpha");
            if (!*passed)
                qWarning() << "search menu smoke failed before first find"
                           << loaded << *triggers
                           << article_search_->selectedText();
            article_search_->setText(QStringLiteral("missing"));
            FindInArticle(false);
            article_search_->setText(QStringLiteral("alpha"));
            FindInArticle(false);
            QTimer::singleShot(
                150, this,
                [this, completion = std::move(completion), passed,
                 initial_session, initial_state, first_tab_id, first_view,
                 trigger_connection, triggers]() mutable {
                    *passed = *passed && article_search_status_->text() ==
                                             QStringLiteral("1 of 2");
                    if (!*passed)
                        qWarning()
                            << "search menu smoke failed after first find"
                            << article_search_status_->text();
                    goldendict::core::TabNavigationState navigation;
                    navigation.title = "Search smoke second";
                    const auto opened = facade_->OpenArticleTab(
                        navigation, goldendict::core::TabOpenPolicy::kNewTab,
                        goldendict::core::TabActivationPolicy::kActivate,
                        goldendict::core::TabPlacementPolicy::kAppend);
                    *passed = *passed && static_cast<bool>(opened);
                    if (!opened) {
                        disconnect(trigger_connection);
                        completion(false);
                        return;
                    }
                    SyncArticleTabs();
                    const auto second_tab_id = opened.tab_id;
                    auto* second_view = ArticleViewForTab(second_tab_id);
                    connect(
                        second_view, &ArticleView::loadFinished, this,
                        [this, completion = std::move(completion), passed,
                         initial_session, initial_state, first_tab_id,
                         first_view, second_tab_id, trigger_connection,
                         triggers](bool second_loaded) mutable {
                            *passed = *passed && second_loaded &&
                                      article_search_->text().isEmpty() &&
                                      article_search_status_->text().isEmpty();
                            if (!*passed)
                                qWarning()
                                    << "search menu smoke failed on second tab"
                                    << second_loaded << article_search_->text()
                                    << article_search_status_->text();
                            article_search_->setText(QStringLiteral("missing"));
                            FindInArticle(false);
                            QTimer::singleShot(
                                150, this,
                                [this, completion = std::move(completion),
                                 passed, initial_session, initial_state,
                                 first_tab_id, first_view, second_tab_id,
                                 trigger_connection, triggers]() mutable {
                                    const QPointer<ArticleView> closing_view =
                                        ArticleViewForTab(second_tab_id);
                                    reload_action_->trigger();
                                    const auto pending_reload =
                                        article_reload_states_.find(
                                            second_tab_id);
                                    *passed =
                                        *passed &&
                                        article_search_status_->text() ==
                                            QStringLiteral("No matches") &&
                                        pending_reload !=
                                            article_reload_states_.end() &&
                                        pending_reload->second.generation ==
                                            1U &&
                                        pending_reload->second
                                            .in_flight_generation.has_value() &&
                                        facade_->ActivateArticleTab(
                                            first_tab_id);
                                    if (!*passed)
                                        qWarning()
                                            << "search menu smoke failed after "
                                               "second find"
                                            << article_search_status_->text();
                                    SyncArticleTabs();
                                    *passed =
                                        *passed &&
                                        article_view_ == first_view &&
                                        article_search_->text() ==
                                            QStringLiteral("alpha") &&
                                        article_search_status_->text() ==
                                            QStringLiteral("1 of 2") &&
                                        *triggers == 1 &&
                                        facade_->CloseArticleTab(second_tab_id);
                                    SyncArticleTabs();
                                    if (closing_view != nullptr)
                                        emit closing_view->loadFinished(true);
                                    QApplication::processEvents();
                                    *passed =
                                        *passed &&
                                        article_reload_states_.find(
                                            second_tab_id) ==
                                            article_reload_states_.end() &&
                                        article_search_presentations_.find(
                                            second_tab_id) ==
                                            article_search_presentations_
                                                .end() &&
                                        article_view_ == first_view &&
                                        article_search_->text() ==
                                            QStringLiteral("alpha") &&
                                        article_search_status_->text() ==
                                            QStringLiteral("1 of 2") &&
                                        facade_->ExportArticleTabSession() ==
                                            initial_session &&
                                        CaptureMainWindowState() ==
                                            initial_state &&
                                        centralWidget() != nullptr &&
                                        article_tabs_->isVisible();
                                    disconnect(trigger_connection);
                                    if (!*passed)
                                        qWarning()
                                            << "search menu smoke failed "
                                               "during restoration";
                                    completion(*passed);
                                });
                        },
                        Qt::SingleShotConnection);
                    second_view->setHtml(QStringLiteral(
                        "<!doctype html><html><body>beta beta</body></html>"));
                });
        },
        Qt::SingleShotConnection);
    first_view->setHtml(QStringLiteral(
        "<!doctype html><html><body>alpha beta alpha</body></html>"));
}

void MainWindow::RunFileMenuSmokeCheck(const QString& path,
                                       std::function<void(bool)> completion) {
    auto* file_menu = findChild<QMenu*>(QStringLiteral("menuFile"));
    auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    auto* add_tab_button =
        findChild<QToolButton*>(QStringLiteral("addArticleTabButton"));
    if (file_menu == nullptr || article_toolbar == nullptr ||
        add_tab_button == nullptr || add_tab_button->menu() == nullptr ||
        facade_ == nullptr || article_view_ == nullptr) {
        completion(false);
        return;
    }

    const auto actions = file_menu->actions();
    bool passed =
        menuBar()->actions().size() == 7 &&
        menuBar()->actions()[0]->menu() == file_menu &&
        menuBar()->actions()[1]->menu()->objectName() ==
            QStringLiteral("menuView") &&
        menuBar()->actions()[2]->menu()->objectName() ==
            QStringLiteral("menu_Edit") &&
        menuBar()->actions()[3]->menu()->objectName() ==
            QStringLiteral("menuSearch") &&
        menuBar()->actions()[4]->menu()->objectName() ==
            QStringLiteral("menuHistory") &&
        findChildren<QMenu*>(QStringLiteral("menuFile")).size() == 1 &&
        file_menu->title() == QStringLiteral("&File") && actions.size() == 8 &&
        actions[0] == new_tab_action_ && actions[1]->isSeparator() &&
        actions[2] == print_preview_action_ && actions[3] == print_action_ &&
        actions[4]->isSeparator() && actions[5] == save_article_action_ &&
        actions[6]->isSeparator() && actions[7] == quit_action_ &&
        new_tab_action_->objectName() == QStringLiteral("newTab") &&
        print_preview_action_->objectName() == QStringLiteral("printPreview") &&
        print_action_->objectName() == QStringLiteral("print") &&
        save_article_action_->objectName() == QStringLiteral("saveArticle") &&
        quit_action_->objectName() == QStringLiteral("quit") &&
        new_tab_action_->text() == QStringLiteral("&New Tab") &&
        print_preview_action_->text() == QStringLiteral("Print Pre&view") &&
        print_action_->text() == QStringLiteral("&Print") &&
        save_article_action_->text() == QStringLiteral("&Save Article") &&
        quit_action_->text() == QStringLiteral("&Quit") &&
        new_tab_action_->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_T) &&
        new_tab_action_->shortcutContext() == Qt::WidgetShortcut &&
        print_preview_action_->shortcut().isEmpty() &&
        print_action_->shortcut() == QKeySequence::Print &&
        save_article_action_->shortcut() == QKeySequence(Qt::Key_F2) &&
        quit_action_->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_Q) &&
        new_tab_action_->menuRole() == QAction::NoRole &&
        print_preview_action_->menuRole() == QAction::NoRole &&
        print_action_->menuRole() == QAction::NoRole &&
        save_article_action_->menuRole() == QAction::NoRole &&
        quit_action_->menuRole() == QAction::QuitRole &&
        article_toolbar->actions().contains(print_preview_action_) &&
        article_toolbar->actions().contains(print_action_) &&
        article_toolbar->actions().contains(save_article_action_) &&
        add_tab_button->menu()->actions().contains(new_tab_action_);

    const auto all_actions = findChildren<QAction*>();
    for (const auto& shortcut :
         {QKeySequence(Qt::CTRL | Qt::Key_T), QKeySequence(QKeySequence::Print),
          QKeySequence(Qt::Key_F2), QKeySequence(Qt::CTRL | Qt::Key_Q)}) {
        passed =
            passed &&
            std::count_if(all_actions.cbegin(), all_actions.cend(),
                          [&shortcut](const QAction* action) {
                              return action->shortcuts().contains(shortcut);
                          }) == 1;
    }

    int new_tab_triggers = 0;
    const auto new_tab_connection = connect(
        new_tab_action_, &QAction::triggered, this,
        [&new_tab_triggers]() { ++new_tab_triggers; }, Qt::DirectConnection);
    const int tab_count = article_tabs_->count();
    add_tab_button->click();
    passed = passed && new_tab_triggers == 1 &&
             article_tabs_->count() == tab_count + 1;
    disconnect(new_tab_connection);
    const auto session = facade_->ExportArticleTabSession();
    const std::string window_state = CaptureMainWindowState();

    int print_triggers = 0;
    int preview_triggers = 0;
    int print_dialogs = 0;
    int print_dispatches = 0;
    const auto print_connection = connect(
        print_action_, &QAction::triggered, this,
        [&print_triggers]() { ++print_triggers; }, Qt::DirectConnection);
    const auto preview_connection = connect(
        print_preview_action_, &QAction::triggered, this,
        [&preview_triggers]() { ++preview_triggers; }, Qt::DirectConnection);
    printer_available_ = []() {
        return true;
    };
    print_dialog_executor_ = [&print_dialogs](QPrinter*) {
        ++print_dialogs;
        return false;
    };
    print_dispatcher_ = [&print_dispatches](ArticleView*, QPrinter*) {
        ++print_dispatches;
    };
    print_action_->trigger();
    passed = passed && print_triggers == 1 && print_dialogs == 1 &&
             print_dispatches == 0 && print_action_->isEnabled() &&
             print_preview_action_->isEnabled();
    print_dialog_executor_ = [&print_dialogs](QPrinter*) {
        ++print_dialogs;
        return true;
    };
    print_action_->trigger();
    passed = passed && print_triggers == 2 && print_dialogs == 2 &&
             print_dispatches == 1 && !print_action_->isEnabled() &&
             !print_preview_action_->isEnabled();
    print_preview_action_->trigger();
    passed = passed && preview_triggers == 0 && print_dispatches == 1;
    emit article_view_->printFinished(false);
    print_preview_executor_ =
        [&print_dispatches](QPrinter*, const std::function<void()>& paint) {
            static_cast<void>(print_dispatches);
            paint();
        };
    print_preview_action_->trigger();
    passed = passed && preview_triggers == 1 && print_dispatches == 2 &&
             !print_action_->isEnabled() && !print_preview_action_->isEnabled();
    emit article_view_->printFinished(true);
    disconnect(print_connection);
    disconnect(preview_connection);
    printer_available_ = {};
    print_dialog_executor_ = {};
    print_preview_executor_ = {};
    print_dispatcher_ = {};

    int quit_triggers = 0;
    int quit_dispatches = 0;
    const auto quit_connection = connect(
        quit_action_, &QAction::triggered, this,
        [&quit_triggers]() { ++quit_triggers; }, Qt::DirectConnection);
    quit_dispatcher_ = [&quit_dispatches]() {
        ++quit_dispatches;
    };
    quit_action_->trigger();
    passed = passed && quit_triggers == 1 && quit_dispatches == 1;
    disconnect(quit_connection);
    quit_dispatcher_ = {};

    QFile destination(path);
    passed = passed && destination.open(QIODevice::WriteOnly) &&
             destination.write("original") == 8;
    destination.close();
    auto save_triggers = std::make_shared<int>(0);
    auto save_writes = std::make_shared<int>(0);
    const auto save_connection = connect(
        save_article_action_, &QAction::triggered, this,
        [save_triggers]() { ++*save_triggers; }, Qt::DirectConnection);
    save_article_path_provider_ = []() {
        return QString();
    };
    save_article_action_->trigger();
    passed = passed && *save_triggers == 1 && !save_in_progress_;
    save_article_path_provider_ = [path]() {
        return path;
    };
    article_save_writer_ = [save_writes](const QString&, const QString&) {
        ++*save_writes;
        return false;
    };
    auto save_started = std::make_shared<bool>(false);
    auto save_disabled_while_pending = std::make_shared<bool>(false);
    auto start_save = std::make_shared<std::function<void(int)>>();
    *start_save = [this, start_save, save_started,
                   save_disabled_while_pending](int remaining) {
        if (article_view_->page()->isLoading() && remaining > 0) {
            QTimer::singleShot(10, this, [start_save, remaining]() {
                (*start_save)(remaining - 1);
            });
            return;
        }
        save_article_action_->trigger();
        *save_started = true;
        *save_disabled_while_pending =
            save_in_progress_ && !save_article_action_->isEnabled();
    };
    (*start_save)(100);

    auto poll = std::make_shared<std::function<void(int)>>();
    *poll = [this, path, completion = std::move(completion), poll,
             save_connection, save_triggers, save_writes, save_started,
             save_disabled_while_pending, passed, session,
             window_state](int remaining) mutable {
        if ((!*save_started || save_in_progress_) && remaining > 0) {
            QTimer::singleShot(10, this,
                               [poll, remaining]() { (*poll)(remaining - 1); });
            return;
        }
        QFile preserved(path);
        const bool opened = preserved.open(QIODevice::ReadOnly);
        const bool failure_reported =
            status_->text() == QStringLiteral("HTML save failed");
        bool ownership_checked = false;
        const auto tab_id = TabIdAt(article_tabs_->currentIndex());
        auto* const save_view = ArticleViewForTab(tab_id);
        const auto navigation = article_navigation_generations_.find(tab_id);
        if (save_view != nullptr && save_view->page() != nullptr &&
            navigation != article_navigation_generations_.end()) {
            int ownership_writes = 0;
            article_save_writer_ = [&ownership_writes](const QString&,
                                                       const QString&) {
                ++ownership_writes;
                return true;
            };
            const QPointer<ArticleView> guarded_view(save_view);
            const QPointer<QWebEnginePage> guarded_page(save_view->page());
            const std::uint64_t navigation_generation = navigation->second;
            auto* foreign_page = new QWebEnginePage(save_view);
            save_in_progress_ = true;
            FinishArticleSave(tab_id, guarded_view, foreign_page,
                              navigation_generation, path,
                              QStringLiteral("foreign"));
            const bool foreign_page_rejected =
                ownership_writes == 0 && !save_in_progress_ &&
                status_->text() == QStringLiteral("HTML save canceled");
            delete foreign_page;
            save_in_progress_ = true;
            FinishArticleSave(tab_id, guarded_view, guarded_page,
                              navigation_generation + 1U, path,
                              QStringLiteral("stale"));
            const bool stale_navigation_rejected =
                ownership_writes == 0 && !save_in_progress_ &&
                status_->text() == QStringLiteral("HTML save canceled");
            save_in_progress_ = true;
            FinishArticleSave(tab_id, guarded_view, guarded_page,
                              navigation_generation, path,
                              QStringLiteral("owned"));
            const bool matching_owner_accepted =
                ownership_writes == 1 && !save_in_progress_ &&
                status_->text() == QStringLiteral("HTML saved");
            ownership_checked = foreign_page_rejected &&
                                stale_navigation_rejected &&
                                matching_owner_accepted;
        }
        bool final_passed =
            passed && *save_started && *save_disabled_while_pending &&
            *save_triggers == 2 && !save_in_progress_ && *save_writes == 1 &&
            opened && preserved.readAll() == "original" && failure_reported &&
            ownership_checked &&
            facade_->ExportArticleTabSession() == session &&
            CaptureMainWindowState() == window_state &&
            centralWidget() != nullptr && article_tabs_->isVisible() &&
            article_tabs_->size().width() > 0 &&
            article_tabs_->size().height() > 0 && kMainWindowStateVersion == 7;
        if (!final_passed) {
            qWarning() << "file menu save ownership smoke failed" << passed
                       << save_in_progress_ << *save_writes << opened
                       << failure_reported << ownership_checked
                       << status_->text();
        }
        disconnect(save_connection);
        save_article_path_provider_ = {};
        article_save_writer_ = {};
        completion(final_passed);
    };
    QTimer::singleShot(0, this, [poll]() { (*poll)(100); });
}

void MainWindow::RunWebEngineInteractionCheck(
    std::function<void(bool)> completion) {
    connect(
        article_view_, &ArticleView::loadFinished, this,
        [this, completion = std::move(completion)](bool loaded) mutable {
            if (!loaded) {
                completion(false);
                return;
            }
            article_view_->setZoomFactor(1.25);
            article_view_->findText(
                QStringLiteral("needle"), {},
                [this, completion = std::move(completion)](
                    const QWebEngineFindTextResult& result) mutable {
                    const bool interaction_passed =
                        result.numberOfMatches() == 2 &&
                        article_view_->zoomFactor() == 1.25;
                    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
                    auto& presentation = article_search_presentations_[tab_id];
                    presentation.query = QStringLiteral("needle 😀");
                    presentation.status.clear();
                    const std::uint64_t generation = ++presentation.generation;
                    RefreshArticleSearch();
                    DispatchArticleSearch(tab_id, article_view_,
                                          presentation.query, generation,
                                          false);
                    auto attempts = std::make_shared<int>(0);
                    auto poll = std::make_shared<std::function<void()>>();
                    *poll = [this, tab_id, interaction_passed,
                             completion = std::move(completion), attempts,
                             poll]() mutable {
                        const auto found =
                            article_search_presentations_.find(tab_id);
                        if (found != article_search_presentations_.end() &&
                            found->second.status == QStringLiteral("1 of 2")) {
                            found->second.query = QStringLiteral("absent 😀");
                            found->second.status.clear();
                            const std::uint64_t no_match_generation =
                                ++found->second.generation;
                            DispatchArticleSearch(tab_id, article_view_,
                                                  found->second.query,
                                                  no_match_generation, false);
                            auto no_match_attempts = std::make_shared<int>(0);
                            auto no_match_poll =
                                std::make_shared<std::function<void()>>();
                            *no_match_poll = [this, tab_id, interaction_passed,
                                              completion =
                                                  std::move(completion),
                                              no_match_attempts,
                                              no_match_poll]() mutable {
                                const auto current =
                                    article_search_presentations_.find(tab_id);
                                if (current !=
                                        article_search_presentations_.end() &&
                                    current->second.status ==
                                        QStringLiteral("No matches")) {
                                    current->second.status =
                                        QStringLiteral("stable status");
                                    current->second.query =
                                        QStringLiteral("needle 😀");
                                    const std::uint64_t stale_generation =
                                        ++current->second.generation;
                                    DispatchArticleSearch(tab_id, article_view_,
                                                          current->second.query,
                                                          stale_generation,
                                                          false);
                                    current->second.query =
                                        QStringLiteral("replacement");
                                    ++current->second.generation;
                                    QTimer::singleShot(
                                        100, this,
                                        [this, tab_id, interaction_passed,
                                         completion =
                                             std::move(completion)]() mutable {
                                            const auto final_search =
                                                article_search_presentations_
                                                    .find(tab_id);
                                            const bool stale_safe =
                                                final_search !=
                                                    article_search_presentations_
                                                        .end() &&
                                                final_search->second.status ==
                                                    QStringLiteral(
                                                        "stable status");
                                            RunArticleSearchReloadCheck(
                                                tab_id,
                                                interaction_passed &&
                                                    stale_safe,
                                                std::move(completion));
                                        });
                                    return;
                                }
                                if (++*no_match_attempts >= 200) {
                                    completion(false);
                                    return;
                                }
                                QTimer::singleShot(10, this, *no_match_poll);
                            };
                            QTimer::singleShot(0, this, *no_match_poll);
                            return;
                        }
                        if (++*attempts >= 200) {
                            completion(false);
                            return;
                        }
                        QTimer::singleShot(10, this, *poll);
                    };
                    QTimer::singleShot(0, this, *poll);
                });
        },
        Qt::SingleShotConnection);
    article_view_->setHtml(
        QStringLiteral("<!doctype html><html><body><p>needle 😀</p><p>needle "
                       "😀</p></body></html>"));
}

void MainWindow::RunArticleSearchReloadCheck(
    goldendict::core::ArticleTabId tab_id, bool passed,
    std::function<void(bool)> completion) {
    const auto search = article_search_presentations_.find(tab_id);
    if (search == article_search_presentations_.end() ||
        reload_action_ == nullptr || article_view_ == nullptr) {
        completion(false);
        return;
    }
    search->second.query = QStringLiteral("needle 😀");
    search->second.status = QStringLiteral("stable navigation status");
    const std::uint64_t ownership_search_generation =
        ++search->second.generation;
    const auto navigation = article_navigation_generations_.find(tab_id);
    if (navigation == article_navigation_generations_.end() ||
        article_view_->page() == nullptr) {
        completion(false);
        return;
    }
    const QPointer<ArticleView> guarded_view(article_view_);
    const QPointer<QWebEnginePage> guarded_page(article_view_->page());
    const std::uint64_t stale_navigation_generation = navigation->second;
    auto* foreign_page = new QWebEnginePage(article_view_);
    FinishArticleSearch(tab_id, guarded_view, foreign_page,
                        stale_navigation_generation, search->second.query,
                        ownership_search_generation, 1, 2);
    const bool foreign_page_rejected =
        search->second.status == QStringLiteral("stable navigation status");
    delete foreign_page;
    ++navigation->second;
    FinishArticleSearch(tab_id, guarded_view, guarded_page,
                        stale_navigation_generation, search->second.query,
                        ownership_search_generation, 1, 2);
    const bool stale_navigation_rejected =
        search->second.status == QStringLiteral("stable navigation status");
    FinishArticleSearch(tab_id, guarded_view, guarded_page, navigation->second,
                        search->second.query, ownership_search_generation, 1,
                        2);
    const bool matching_navigation_accepted =
        search->second.status == QStringLiteral("1 of 2");
    passed = passed && foreign_page_rejected && stale_navigation_rejected &&
             matching_navigation_accepted;
    search->second.status.clear();
    ++search->second.generation;
    RefreshArticleSearch();
    article_reload_states_.erase(tab_id);
    auto load_starts = std::make_shared<int>(0);
    auto failed_finishes = std::make_shared<int>(0);
    auto successful_finishes = std::make_shared<int>(0);
    auto controlled_extra_finish = std::make_shared<bool>(false);
    auto extra_finishes = std::make_shared<int>(0);
    auto late_completion_rejected = std::make_shared<bool>(false);
    auto second_reload_requested = std::make_shared<bool>(false);
    auto started_connection = std::make_shared<QMetaObject::Connection>();
    auto finished_connection = std::make_shared<QMetaObject::Connection>();
    *started_connection = connect(
        article_view_, &ArticleView::loadStarted, this,
        [this, tab_id, load_starts, second_reload_requested,
         controlled_extra_finish, late_completion_rejected]() {
            ++*load_starts;
            if (!*second_reload_requested) {
                *second_reload_requested = true;
                reload_action_->trigger();
                article_view_->page()->triggerAction(QWebEnginePage::Stop);
                return;
            }
            if (*load_starts != 2)
                return;
            const bool loading = article_view_->page()->isLoading();
            *controlled_extra_finish = true;
            emit article_view_->loadFinished(true);
            *controlled_extra_finish = false;
            const auto reload = article_reload_states_.find(tab_id);
            *late_completion_rejected =
                loading && reload != article_reload_states_.end() &&
                reload->second.generation == 2U &&
                reload->second.in_flight_generation == 2U &&
                reload->second.load_started;
        });
    *finished_connection =
        connect(article_view_, &ArticleView::loadFinished, this,
                [failed_finishes, successful_finishes, controlled_extra_finish,
                 extra_finishes](bool success) {
                    if (*controlled_extra_finish) {
                        ++*extra_finishes;
                        return;
                    }
                    if (success)
                        ++*successful_finishes;
                    else
                        ++*failed_finishes;
                });
    reload_action_->trigger();

    auto attempts = std::make_shared<int>(0);
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, tab_id, passed, completion = std::move(completion), attempts,
             poll, load_starts, failed_finishes, successful_finishes,
             controlled_extra_finish, extra_finishes, late_completion_rejected,
             second_reload_requested, started_connection,
             finished_connection]() mutable {
        const auto current = article_search_presentations_.find(tab_id);
        const auto reload = article_reload_states_.find(tab_id);
        if (current != article_search_presentations_.end() &&
            reload != article_reload_states_.end() &&
            current->second.status.endsWith(QStringLiteral("of 2")) &&
            reload->second.generation == 2U &&
            !reload->second.in_flight_generation.has_value() &&
            *second_reload_requested && *load_starts >= 2 &&
            *failed_finishes >= 1 && *successful_finishes >= 1 &&
            *extra_finishes == 1 && *late_completion_rejected &&
            !*controlled_extra_finish) {
            passed = passed &&
                     current->second.query == QStringLiteral("needle 😀") &&
                     article_search_->text() == QStringLiteral("needle 😀") &&
                     article_search_status_->text() == current->second.status;
            const QString initial_status = current->second.status;
            const QString next_status =
                initial_status == QStringLiteral("1 of 2")
                    ? QStringLiteral("2 of 2")
                    : QStringLiteral("1 of 2");
            FindInArticle(false);
            auto next_attempts = std::make_shared<int>(0);
            auto next_poll = std::make_shared<std::function<void()>>();
            *next_poll = [this, tab_id, passed,
                          completion = std::move(completion), next_attempts,
                          next_poll, started_connection, finished_connection,
                          initial_status, next_status]() mutable {
                const auto next = article_search_presentations_.find(tab_id);
                if (next != article_search_presentations_.end() &&
                    next->second.status == next_status &&
                    article_search_status_->text() == next_status) {
                    FindInArticle(true);
                    auto previous_attempts = std::make_shared<int>(0);
                    auto previous_poll =
                        std::make_shared<std::function<void()>>();
                    *previous_poll = [this, tab_id, passed,
                                      completion = std::move(completion),
                                      previous_attempts, previous_poll,
                                      started_connection, finished_connection,
                                      initial_status]() mutable {
                        const auto previous =
                            article_search_presentations_.find(tab_id);
                        if (previous != article_search_presentations_.end() &&
                            previous->second.status == initial_status &&
                            article_search_status_->text() == initial_status) {
                            disconnect(*started_connection);
                            disconnect(*finished_connection);
                            article_view_->findText(QString());
                            completion(passed);
                            return;
                        }
                        if (++*previous_attempts >= 200) {
                            disconnect(*started_connection);
                            disconnect(*finished_connection);
                            completion(false);
                            return;
                        }
                        QTimer::singleShot(10, this, *previous_poll);
                    };
                    QTimer::singleShot(0, this, *previous_poll);
                    return;
                }
                if (++*next_attempts >= 200) {
                    disconnect(*started_connection);
                    disconnect(*finished_connection);
                    completion(false);
                    return;
                }
                QTimer::singleShot(10, this, *next_poll);
            };
            QTimer::singleShot(0, this, *next_poll);
            return;
        }
        if (++*attempts >= 200) {
            qWarning() << "article reload smoke timed out" << *load_starts
                       << *failed_finishes << *successful_finishes
                       << *extra_finishes << *late_completion_rejected
                       << *second_reload_requested
                       << (current == article_search_presentations_.end()
                               ? QStringLiteral("missing search")
                               : current->second.status)
                       << (reload == article_reload_states_.end()
                               ? 0U
                               : reload->second.generation)
                       << (reload != article_reload_states_.end() &&
                           reload->second.in_flight_generation.has_value());
            disconnect(*started_connection);
            disconnect(*finished_connection);
            completion(false);
            return;
        }
        QTimer::singleShot(10, this, *poll);
    };
    QTimer::singleShot(0, this, *poll);
}

void MainWindow::RunArticleContextMenuCheck(
    std::function<void(bool)> completion) {
    ArticleView view;
    view.SetFacade(facade_);
    const ArticleContext internal{
        QStringLiteral(" selected "),
        QUrl(QStringLiteral("goldendict://lookup/linked%20word")), true};
    const auto internal_actions = view.AvailableContextActions(internal);
    const ArticleContext external{
        {}, QUrl(QStringLiteral("https://example.test/article")), false};
    const auto external_actions = view.AvailableContextActions(external);
    const ArticleContext rejected{QString(60, QLatin1Char('x')),
                                  QUrl(QStringLiteral("file:///tmp/untrusted")),
                                  false};
    const auto rejected_actions = view.AvailableContextActions(rejected);
    const auto credential_actions = view.AvailableContextActions(ArticleContext{
        {},
        QUrl(QStringLiteral("https://user:secret@example.test/article")),
        false});
    QString selection;
    QUrl link;
    connect(&view, &ArticleView::SelectionLookupRequested, this,
            [&selection](const QString& text, ArticleLinkDisposition) {
                selection = text;
            });
    connect(&view, &ArticleView::LinkRequested, this,
            [&link](const QUrl& url, ArticleLinkDisposition) { link = url; });
    view.TriggerContextActionForTest(ArticleContextAction::kLookupSelection,
                                     internal);
    view.TriggerContextActionForTest(ArticleContextAction::kOpenLink, internal);
    view.TriggerContextActionForTest(ArticleContextAction::kCopyLink, external);
    const bool link_copied =
        QApplication::clipboard()->text() == external.link_url.toString();
    const auto session_before_copy = facade_->ExportArticleTabSession();
    article_view_->TriggerContextActionForTest(
        ArticleContextAction::kCopyAsText,
        ArticleContext{QStringLiteral("clipboard selection"), {}, false});
    const bool copy_preserved_session =
        facade_->ExportArticleTabSession() == session_before_copy;
    const auto state_before_navigation = facade_->GetArticleTabsState();
    const auto original_tab_id = state_before_navigation.active_tab_id;
    const auto original_tab = std::find_if(state_before_navigation.tabs.begin(),
                                           state_before_navigation.tabs.end(),
                                           [original_tab_id](const auto& tab) {
                                               return tab.id == original_tab_id;
                                           });
    const std::uint32_t original_group =
        original_tab == state_before_navigation.tabs.end()
            ? 0U
            : original_tab->navigation.group_id;
    article_view_->TriggerContextActionForTest(
        ArticleContextAction::kLookupSelectionInNewTab, internal);
    const auto navigation_state = facade_->GetArticleTabsState();
    const bool navigation_preserved_origin =
        navigation_state.tabs.size() ==
            state_before_navigation.tabs.size() + 1U &&
        navigation_state.active_tab_id == original_tab_id &&
        navigation_state.tabs.back().navigation.query == " selected " &&
        navigation_state.tabs.back().navigation.group_id == original_group;
    const bool restored = static_cast<bool>(
        facade_->RestoreArticleTabSession(session_before_copy));
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    const bool passed =
        internal_actions ==
            QList<ArticleContextAction>{
                ArticleContextAction::kOpenLink,
                ArticleContextAction::kOpenLinkInNewTab,
                ArticleContextAction::kLookupSelection,
                ArticleContextAction::kLookupSelectionInNewTab,
                ArticleContextAction::kSendSelectionToInput,
                ArticleContextAction::kCopy,
                ArticleContextAction::kCopyAsText,
                ArticleContextAction::kCopyImage} &&
        external_actions ==
            QList<ArticleContextAction>{ArticleContextAction::kOpenExternalLink,
                                        ArticleContextAction::kCopyLink,
                                        ArticleContextAction::kSelectAll} &&
        rejected_actions ==
            QList<ArticleContextAction>{ArticleContextAction::kCopy,
                                        ArticleContextAction::kCopyAsText} &&
        credential_actions ==
            QList<ArticleContextAction>{ArticleContextAction::kSelectAll} &&
        selection == internal.selected_text && link == internal.link_url &&
        link_copied && copy_preserved_session && navigation_preserved_origin &&
        restored;
    if (!passed) {
        qWarning() << "context menu smoke mismatch" << internal_actions.size()
                   << external_actions.size() << rejected_actions.size()
                   << credential_actions.size() << selection << link
                   << QApplication::clipboard()->text();
    }
    completion(passed);
}

void MainWindow::RunDictionaryContextNavigationCheck(
    std::function<void(bool)> completion) {
    const auto original_preferences = preferences_;
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    auto* view = ArticleViewForTab(tab_id);
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    if (tab_id == 0U || view == nullptr || results_dock == nullptr) {
        completion(false);
        return;
    }

    goldendict::core::TabNavigationState ordered_navigation;
    ordered_navigation.kind = goldendict::core::TabNavigationKind::kLookup;
    ordered_navigation.query = "scoped";
    ordered_navigation.title = ordered_navigation.query;
    ordered_navigation.group_id = selected_group_id_;
    ordered_navigation.dictionary_filter_active = true;
    ordered_navigation.dictionary_ids = {"second", "first", "second"};
    goldendict::core::LookupQuery ordered_query;
    ordered_query.group_id = selected_group_id_;
    ApplyNavigationDictionaryFilter(&ordered_query, ordered_navigation);
    auto empty_navigation = ordered_navigation;
    empty_navigation.dictionary_ids.clear();
    goldendict::core::LookupQuery empty_query;
    empty_query.group_id = selected_group_id_;
    ApplyNavigationDictionaryFilter(&empty_query, empty_navigation);
    auto ordinary_navigation = ordered_navigation;
    ordinary_navigation.dictionary_filter_active = false;
    ordinary_navigation.dictionary_ids.clear();
    goldendict::core::LookupQuery ordinary_expected;
    ordinary_expected.group_id = selected_group_id_;
    ApplyDictionaryFilter(&ordinary_expected);
    goldendict::core::LookupQuery ordinary_actual;
    ordinary_actual.group_id = selected_group_id_;
    ApplyNavigationDictionaryFilter(&ordinary_actual, ordinary_navigation);
    bool passed =
        ordered_query.dictionary_filter_active &&
        ordered_query.dictionary_ids == ordered_navigation.dictionary_ids &&
        empty_query.dictionary_filter_active &&
        empty_query.dictionary_ids.empty() &&
        ordinary_actual.dictionary_filter_active ==
            ordinary_expected.dictionary_filter_active &&
        ordinary_actual.dictionary_ids == ordinary_expected.dictionary_ids;

    goldendict::core::LookupResponse response;
    const auto append = [&response](std::string id, std::string name) {
        goldendict::core::DictionaryEntry entry;
        entry.dictionary.id = std::move(id);
        entry.dictionary.name = std::move(name);
        response.entries.push_back(std::move(entry));
    };
    append("dictionary-0", "Catalog Dictionary 0");
    append("dictionary-0", "Repeated Name Must Be Ignored");
    append("dictionary-1", "");
    for (int index = 2; index < 22; ++index) {
        append("dictionary-" + std::to_string(index),
               "Catalog Dictionary " + std::to_string(index));
    }
    auto& presentation = lookup_results_[tab_id];
    ++presentation.generation;
    StoreLookupResults(tab_id, response);
    RefreshResultsNavigation();
    auto snapshot = view->DictionaryContextSnapshot();
    passed =
        passed && snapshot.entries.size() == 20 && snapshot.overflow &&
        results_list_->count() == 22 &&
        snapshot.entries[0].dictionary_id == QStringLiteral("dictionary-0") &&
        snapshot.entries[0].display_name ==
            QStringLiteral("Catalog Dictionary 0") &&
        snapshot.entries[1].dictionary_id == QStringLiteral("dictionary-1") &&
        snapshot.entries[1].display_name == QStringLiteral("dictionary-1") &&
        snapshot.entries[1].first_result_index == 2 &&
        results_list_->item(1)->data(Qt::UserRole + 1).toInt() == 2;

    auto navigation_requests = std::make_shared<int>(0);
    auto requested_result = std::make_shared<int>(-1);
    connect(view, &ArticleView::DictionaryResultRequested, this,
            [navigation_requests, requested_result](const QString&, int index,
                                                    quint64) {
                ++*navigation_requests;
                *requested_result = index;
            });
    view->TriggerDictionaryContextActionForTest(snapshot, 1);
    passed = passed && *navigation_requests == 1 && *requested_result == 2;

    auto zero_preferences = preferences_;
    zero_preferences.maximum_dictionary_references = 0U;
    SetPreferences(zero_preferences);
    auto zero_snapshot = view->DictionaryContextSnapshot();
    view->TriggerDictionaryContextActionForTest(snapshot, 0);
    passed = passed && zero_snapshot.entries.isEmpty() &&
             zero_snapshot.overflow && *navigation_requests == 1;

    auto one_preferences = preferences_;
    one_preferences.maximum_dictionary_references = 1U;
    SetPreferences(one_preferences);
    auto one_snapshot = view->DictionaryContextSnapshot();
    passed =
        passed && one_snapshot.entries.size() == 1 && one_snapshot.overflow;

    auto maximum_preferences = preferences_;
    maximum_preferences.maximum_dictionary_references = 9999U;
    SetPreferences(maximum_preferences);
    auto maximum_snapshot = view->DictionaryContextSnapshot();
    passed = passed && maximum_snapshot.entries.size() == 22 &&
             !maximum_snapshot.overflow;
    SetPreferences(original_preferences);
    snapshot = view->DictionaryContextSnapshot();

    presentation.rows.clear();
    RefreshDictionaryContext(tab_id);
    RefreshResultsNavigation();
    passed = passed && view->DictionaryContextSnapshot().entries.isEmpty() &&
             results_list_->count() == 0;
    StoreLookupResults(tab_id, response);
    RefreshResultsNavigation();
    ++presentation.generation;
    RefreshDictionaryContext(tab_id);
    view->TriggerDictionaryContextActionForTest(snapshot, 0);
    passed = passed && *navigation_requests == 1;
    snapshot = view->DictionaryContextSnapshot();

    results_dock->hide();
    view->TriggerDictionaryContextOverflowForTest(snapshot);
    passed = passed && results_dock->isVisible() &&
             TabIdAt(article_tabs_->currentIndex()) == tab_id &&
             results_list_->count() == 22;

    CreateEmptyArticleTab(true);
    const auto closing_tab_id = TabIdAt(article_tabs_->currentIndex());
    QPointer<ArticleView> closing_view = ArticleViewForTab(closing_tab_id);
    if (closing_view.isNull()) {
        completion(false);
        return;
    }
    auto& closing_presentation = lookup_results_[closing_tab_id];
    ++closing_presentation.generation;
    StoreLookupResults(closing_tab_id, response);
    const auto closing_snapshot = closing_view->DictionaryContextSnapshot();
    const int closing_index = article_tabs_->indexOf(closing_view);
    CloseArticleTab(closing_index);
    results_dock->hide();
    if (!closing_view.isNull())
        closing_view->TriggerDictionaryContextOverflowForTest(closing_snapshot);
    passed = passed &&
             lookup_results_.find(closing_tab_id) == lookup_results_.end() &&
             !results_dock->isVisible() &&
             TabIdAt(article_tabs_->currentIndex()) == tab_id;

    connect(
        view, &ArticleView::loadFinished, this,
        [this, view, snapshot, tab_id, navigation_requests, passed,
         completion = std::move(completion)](bool loaded) mutable {
            view->TriggerDictionaryContextActionForTest(snapshot, 0);
            view->TriggerDictionaryContextOverflowForTest(snapshot);
            bool final_passed =
                passed && loaded && *navigation_requests == 1 &&
                view->DictionaryContextSnapshot().entries.isEmpty() &&
                !view->DictionaryContextSnapshot().overflow;
            lookup_results_.erase(tab_id);
            RefreshDictionaryContext(tab_id);
            RefreshResultsNavigation();
            final_passed = final_passed && results_list_->count() == 0 &&
                           view->DictionaryContextSnapshot().entries.isEmpty();
            completion(final_passed);
        },
        Qt::SingleShotConnection);
    view->setHtml(QStringLiteral(
        "<!doctype html><html><body><h1>Non-lookup page</h1></body></html>"));
}

void MainWindow::RunSystemPrintCheck(std::function<void(bool)> completion) {
    int dialog_calls = 0;
    int dispatch_calls = 0;
    int preview_calls = 0;
    int pdf_writes = 0;
    std::vector<std::function<void(const QByteArray&)>> pdf_completions;
    const auto session_before = facade_->ExportArticleTabSession();
    const auto original_tab_id = TabIdAt(article_tabs_->currentIndex());
    auto* const original_view = article_view_;
    pdf_dispatcher_ = [&pdf_completions](
                          ArticleView*,
                          const std::function<void(const QByteArray&)>& done) {
        pdf_completions.push_back(done);
    };
    pdf_writer_ = [&pdf_writes](const QString& path,
                                const QByteArray& pdf_data) {
        ++pdf_writes;
        return path == QStringLiteral("second.pdf") && pdf_data == "second";
    };
    StartPdfExport(QStringLiteral("first.pdf"));
    StartPdfExport(QStringLiteral("second.pdf"));
    pdf_completions[0](QByteArrayLiteral("first"));
    const bool pdf_overlap_superseded =
        pdf_writes == 0 && status_->text() == QStringLiteral("Saving PDF...");
    pdf_completions[1](QByteArrayLiteral("second"));
    const bool pdf_latest_owned =
        pdf_writes == 1 && status_->text() == QStringLiteral("PDF saved");

    StartPdfExport(QStringLiteral("navigation.pdf"));
    const auto navigation_completion = pdf_completions.back();
    ++article_navigation_generations_[original_tab_id];
    status_->setText(QStringLiteral("navigation replaced"));
    navigation_completion(QByteArrayLiteral("navigation"));
    const bool pdf_navigation_replacement_safe =
        pdf_writes == 1 &&
        status_->text() == QStringLiteral("navigation replaced");

    StartPdfExport(QStringLiteral("page.pdf"));
    const auto page_completion = pdf_completions.back();
    auto* const original_page = original_view->page();
    auto* const replacement_page = new ArticlePage(original_view);
    replacement_page->SetFacade(facade_);
    original_view->setPage(replacement_page);
    status_->setText(QStringLiteral("page replaced"));
    page_completion(QByteArrayLiteral("page"));
    const bool pdf_page_replacement_safe =
        pdf_writes == 1 && status_->text() == QStringLiteral("page replaced");
    original_view->setPage(original_page);
    replacement_page->deleteLater();

    StartPdfExport(QStringLiteral("inactive.pdf"));
    const auto inactive_completion = pdf_completions.back();
    CreateEmptyArticleTab(true);
    status_->setText(QStringLiteral("other tab active"));
    inactive_completion(QByteArrayLiteral("inactive"));
    const bool pdf_active_projection_safe =
        pdf_writes == 2 &&
        status_->text() == QStringLiteral("other tab active");
    CloseArticleTab(article_tabs_->currentIndex());

    printer_available_ = []() {
        return false;
    };
    print_dialog_executor_ = [&dialog_calls](QPrinter*) {
        ++dialog_calls;
        return false;
    };
    print_dispatcher_ = [&dispatch_calls](ArticleView*, QPrinter*) {
        ++dispatch_calls;
    };
    PrintArticle();
    const bool unavailable_safe =
        dialog_calls == 0 && dispatch_calls == 0 &&
        status_->text() == QStringLiteral("No printer is available");
    printer_available_ = []() {
        return true;
    };
    PrintArticle();
    const bool cancellation_safe = dialog_calls == 1 && dispatch_calls == 0;
    print_dialog_executor_ = [&dialog_calls](QPrinter*) {
        ++dialog_calls;
        return true;
    };
    PrintArticle();
    const bool accepted =
        dialog_calls == 2 && dispatch_calls == 1 && print_in_progress_;
    PrintArticle();
    const bool overlap_rejected = dialog_calls == 2 && dispatch_calls == 1;
    emit article_view_->printFinished(true);
    print_preview_executor_ =
        [&preview_calls](QPrinter*, const std::function<void()>& paint) {
            ++preview_calls;
            paint();
        };
    PreviewArticle();
    const bool preview =
        preview_calls == 1 && dispatch_calls == 2 && print_in_progress_;
    emit article_view_->printFinished(false);
    const bool failure_reported =
        status_->text() == QStringLiteral("Printing failed");

    PrintArticle();
    const bool replacement_print_started = print_in_progress_;
    InvalidateArticleOutputOwnership(original_tab_id, original_view);
    status_->setText(QStringLiteral("print owner replaced"));
    emit original_view->printFinished(true);
    const bool print_replacement_safe =
        replacement_print_started && !print_in_progress_ &&
        status_->text() == QStringLiteral("print owner replaced");

    StartPdfExport(QStringLiteral("facade.pdf"));
    const auto facade_completion = pdf_completions.back();
    PrintArticle();
    const bool facade_requests_started =
        pending_pdf_request_.has_value() && print_in_progress_;
    SetFacade(facade_);
    status_->setText(QStringLiteral("facade replaced"));
    facade_completion(QByteArrayLiteral("facade"));
    const bool facade_replacement_safe =
        facade_requests_started && !pending_pdf_request_.has_value() &&
        !pending_print_request_.has_value() && !print_in_progress_ &&
        pdf_writes == 2 && status_->text() == QStringLiteral("facade replaced");

    printer_available_ = {};
    print_dialog_executor_ = {};
    print_preview_executor_ = {};
    print_dispatcher_ = {};
    pdf_dispatcher_ = {};
    pdf_writer_ = {};
    completion(pdf_overlap_superseded && pdf_latest_owned &&
               pdf_navigation_replacement_safe && pdf_page_replacement_safe &&
               pdf_active_projection_safe && unavailable_safe &&
               cancellation_safe && accepted && overlap_rejected && preview &&
               failure_reported && print_replacement_safe &&
               facade_replacement_safe && !print_in_progress_ &&
               facade_->ExportArticleTabSession() == session_before);
}

void MainWindow::RunSuggestionPaneSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    if (search_dock == nullptr || suggestions_list_ == nullptr) {
        completion(false);
        return;
    }
    ApplyDefaultPaneLayout();
    auto lookup_count = std::make_shared<int>(0);
    connect(this, &MainWindow::LookupSubmitted, this,
            [lookup_count](const QString&, std::uint32_t) { ++*lookup_count; });
    query_->setText(QStringLiteral("a"));
    query_->setText(QStringLiteral("app"));
    auto attempts = std::make_shared<int>(0);
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, search_dock, lookup_count, attempts, poll,
             completion = std::move(completion)]() mutable {
        if (suggestions_list_->count() == 0 && ++*attempts < 100) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        bool passed =
            findChildren<QDockWidget*>(QString::fromLatin1(kSearchPaneName))
                    .size() == 1 &&
            dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
            search_dock->isVisible() && suggestions_list_->count() == 2 &&
            suggestions_list_->item(0)->text() == QStringLiteral("apple") &&
            suggestions_list_->item(1)->text() ==
                QStringLiteral("application") &&
            suggestions_list_->count() <= 20;
        query_->setFocus();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(query_, &down);
        passed = passed && suggestions_list_->hasFocus() &&
                 suggestions_list_->currentRow() == 0;
        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
        QApplication::sendEvent(suggestions_list_, &up);
        passed = passed && query_->hasFocus();
        suggestions_list_->setCurrentRow(0);
        emit suggestions_list_->itemClicked(suggestions_list_->item(0));
        passed = passed && query_->text() == QStringLiteral("apple") &&
                 suggestions_list_->count() == 0 && *lookup_count == 1 &&
                 article_view_ != nullptr && article_view_->hasFocus();
        query_->setText(QStringLiteral("app"));
        auto keyboard_attempts = std::make_shared<int>(0);
        auto keyboard_poll = std::make_shared<std::function<void()>>();
        *keyboard_poll = [this, passed, lookup_count, keyboard_attempts,
                          keyboard_poll,
                          completion = std::move(completion)]() mutable {
            if (suggestions_list_->count() < 2 && ++*keyboard_attempts < 100) {
                QTimer::singleShot(10, this, *keyboard_poll);
                return;
            }
            bool keyboard_passed = passed && suggestions_list_->count() == 2;
            suggestions_list_->setCurrentRow(1);
            suggestions_list_->setFocus();
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(suggestions_list_, &enter);
            keyboard_passed = keyboard_passed &&
                              query_->text() == QStringLiteral("application") &&
                              suggestions_list_->count() == 0 &&
                              *lookup_count == 2;
            query_->setText(QStringLiteral("app"));
            query_->clear();
            keyboard_passed =
                keyboard_passed && suggestions_list_->count() == 0;
            query_->setText(QString(5000, QLatin1Char('x')));
            QTimer::singleShot(100, this,
                               [this, keyboard_passed,
                                completion = std::move(completion)]() mutable {
                                   FinishLookup();
                                   completion(keyboard_passed &&
                                              suggestions_list_->count() == 0);
                               });
        };
        QTimer::singleShot(10, this, *keyboard_poll);
    };
    QTimer::singleShot(10, this, *poll);
}

void MainWindow::RunArticleTabsSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    goldendict::core::TabNavigationState initial_navigation;
    initial_navigation.title = "(untitled)";
    const goldendict::core::ArticleTabSession initial_session = {
        {{1U, {initial_navigation}, 0U}}, 1U};
    if (!facade_->RestoreArticleTabSession(initial_session)) {
        completion(false);
        return;
    }
    SyncArticleTabs();
    bool scroll_restoration_ownership = true;
    goldendict::core::TabNavigationState restored_navigation;
    restored_navigation.kind = goldendict::core::TabNavigationKind::kLookup;
    restored_navigation.query = "application";
    restored_navigation.title = "application";
    const goldendict::core::ArticleTabSession restored_session = {
        {{1U, {restored_navigation}, 0U}}, 1U};
    if (!facade_->RestoreArticleTabSession(restored_session)) {
        completion(false);
        return;
    }
    RebuildArticleTabs();
    const auto restoration_tab_id = TabIdAt(0);
    const auto wait_for_request = [this, restoration_tab_id]() {
        QElapsedTimer timer;
        timer.start();
        while (requests_.count(restoration_tab_id) == 1U &&
               !requests_.at(restoration_tab_id)->IsFinished() &&
               timer.elapsed() < 2000) {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        if (requests_.count(restoration_tab_id) == 1U &&
            requests_.at(restoration_tab_id)->IsFinished()) {
            FinishLookup();
            return true;
        }
        return requests_.count(restoration_tab_id) == 0U;
    };
    scroll_restoration_ownership = wait_for_request();
    auto* restoration_old_view = ArticleViewForTab(restoration_tab_id);
    if (restoration_old_view == nullptr) {
        completion(false);
        return;
    }
    QElapsedTimer initial_load_timer;
    initial_load_timer.start();
    while (restoration_old_view->page()->isLoading() &&
           initial_load_timer.elapsed() < 2000) {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    restoration_old_view->setZoomFactor(5.0);
    bool scroll_ready = false;
    restoration_old_view->page()->runJavaScript(
        QStringLiteral("document.body.insertAdjacentHTML('beforeend',"
                       "'<div style=\"height:3000px\"></div>');"
                       "window.scrollTo(11,75); window.scrollY;"),
        [&scroll_ready](const QVariant&) { scroll_ready = true; });
    QElapsedTimer scroll_timer;
    scroll_timer.start();
    while ((!scroll_ready ||
            restoration_old_view->page()->scrollPosition().y() < 60.0) &&
           scroll_timer.elapsed() < 2000) {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    const QPointF saved_scroll = restoration_old_view->page()->scrollPosition();
    scroll_restoration_ownership =
        scroll_restoration_ownership && scroll_ready && saved_scroll.y() >= 60;
    QWebEngineScript scroll_observer;
    scroll_observer.setName(QStringLiteral("scroll-restoration-smoke"));
    scroll_observer.setInjectionPoint(QWebEngineScript::DocumentReady);
    scroll_observer.setWorldId(QWebEngineScript::MainWorld);
    scroll_observer.setRunsOnSubFrames(false);
    scroll_observer.setSourceCode(
        QStringLiteral("globalThis.__goldendictRestoredScroll = null;"
                       "const originalScrollTo = window.scrollTo.bind(window);"
                       "window.scrollTo = (x, y) => {"
                       "globalThis.__goldendictRestoredScroll = [x, y];"
                       "return originalScrollTo(x, y);};"));
    QWebEngineProfile::defaultProfile()->scripts()->insert(scroll_observer);
    const quint64 stale_navigation_token =
        restoration_old_view->ReserveHtmlNavigation();
    const QPointer<ArticleView> restoration_old_view_guard =
        restoration_old_view;
    auto& restoration_search =
        article_search_presentations_[restoration_tab_id];
    restoration_search.query =
        QStringLiteral("absent-scroll-restoration-query");
    const std::uint64_t search_generation = restoration_search.generation;
    SetFacade(facade_);
    auto* restoration_view = ArticleViewForTab(restoration_tab_id);
    if (restoration_view != nullptr)
        restoration_view->setZoomFactor(5.0);
    scroll_restoration_ownership =
        scroll_restoration_ownership && wait_for_request();
    const auto pending_success =
        pending_article_scroll_restorations_.find(restoration_tab_id);
    scroll_restoration_ownership =
        restoration_old_view_guard.isNull() && restoration_view != nullptr &&
        pending_success != pending_article_scroll_restorations_.end() &&
        pending_success->second.navigation_token != 0U &&
        pending_success->second.navigation_token != stale_navigation_token;
    const quint64 matching_navigation_token =
        pending_success == pending_article_scroll_restorations_.end()
            ? 0U
            : pending_success->second.navigation_token;
    if (restoration_view != nullptr) {
        emit restoration_view->HtmlNavigationFinished(stale_navigation_token,
                                                      true);
    }
    scroll_restoration_ownership =
        scroll_restoration_ownership &&
        pending_article_scroll_restorations_.count(restoration_tab_id) == 1U &&
        restoration_search.generation == search_generation;
    QElapsedTimer restoration_timer;
    restoration_timer.start();
    while (
        (pending_article_scroll_restorations_.count(restoration_tab_id) == 1U ||
         (restoration_view != nullptr &&
          restoration_view->page()->scrollPosition().y() < 60.0)) &&
        restoration_timer.elapsed() < 2000) {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    QPointF requested_scroll;
    QPointF actual_javascript_scroll;
    bool requested_scroll_ready = false;
    if (restoration_view != nullptr) {
        restoration_view->page()->runJavaScript(
            QStringLiteral(
                "[globalThis.__goldendictRestoredScroll, window.scrollX, "
                "window.scrollY]"),
            [&requested_scroll, &actual_javascript_scroll,
             &requested_scroll_ready](const QVariant& value) {
                const QVariantList result = value.toList();
                const QVariantList coordinates = result.value(0).toList();
                if (coordinates.size() == 2) {
                    requested_scroll = QPointF(coordinates[0].toDouble(),
                                               coordinates[1].toDouble());
                }
                if (result.size() == 3) {
                    actual_javascript_scroll =
                        QPointF(result[1].toDouble(), result[2].toDouble());
                }
                requested_scroll_ready = true;
            });
    }
    QElapsedTimer requested_scroll_timer;
    requested_scroll_timer.start();
    while (!requested_scroll_ready && requested_scroll_timer.elapsed() < 2000)
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    QWebEngineProfile::defaultProfile()->scripts()->remove(scroll_observer);
    const QPointF expected_css_scroll(saved_scroll.x() / 5.0,
                                      saved_scroll.y() / 5.0);
    scroll_restoration_ownership =
        scroll_restoration_ownership &&
        pending_article_scroll_restorations_.count(restoration_tab_id) == 0U &&
        restoration_search.generation == search_generation + 1U &&
        restoration_view != nullptr && requested_scroll_ready &&
        qAbs(restoration_view->page()->scrollPosition().x() -
             saved_scroll.x()) < 0.5 &&
        qAbs(restoration_view->page()->scrollPosition().y() -
             saved_scroll.y()) < 0.5 &&
        qAbs(requested_scroll.x() - expected_css_scroll.x()) < 0.5 &&
        qAbs(requested_scroll.y() - expected_css_scroll.y()) < 0.5 &&
        qAbs(actual_javascript_scroll.x() - expected_css_scroll.x()) < 0.5 &&
        qAbs(actual_javascript_scroll.y() - expected_css_scroll.y()) < 0.5;
    if (restoration_view != nullptr) {
        emit restoration_view->HtmlNavigationFinished(matching_navigation_token,
                                                      true);
    }
    scroll_restoration_ownership =
        scroll_restoration_ownership &&
        restoration_search.generation == search_generation + 1U;

    SetFacade(facade_);
    scroll_restoration_ownership =
        scroll_restoration_ownership && wait_for_request();
    restoration_view = ArticleViewForTab(restoration_tab_id);
    const auto pending_failure =
        pending_article_scroll_restorations_.find(restoration_tab_id);
    const quint64 failed_navigation_token =
        pending_failure == pending_article_scroll_restorations_.end()
            ? 0U
            : pending_failure->second.navigation_token;
    if (restoration_view != nullptr) {
        emit restoration_view->HtmlNavigationFinished(failed_navigation_token,
                                                      false);
    }
    scroll_restoration_ownership =
        scroll_restoration_ownership && failed_navigation_token != 0U &&
        pending_article_scroll_restorations_.count(restoration_tab_id) == 0U &&
        restoration_search.generation == search_generation + 1U;

    SetFacade(facade_);
    scroll_restoration_ownership =
        scroll_restoration_ownership &&
        pending_article_scroll_restorations_.count(restoration_tab_id) == 1U;
    restoration_search.query.clear();
    restoration_view = ArticleViewForTab(restoration_tab_id);
    ReloadCurrentArticle();
    scroll_restoration_ownership =
        scroll_restoration_ownership &&
        pending_article_scroll_restorations_.count(restoration_tab_id) == 0U;
    QElapsedTimer reload_timer;
    reload_timer.start();
    while (restoration_view != nullptr &&
           restoration_view->page()->isLoading() &&
           reload_timer.elapsed() < 2000) {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    scroll_restoration_ownership =
        scroll_restoration_ownership &&
        pending_article_scroll_restorations_.count(restoration_tab_id) == 0U;

    SetFacade(facade_);
    const auto replacement_restoration_tab_id = TabIdAt(0);
    restoration_view = ArticleViewForTab(replacement_restoration_tab_id);
    scroll_restoration_ownership = scroll_restoration_ownership &&
                                   pending_article_scroll_restorations_.count(
                                       replacement_restoration_tab_id) == 1U;
    if (restoration_view != nullptr) {
        auto* replacement_page = new ArticlePage(restoration_view);
        replacement_page->SetFacade(facade_);
        restoration_view->setPage(replacement_page);
    }
    scroll_restoration_ownership = scroll_restoration_ownership &&
                                   pending_article_scroll_restorations_.count(
                                       replacement_restoration_tab_id) == 0U;

    SetFacade(facade_);
    const auto closing_restoration_tab_id = TabIdAt(0);
    scroll_restoration_ownership = scroll_restoration_ownership &&
                                   pending_article_scroll_restorations_.count(
                                       closing_restoration_tab_id) == 1U;
    CloseArticleTab(0);
    scroll_restoration_ownership = scroll_restoration_ownership &&
                                   pending_article_scroll_restorations_.count(
                                       closing_restoration_tab_id) == 0U;
    scroll_restoration_ownership =
        scroll_restoration_ownership &&
        facade_->RestoreArticleTabSession(initial_session);
    SetFacade(facade_);
    const QSize default_size = size();
    const std::string saved_geometry = CaptureMainWindowGeometry();
    resize(default_size + QSize(37, 29));
    const bool geometry_accepted = RestoreMainWindowGeometry(saved_geometry);
    QApplication::processEvents();
    const bool restored_geometry = geometry_accepted;
    const QRect before_rejected = geometry();
    const bool rejected_geometry =
        !RestoreMainWindowGeometry("not-qt-geometry") &&
        geometry() == before_rejected;
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    auto* dictionary_bar =
        findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
    auto* back_button = findChild<QWidget*>(QStringLiteral("backButton"));
    auto* forward_button = findChild<QWidget*>(QStringLiteral("forwardButton"));
    if (history_dock == nullptr || favorites_dock == nullptr ||
        results_dock == nullptr || search_dock == nullptr ||
        results_list_ == nullptr || suggestions_list_ == nullptr ||
        article_toolbar == nullptr || nav_toolbar == nullptr ||
        dictionary_bar == nullptr || back_button == nullptr ||
        forward_button == nullptr) {
        completion(false);
        return;
    }
    ApplyDefaultPaneLayout();
    const auto nav_actions = nav_toolbar->actions();
    const auto top_level_toolbars =
        findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
    const bool toolbar_order =
        top_level_toolbars.indexOf(nav_toolbar) >= 0 &&
        top_level_toolbars.indexOf(article_toolbar) >= 0 &&
        top_level_toolbars.indexOf(nav_toolbar) <
            top_level_toolbars.indexOf(article_toolbar);
    const bool default_navigation_toolbar =
        findChildren<QToolBar*>(QStringLiteral("navToolbar")).size() == 1 &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea &&
        toolBarArea(article_toolbar) == Qt::TopToolBarArea && toolbar_order &&
        nav_toolbar->isVisible() && nav_toolbar->isMovable() &&
        nav_toolbar->isFloatable() &&
        nav_toolbar->allowedAreas() ==
            (Qt::TopToolBarArea | Qt::BottomToolBarArea) &&
        nav_actions.size() == 3 && nav_actions[0] == back_action_ &&
        nav_actions[1] == forward_action_ &&
        nav_toolbar->widgetForAction(nav_actions[0]) == back_button &&
        nav_toolbar->widgetForAction(nav_actions[1]) == forward_button &&
        group_selector_->parentWidget() == group_selector_host_ &&
        group_selector_host_->parentWidget() == query_->parentWidget() &&
        query_->parentWidget() == lookup_button_->parentWidget() &&
        query_->parentWidget()->parentWidget() == nav_toolbar &&
        group_selector_->objectName() == QStringLiteral("groupSelector") &&
        query_->objectName() == QStringLiteral("translateLine") &&
        lookup_button_->objectName() == QStringLiteral("lookupButton") &&
        query_->minimumWidth() >= 200 &&
        group_selector_->nextInFocusChain() == query_ &&
        query_->nextInFocusChain() == lookup_button_;
    const bool default_dictionary_toolbar =
        findChildren<QToolBar*>(QStringLiteral("dictionaryBar")).size() == 1 &&
        toolBarArea(dictionary_bar) == Qt::TopToolBarArea &&
        dictionary_bar->isVisible() && dictionary_bar->isMovable() &&
        dictionary_bar->isFloatable() &&
        dictionary_bar->allowedAreas() == Qt::AllToolBarAreas;
    query_->setText(QStringLiteral("focus selection"));
    bool focus_shortcuts = false;
    for (auto* action : actions()) {
        if (action->shortcuts().contains(
                QKeySequence(QStringLiteral("Ctrl+L")))) {
            action->trigger();
            QApplication::processEvents();
            focus_shortcuts = query_->focusPolicy() != Qt::NoFocus &&
                              query_->selectedText() == query_->text() &&
                              action->shortcuts().contains(
                                  QKeySequence(QStringLiteral("Alt+D")));
            break;
        }
    }
    query_->clear();
    const bool default_shell =
        scroll_restoration_ownership &&
        findChildren<QDockWidget*>(QString::fromLatin1(kHistoryPaneName))
                .size() == 1 &&
        findChildren<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName))
                .size() == 1 &&
        findChildren<QDockWidget*>(QString::fromLatin1(kResultsPaneName))
                .size() == 1 &&
        findChildren<QDockWidget*>(QString::fromLatin1(kSearchPaneName))
                .size() == 1 &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(results_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        tabifiedDockWidgets(history_dock).empty() &&
        tabifiedDockWidgets(favorites_dock).empty() &&
        tabifiedDockWidgets(results_dock).empty() &&
        tabifiedDockWidgets(search_dock).empty() &&
        results_dock->geometry().top() <= favorites_dock->geometry().top() &&
        favorites_dock->geometry().top() <= history_dock->geometry().top() &&
        history_dock->findChild<QListWidget*>(QStringLiteral("historyList")) !=
            nullptr &&
        favorites_dock->findChild<QTreeWidget*>(
            QStringLiteral("favoritesTree")) == favorites_tree_ &&
        results_dock->findChild<QListWidget*>(QStringLiteral("dictsList")) ==
            results_list_ &&
        search_dock->findChild<QListWidget*>(QStringLiteral("wordList")) ==
            suggestions_list_ &&
        lookup_button_->nextInFocusChain() == suggestions_list_ &&
        centralWidget() != nullptr && article_tabs_ != nullptr &&
        !article_tabs_->isHidden() && article_tabs_->size().width() > 0 &&
        article_tabs_->size().height() > 0;
    history_dock->toggleViewAction()->trigger();
    const bool history_hidden = !history_dock->isVisible();
    history_dock->toggleViewAction()->trigger();
    favorites_dock->toggleViewAction()->trigger();
    const bool favorites_hidden = !favorites_dock->isVisible();
    favorites_dock->toggleViewAction()->trigger();
    results_dock->toggleViewAction()->trigger();
    const bool results_hidden = !results_dock->isVisible();
    results_dock->toggleViewAction()->trigger();
    search_dock->toggleViewAction()->trigger();
    const bool search_hidden = !search_dock->isVisible();
    search_dock->toggleViewAction()->trigger();
    const bool toggle_actions =
        history_hidden && favorites_hidden && results_hidden && search_hidden &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible();
    const std::string default_state = CaptureMainWindowState();
    dictionary_bar->setObjectName(
        QStringLiteral("preDictionaryBarPlaceholder"));
    const QByteArray previous_current_state =
        saveState(kPreviousMainWindowStateVersion);
    search_dock->setObjectName(QStringLiteral("preSearchPanePlaceholder"));
    const QByteArray older_current_state =
        saveState(kOlderMainWindowStateVersion);
    results_dock->setObjectName(QStringLiteral("preResultsPanePlaceholder"));
    const QByteArray oldest_current_state =
        saveState(kOldestMainWindowStateVersion);
    nav_toolbar->setObjectName(
        QStringLiteral("preNavigationToolbarPlaceholder"));
    const QByteArray earlier_current_state =
        saveState(kEarlierMainWindowStateVersion);
    history_dock->setObjectName(QString::fromLatin1(kPreviousHistoryDockName));
    favorites_dock->setObjectName(
        QString::fromLatin1(kPreviousFavoritesDockName));
    const QByteArray earliest_current_state =
        saveState(kEarliestMainWindowStateVersion);
    history_dock->setObjectName(QString::fromLatin1(kHistoryPaneName));
    favorites_dock->setObjectName(QString::fromLatin1(kFavoritesPaneName));
    results_dock->setObjectName(QString::fromLatin1(kResultsPaneName));
    search_dock->setObjectName(QString::fromLatin1(kSearchPaneName));
    nav_toolbar->setObjectName(QStringLiteral("navToolbar"));
    dictionary_bar->setObjectName(QStringLiteral("dictionaryBar"));
    addDockWidget(Qt::BottomDockWidgetArea, history_dock);
    addDockWidget(Qt::RightDockWidgetArea, favorites_dock);
    addDockWidget(Qt::LeftDockWidgetArea, results_dock);
    addDockWidget(Qt::RightDockWidgetArea, search_dock);
    addToolBar(Qt::BottomToolBarArea, article_toolbar);
    addToolBar(Qt::BottomToolBarArea, nav_toolbar);
    favorites_dock->hide();
    results_dock->hide();
    search_dock->hide();
    nav_toolbar->hide();
    dictionary_bar->hide();
    const std::string changed_state = CaptureMainWindowState();
    const bool reset_state = RestoreMainWindowState(default_state);
    const bool restored_state =
        RestoreMainWindowState(changed_state) &&
        dockWidgetArea(history_dock) == Qt::BottomDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(results_dock) == Qt::LeftDockWidgetArea &&
        dockWidgetArea(search_dock) == Qt::RightDockWidgetArea &&
        toolBarArea(article_toolbar) == Qt::BottomToolBarArea &&
        toolBarArea(nav_toolbar) == Qt::BottomToolBarArea &&
        !favorites_dock->isVisible() && !results_dock->isVisible() &&
        !search_dock->isVisible() && !nav_toolbar->isVisible() &&
        !dictionary_bar->isVisible();
    const bool restored_previous_current_state =
        RestoreMainWindowState(
            {previous_current_state.constData(),
             static_cast<std::size_t>(previous_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea &&
        toolBarArea(dictionary_bar) == Qt::TopToolBarArea &&
        dictionary_bar->isVisible();
    const bool restored_older_current_state =
        RestoreMainWindowState(
            {older_current_state.constData(),
             static_cast<std::size_t>(older_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea;
    const bool restored_oldest_current_state =
        RestoreMainWindowState(
            {oldest_current_state.constData(),
             static_cast<std::size_t>(oldest_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(results_dock) == Qt::RightDockWidgetArea &&
        history_dock->isVisible() && favorites_dock->isVisible() &&
        results_dock->isVisible() && search_dock->isVisible() &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea;
    const bool restored_earlier_current_state =
        RestoreMainWindowState(
            {earlier_current_state.constData(),
             static_cast<std::size_t>(earlier_current_state.size())}) &&
        dockWidgetArea(history_dock) == Qt::RightDockWidgetArea &&
        dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
        results_dock->isVisible() && search_dock->isVisible() &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea &&
        toolBarArea(dictionary_bar) == Qt::TopToolBarArea &&
        dictionary_bar->isVisible();
    const bool restored_earliest_current_state =
        RestoreMainWindowState(
            {earliest_current_state.constData(),
             static_cast<std::size_t>(earliest_current_state.size())}) &&
        dockWidgetArea(search_dock) == Qt::LeftDockWidgetArea &&
        search_dock->isVisible() && results_dock->isVisible() &&
        toolBarArea(nav_toolbar) == Qt::TopToolBarArea;
    const std::string before_bad_state = CaptureMainWindowState();
    const QByteArray incompatible_state = saveState(1);
    const bool rejected_state =
        !RestoreMainWindowState("not-qt-main-window-state") &&
        CaptureMainWindowState() == before_bad_state &&
        !RestoreMainWindowState(
            {incompatible_state.constData(),
             static_cast<std::size_t>(incompatible_state.size())}) &&
        CaptureMainWindowState() == before_bad_state &&
        !RestoreMainWindowState(std::string(64U * 1024U + 1U, 'x')) &&
        CaptureMainWindowState() == before_bad_state;
    favorites_dock->show();
    favorites_dock->setFloating(true);
    favorites_dock->setGeometry(100000, 100000, 240, 320);
    const std::string unreachable_state = CaptureMainWindowState();
    RestoreMainWindowState(default_state);
    const std::string before_unreachable = CaptureMainWindowState();
    const bool topology_restored = RestoreMainWindowState(unreachable_state);
    const bool topology_safe =
        topology_restored ? HasUsableMainWindowLayout()
                          : CaptureMainWindowState() == before_unreachable;
    const bool final_default = RestoreMainWindowState(default_state);
    bool passed =
        article_tabs_->count() == 1 &&
        facade_->GetArticleTabsState().tabs.size() == 1U && restored_geometry &&
        rejected_geometry && default_shell && default_navigation_toolbar &&
        default_dictionary_toolbar && focus_shortcuts && toggle_actions &&
        reset_state && restored_state && restored_previous_current_state &&
        rejected_state && restored_older_current_state &&
        restored_oldest_current_state && restored_earlier_current_state &&
        restored_earliest_current_state && topology_safe && final_default;
    if (!passed) {
        qWarning() << "shell state check failed" << restored_geometry
                   << rejected_geometry << default_shell << toggle_actions
                   << default_navigation_toolbar << default_dictionary_toolbar
                   << focus_shortcuts << reset_state << restored_state
                   << restored_previous_current_state << rejected_state
                   << restored_older_current_state
                   << restored_oldest_current_state
                   << restored_earlier_current_state << topology_safe
                   << restored_earliest_current_state << final_default;
    }
    query_->setText(QStringLiteral("application"));
    SelectGroup(0U);
    StartLookup();
    query_->setText(QStringLiteral("apple"));
    StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                     goldendict::core::TabActivationPolicy::kActivate);
    query_->setText(QStringLiteral("application"));
    StartLookupInTab(goldendict::core::TabOpenPolicy::kNewTab,
                     goldendict::core::TabActivationPolicy::kKeepActive);

    auto poll = std::make_shared<std::function<void()>>();
    auto rendered = std::make_shared<bool>(false);
    *poll = [this, passed, completion = std::move(completion), poll,
             rendered]() mutable {
        FinishLookup();
        if (!requests_.empty()) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        if (!*rendered) {
            *rendered = true;
            QTimer::singleShot(250, this, *poll);
            return;
        }
        auto state = facade_->GetArticleTabsState();
        bool ok =
            passed && state.tabs.size() == 3U && article_tabs_->count() == 3 &&
            state.tabs[0].navigation.query == "application" &&
            state.tabs[1].navigation.query == "apple" &&
            state.tabs[2].navigation.query == "application" &&
            state.active_tab_id == state.tabs[1].id &&
            results_list_->count() == 1 && results_list_->currentRow() == 0 &&
            results_list_->item(0)->text() ==
                QStringLiteral("Fixture Dictionary") &&
            results_list_->item(0)->data(Qt::UserRole).toString() ==
                QString::fromStdString(
                    facade_->GetDictionaryService().GetCatalog().front().id);
        if (!ok) {
            qWarning()
                << "results navigation check failed" << passed
                << state.tabs.size() << article_tabs_->count()
                << results_list_->count()
                << (results_list_->count() == 0
                        ? QString()
                        : results_list_->item(0)->text())
                << facade_->GetDictionaryService().GetCatalog().front().id;
        }
        auto* first = ArticleViewForTab(state.tabs[0].id);
        auto* second = ArticleViewForTab(state.tabs[1].id);
        auto* third = ArticleViewForTab(state.tabs[2].id);
        if (first == nullptr || second == nullptr || third == nullptr) {
            completion(false);
            return;
        }
        first->page()->toPlainText([this, second, third, ok,
                                    completion = std::move(completion)](
                                       const QString& first_text) mutable {
            second->page()->toPlainText([this, third, ok, first_text,
                                         completion = std::move(completion)](
                                            const QString&
                                                second_text) mutable {
                third->page()->toPlainText([this, ok, first_text, second_text,
                                            completion = std::move(completion)](
                                               const QString&
                                                   third_text) mutable {
                    bool smoke_passed = ok &&
                                        first_text.contains("A program") &&
                                        second_text.contains("A fruit") &&
                                        third_text.contains("A program");
                    auto current_state = facade_->GetArticleTabsState();
                    facade_->ActivateArticleTab(current_state.tabs[0].id);
                    SyncArticleTabs();
                    smoke_passed = smoke_passed &&
                                   query_->text() == "application" &&
                                   selected_group_id_ == 0U &&
                                   results_list_->count() == 1 &&
                                   results_list_->currentRow() == 0;
                    auto activations = std::make_shared<int>(0);
                    connect(
                        results_list_, &QListWidget::itemActivated, this,
                        [activations](QListWidgetItem*) { ++*activations; },
                        Qt::SingleShotConnection);
                    results_list_->setFocus();
                    QKeyEvent activate_press(QEvent::KeyPress, Qt::Key_Return,
                                             Qt::NoModifier);
                    QKeyEvent activate_release(QEvent::KeyRelease,
                                               Qt::Key_Return, Qt::NoModifier);
                    QApplication::sendEvent(results_list_, &activate_press);
                    QApplication::sendEvent(results_list_, &activate_release);
                    smoke_passed = smoke_passed && *activations == 1;
                    auto mouse_activations = std::make_shared<int>(0);
                    connect(
                        results_list_, &QListWidget::itemActivated, this,
                        [mouse_activations](QListWidgetItem*) {
                            ++*mouse_activations;
                        },
                        Qt::SingleShotConnection);
                    results_list_->setFocus();
                    const QPoint result_position =
                        results_list_->visualItemRect(results_list_->item(0))
                            .center();
                    QMouseEvent mouse_press(
                        QEvent::MouseButtonPress, result_position,
                        results_list_->viewport()->mapToGlobal(result_position),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent mouse_release(
                        QEvent::MouseButtonRelease, result_position,
                        results_list_->viewport()->mapToGlobal(result_position),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QMouseEvent double_click(
                        QEvent::MouseButtonDblClick, result_position,
                        results_list_->viewport()->mapToGlobal(result_position),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &mouse_press);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &mouse_release);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &double_click);
                    QApplication::sendEvent(results_list_->viewport(),
                                            &mouse_release);
                    smoke_passed = smoke_passed && *mouse_activations == 1;
                    NavigateArticleTab(false);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed &&
                        current_state.tabs[0].navigation.kind ==
                            goldendict::core::TabNavigationKind::kEmpty;
                    NavigateArticleTab(true);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed &&
                        current_state.tabs[0].navigation.query == "application";
                    NavigateArticleTab(false);
                    query_->setText(QStringLiteral("apple"));
                    StartLookup();
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed && !current_state.tabs[0].can_go_forward;

                    article_page_->LookupRequested(
                        QStringLiteral("application"),
                        QStringLiteral("goldendict://lookup/application"),
                        ArticleLinkDisposition::kCurrentTab);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed && current_state.tabs.size() == 3U &&
                        current_state.tabs.front().navigation.kind ==
                            goldendict::core::TabNavigationKind::kInternalLink;
                    article_page_->LookupRequested(
                        QStringLiteral("application"),
                        QStringLiteral("goldendict://lookup/application"),
                        ArticleLinkDisposition::kNewBackgroundTab);
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed = smoke_passed &&
                                   current_state.tabs.size() == 4U &&
                                   current_state.tabs.back().navigation.kind ==
                                       goldendict::core::TabNavigationKind::
                                           kInternalLink &&
                                   current_state.active_tab_id ==
                                       current_state.tabs.front().id;

                    const int before_middle = article_tabs_->count();
                    const int middle_index = before_middle - 1;
                    const QPoint middle_position =
                        article_tabs_->tabBar()->tabRect(middle_index).center();
                    QMouseEvent middle_event(
                        QEvent::MouseButtonPress, middle_position,
                        middle_position,
                        article_tabs_->tabBar()->mapToGlobal(middle_position),
                        Qt::MiddleButton, Qt::MiddleButton, {});
                    QApplication::sendEvent(article_tabs_->tabBar(),
                                            &middle_event);
                    smoke_passed = smoke_passed &&
                                   article_tabs_->count() == before_middle - 1;

                    current_state = facade_->GetArticleTabsState();
                    const auto expected_fallback = current_state.tabs[1].id;
                    CloseArticleTab(article_tabs_->currentIndex());
                    current_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed &&
                        current_state.active_tab_id == expected_fallback;

                    while (current_state.tabs.size() <
                           goldendict::core::kMaximumArticleTabs) {
                        goldendict::core::TabNavigationState extra;
                        extra.kind =
                            goldendict::core::TabNavigationKind::kLookup;
                        extra.query = "limit-" +
                                      std::to_string(current_state.tabs.size());
                        extra.title = extra.query;
                        if (!facade_->OpenArticleTab(
                                extra, goldendict::core::TabOpenPolicy::kNewTab,
                                goldendict::core::TabActivationPolicy::
                                    kKeepActive)) {
                            smoke_passed = false;
                            break;
                        }
                        current_state = facade_->GetArticleTabsState();
                    }
                    goldendict::core::TabNavigationState overflow;
                    overflow.kind =
                        goldendict::core::TabNavigationKind::kLookup;
                    overflow.query = "overflow";
                    overflow.title = overflow.query;
                    smoke_passed =
                        smoke_passed &&
                        facade_->OpenArticleTab(
                                   overflow,
                                   goldendict::core::TabOpenPolicy::kNewTab,
                                   goldendict::core::TabActivationPolicy::
                                       kKeepActive)
                                .error == goldendict::core::TabOperationError::
                                              kTabLimitReached;

                    CloseOtherArticleTabs(article_tabs_->currentIndex());
                    smoke_passed = smoke_passed && article_tabs_->count() == 1;
                    goldendict::core::TabOperationError navigation_error =
                        goldendict::core::TabOperationError::kNone;
                    for (std::size_t index = 0;
                         index <=
                         goldendict::core::kMaximumTabNavigationEntries;
                         ++index) {
                        goldendict::core::TabNavigationState bounded;
                        bounded.kind =
                            goldendict::core::TabNavigationKind::kLookup;
                        bounded.query = "navigation-" + std::to_string(index);
                        bounded.title = bounded.query;
                        const auto result = facade_->OpenArticleTab(
                            bounded,
                            goldendict::core::TabOpenPolicy::kCurrentTab,
                            goldendict::core::TabActivationPolicy::kActivate);
                        if (!result) {
                            navigation_error = result.error;
                            break;
                        }
                    }
                    smoke_passed = smoke_passed &&
                                   navigation_error ==
                                       goldendict::core::TabOperationError::
                                           kNavigationLimitReached;
                    SyncArticleTabs();
                    const auto retained_id =
                        facade_->GetArticleTabsState().active_tab_id;
                    CloseArticleTab(article_tabs_->currentIndex());
                    const auto replacement = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed && replacement.tabs.size() == 1U &&
                        replacement.active_tab_id != retained_id &&
                        replacement.tabs.front().navigation.kind ==
                            goldendict::core::TabNavigationKind::kEmpty;
                    goldendict::core::TabNavigationState empty;
                    empty.title = "(untitled)";
                    const goldendict::core::ArticleTabSession placement = {
                        {{10U, {empty}, 0U},
                         {20U, {empty}, 0U},
                         {30U, {empty}, 0U}},
                        20U};
                    smoke_passed = smoke_passed &&
                                   facade_->RestoreArticleTabSession(placement);
                    SyncArticleTabs();
                    auto configured = preferences_;
                    configured.open_new_tabs_after_current = true;
                    configured.open_new_tabs_in_background = true;
                    SetPreferences(configured);
                    auto* add_button = findChild<QToolButton*>(
                        QStringLiteral("addArticleTabButton"));
                    if (add_button == nullptr) {
                        completion(false);
                        return;
                    }
                    add_button->click();
                    auto configured_state = facade_->GetArticleTabsState();
                    smoke_passed = smoke_passed &&
                                   configured_state.tabs.size() == 4U &&
                                   configured_state.tabs[0].id == 10U &&
                                   configured_state.tabs[1].id == 20U &&
                                   configured_state.tabs[3].id == 30U &&
                                   configured_state.active_tab_id == 20U;
                    configured.open_new_tabs_in_background = false;
                    SetPreferences(configured);
                    add_button->click();
                    configured_state = facade_->GetArticleTabsState();
                    smoke_passed = smoke_passed &&
                                   configured_state.tabs.size() == 5U &&
                                   configured_state.tabs[1].id == 20U &&
                                   configured_state.active_tab_id ==
                                       configured_state.tabs[2].id;
#if defined(Q_OS_LINUX)
                    const auto before_selection =
                        facade_->ExportArticleTabSession();
                    QMouseEvent left_selection_event(
                        QEvent::MouseButtonPress, QPointF{}, QPointF{},
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    smoke_passed =
                        smoke_passed &&
                        !HandleLinuxPrimarySelectionMousePress(
                            &left_selection_event,
                            QStringLiteral("ignored selection")) &&
                        facade_->ExportArticleTabSession() == before_selection;
                    QMouseEvent empty_selection_event(
                        QEvent::MouseButtonPress, QPointF{}, QPointF{},
                        Qt::MiddleButton, Qt::MiddleButton, Qt::NoModifier);
                    smoke_passed =
                        smoke_passed &&
                        HandleLinuxPrimarySelectionMousePress(
                            &empty_selection_event, QString{}) &&
                        empty_selection_event.isAccepted() &&
                        facade_->ExportArticleTabSession() == before_selection;
                    QMouseEvent selection_event(
                        QEvent::MouseButtonPress, QPointF{}, QPointF{},
                        Qt::MiddleButton, Qt::MiddleButton, Qt::NoModifier);
                    smoke_passed = smoke_passed &&
                                   HandleLinuxPrimarySelectionMousePress(
                                       &selection_event,
                                       QStringLiteral("primary selection")) &&
                                   selection_event.isAccepted();
                    configured_state = facade_->GetArticleTabsState();
                    smoke_passed =
                        smoke_passed &&
                        query_->text() == QStringLiteral("primary selection") &&
                        configured_state.tabs[2].navigation.kind ==
                            goldendict::core::TabNavigationKind::kLookup &&
                        configured_state.tabs[2].navigation.query ==
                            "primary selection";
#endif
                    completion(smoke_passed);
                });
            });
        });
    };
    QTimer::singleShot(10, this, *poll);
}

void MainWindow::RunArticleTabSessionRestartSmokeCheck(
    bool prepare, std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    goldendict::core::TabNavigationState empty;
    empty.title = "(untitled)";
    goldendict::core::TabNavigationState lookup;
    lookup.kind = goldendict::core::TabNavigationKind::kLookup;
    lookup.query = "application";
    lookup.group_id = 7U;
    lookup.title = "application";
    goldendict::core::TabNavigationState link;
    link.kind = goldendict::core::TabNavigationKind::kInternalLink;
    link.query = "apple";
    link.group_id = 7U;
    link.title = "apple link";
    link.internal_url = "goldendict://lookup/apple";
    link.source_dictionary_id = "fixture-source";
    link.source_article_id = "source-article";
    link.target_article_id = "target-article";
    link.target_anchor = "target-anchor";
    goldendict::core::TabNavigationState active_link = link;
    active_link.query = "application";
    active_link.group_id = 9U;
    active_link.title = "application link";
    active_link.internal_url = "goldendict://lookup/application";
    active_link.target_article_id = "active-target";
    const goldendict::core::ArticleTabSession expected = {
        {{7U, {empty, lookup, link}, 1U}, {42U, {link, active_link}, 1U}}, 42U};
    auto* history_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* search_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    auto* dictionary_bar =
        findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
    if (history_dock == nullptr || favorites_dock == nullptr ||
        results_dock == nullptr || search_dock == nullptr ||
        article_toolbar == nullptr || nav_toolbar == nullptr ||
        dictionary_bar == nullptr) {
        completion(false);
        return;
    }
    if (prepare) {
        const auto restored = facade_->RestoreArticleTabSession(expected);
        if (!restored) {
            completion(false);
            return;
        }
        RebuildArticleTabs();
        addDockWidget(Qt::BottomDockWidgetArea, history_dock);
        addDockWidget(Qt::RightDockWidgetArea, favorites_dock);
        addDockWidget(Qt::LeftDockWidgetArea, results_dock);
        addDockWidget(Qt::RightDockWidgetArea, search_dock);
        addToolBar(Qt::BottomToolBarArea, article_toolbar);
        addToolBar(Qt::BottomToolBarArea, nav_toolbar);
        addToolBar(Qt::BottomToolBarArea, dictionary_bar);
        favorites_dock->hide();
        results_dock->hide();
        search_dock->hide();
        dictionary_bar->hide();
        emit ArticleTabSessionMutated();
        completion(true);
        return;
    }
    bool passed = facade_->ExportArticleTabSession() == expected &&
                  article_tabs_->count() == 2 && TabIdAt(0) == 7U &&
                  TabIdAt(1) == 42U &&
                  TabIdAt(article_tabs_->currentIndex()) == 42U &&
                  dockWidgetArea(history_dock) == Qt::BottomDockWidgetArea &&
                  dockWidgetArea(favorites_dock) == Qt::RightDockWidgetArea &&
                  dockWidgetArea(results_dock) == Qt::LeftDockWidgetArea &&
                  dockWidgetArea(search_dock) == Qt::RightDockWidgetArea &&
                  toolBarArea(article_toolbar) == Qt::BottomToolBarArea &&
                  toolBarArea(nav_toolbar) == Qt::BottomToolBarArea &&
                  toolBarArea(dictionary_bar) == Qt::BottomToolBarArea &&
                  nav_toolbar->isVisible() && !favorites_dock->isVisible() &&
                  !results_dock->isVisible() && !search_dock->isVisible() &&
                  !dictionary_bar->isVisible();
    if (!passed) {
        qWarning() << "restart shell check failed"
                   << (facade_->ExportArticleTabSession() == expected)
                   << article_tabs_->count() << TabIdAt(0) << TabIdAt(1)
                   << TabIdAt(article_tabs_->currentIndex())
                   << dockWidgetArea(history_dock)
                   << dockWidgetArea(favorites_dock)
                   << dockWidgetArea(results_dock)
                   << dockWidgetArea(search_dock)
                   << toolBarArea(article_toolbar) << toolBarArea(nav_toolbar)
                   << toolBarArea(dictionary_bar) << nav_toolbar->isVisible()
                   << favorites_dock->isVisible() << results_dock->isVisible()
                   << search_dock->isVisible() << dictionary_bar->isVisible();
    }
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, passed, completion = std::move(completion), poll]() mutable {
        FinishLookup();
        if (!requests_.empty()) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        auto* first = ArticleViewForTab(7U);
        auto* second = ArticleViewForTab(42U);
        if (first == nullptr || second == nullptr) {
            completion(false);
            return;
        }
        first->page()->toPlainText([this, second, passed,
                                    completion = std::move(completion)](
                                       const QString& first_text) mutable {
            second->page()->toPlainText(
                [this, passed, first_text, completion = std::move(completion)](
                    const QString& second_text) mutable {
                    goldendict::core::TabNavigationState next;
                    next.title = "(untitled)";
                    const auto opened = facade_->OpenArticleTab(
                        next, goldendict::core::TabOpenPolicy::kNewTab,
                        goldendict::core::TabActivationPolicy::kActivate);
                    bool ok = passed && first_text.contains("A program") &&
                              second_text.contains("A program") && opened &&
                              opened.tab_id == 43U;
                    if (opened) {
                        SyncArticleTabs();
                        emit ArticleTabSessionMutated();
                    }
                    completion(ok);
                });
        });
    };
    QTimer::singleShot(250, this, *poll);
}

void MainWindow::RunHistorySmokeCheck(std::function<void(bool)> completion) {
    const QString expected = QStringLiteral("history-smoke-entry");
    SetDictionaryGroups({{7U, "History Smoke Group", "", {}}});
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [this, expected, completion = std::move(completion)](
            const QString& submitted, std::uint32_t group_id) mutable {
            const bool recorded =
                submitted == expected && group_id == 7U &&
                history_list_->count() > 0 &&
                history_list_->item(0)->text() == expected &&
                history_list_->item(0)->data(Qt::UserRole).value<quint32>() ==
                    7U;
            SelectGroup(0U);
            connect(
                this, &MainWindow::LookupSubmitted, this,
                [recorded, completion = std::move(completion)](
                    const QString& restored, std::uint32_t restored_group) {
                    completion(recorded &&
                               restored ==
                                   QStringLiteral("history-smoke-entry") &&
                               restored_group == 7U);
                },
                Qt::SingleShotConnection);
            emit history_list_->itemActivated(history_list_->item(0));
        },
        Qt::SingleShotConnection);
    SelectGroup(7U);
    query_->setText(expected);
    StartLookup();
}

void MainWindow::RunHistoryManagementSmokeCheck(
    std::function<void(bool)> completion) {
    SetHistoryWords({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                     QStringLiteral("Alpine")});
    history_filter_->setText(QStringLiteral("alp"));
    const bool filtered =
        history_list_->count() == 2 &&
        history_list_->item(0)->text() == QStringLiteral("Alpha") &&
        history_list_->item(1)->text() == QStringLiteral("Alpine");
    connect(
        this, &MainWindow::ClearHistoryRequested, this,
        [this, filtered, completion = std::move(completion)]() mutable {
            completion(filtered && history_items_.empty() &&
                       history_list_->count() == 0);
        },
        Qt::SingleShotConnection);
    clear_history_button_->click();
}

void MainWindow::RunHistoryExportSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    const QString error = history_export_callback_(path);
    QFile file(path);
    const bool opened = file.open(QIODevice::ReadOnly);
    const QByteArray expected = QByteArray::fromHex("efbbbf") + "Alpha\nBeta\n";
    completion(error.isEmpty() && opened && file.readAll() == expected);
}

void MainWindow::RunHistoryImportSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    connect(
        this, &MainWindow::ImportHistoryRequested, this,
        [this, path, completion = std::move(completion)](
            const QString& requested_path, std::uint32_t group_id) mutable {
            completion(requested_path == path && group_id == 0U &&
                       history_items_.size() == 2 &&
                       history_items_[0].word == QStringLiteral("Alpha") &&
                       history_items_[1].word == QStringLiteral("第二个"));
        },
        Qt::SingleShotConnection);
    emit ImportHistoryRequested(path, selected_group_id_);
}

void MainWindow::RunFavoritesSmokeCheck(std::function<void(bool)> completion) {
    const QString expected = QStringLiteral("favorites-smoke-entry");
    const int initial_count = favorites_tree_->topLevelItemCount();
    connect(
        this, &MainWindow::AddFavoriteFolderRequested, this,
        [this, expected, initial_count, completion = std::move(completion)](
            const QString& folder_name,
            const QList<int>& folder_parent_path) mutable {
            auto* folder = favorites_tree_->topLevelItem(initial_count);
            const bool folder_added =
                folder_name == QStringLiteral("Smoke Folder") &&
                folder_parent_path.isEmpty() && folder != nullptr &&
                folder->data(0, Qt::UserRole).toBool();
            favorites_tree_->setCurrentItem(folder);
            connect(
                this, &MainWindow::AddFavoriteRequested, this,
                [this, expected, folder_added, initial_count,
                 completion = std::move(completion)](
                    const QString& submitted,
                    const QList<int>& word_parent_path) mutable {
                    auto* refreshed_folder =
                        favorites_tree_->topLevelItem(initial_count);
                    const bool nested =
                        folder_added && submitted == expected &&
                        word_parent_path == QList<int>{initial_count} &&
                        refreshed_folder != nullptr &&
                        refreshed_folder->childCount() == 1 &&
                        refreshed_folder->child(0)->text(0) == expected;
                    connect(
                        this, &MainWindow::RenameFavoriteRequested, this,
                        [this, nested, initial_count,
                         completion = std::move(completion)](
                            const QList<int>& path,
                            const QString& renamed_name) mutable {
                            auto* renamed_folder =
                                favorites_tree_->topLevelItem(initial_count);
                            const bool renamed =
                                nested &&
                                path == QList<int>{initial_count, 0} &&
                                renamed_name ==
                                    QStringLiteral("renamed-entry") &&
                                renamed_folder != nullptr &&
                                renamed_folder->childCount() == 1 &&
                                renamed_folder->child(0)->text(0) ==
                                    renamed_name;
                            if (!renamed) {
                                completion(false);
                                return;
                            }
                            connect(
                                this, &MainWindow::AddFavoriteRequested, this,
                                [this, renamed, initial_count,
                                 completion = std::move(completion)](
                                    const QString& second_word,
                                    const QList<int>&
                                        second_parent_path) mutable {
                                    auto* folder_after_add =
                                        favorites_tree_->topLevelItem(
                                            initial_count);
                                    const bool second_added =
                                        renamed &&
                                        second_word ==
                                            QStringLiteral("second-entry") &&
                                        second_parent_path ==
                                            QList<int>{initial_count} &&
                                        folder_after_add != nullptr &&
                                        folder_after_add->childCount() == 2 &&
                                        folder_after_add->child(0)->text(0) ==
                                            QStringLiteral("renamed-entry") &&
                                        folder_after_add->child(1)->text(0) ==
                                            second_word;
                                    if (!second_added) {
                                        completion(false);
                                        return;
                                    }
                                    connect(
                                        this,
                                        &MainWindow::MoveFavoriteRequested,
                                        this,
                                        [this, second_added, initial_count,
                                         completion = std::move(completion)](
                                            const QList<int>& move_path,
                                            int offset) mutable {
                                            auto* folder_after_move =
                                                favorites_tree_->topLevelItem(
                                                    initial_count);
                                            const bool moved =
                                                second_added && offset == -1 &&
                                                move_path ==
                                                    QList<int>{initial_count,
                                                               1} &&
                                                folder_after_move != nullptr &&
                                                folder_after_move
                                                        ->childCount() == 2 &&
                                                folder_after_move->child(0)
                                                        ->text(0) ==
                                                    QStringLiteral(
                                                        "second-entry") &&
                                                folder_after_move->child(1)
                                                        ->text(0) ==
                                                    QStringLiteral(
                                                        "renamed-entry");
                                            if (!moved) {
                                                completion(false);
                                                return;
                                            }
                                            connect(
                                                this,
                                                &MainWindow::
                                                    MoveFavoriteToRootRequested,
                                                this,
                                                [this, moved, initial_count,
                                                 completion =
                                                     std::move(completion)](
                                                    const QList<int>&
                                                        root_path) mutable {
                                                    auto* root_item =
                                                        favorites_tree_
                                                            ->topLevelItem(
                                                                initial_count +
                                                                1);
                                                    const bool moved_to_root =
                                                        moved &&
                                                        root_path ==
                                                            QList<int>{
                                                                initial_count,
                                                                0} &&
                                                        root_item != nullptr &&
                                                        root_item->text(0) ==
                                                            QStringLiteral(
                                                                "second-entry");
                                                    connect(
                                                        this,
                                                        &MainWindow::
                                                            RemoveFavoriteRequested,
                                                        this,
                                                        [this, moved_to_root,
                                                         initial_count,
                                                         completion = std::move(
                                                             completion)](
                                                            const QList<int>&
                                                                root_remove_path) mutable {
                                                            const bool root_removed =
                                                                moved_to_root &&
                                                                root_remove_path ==
                                                                    QList<int>{
                                                                        initial_count +
                                                                        1} &&
                                                                favorites_tree_
                                                                        ->topLevelItemCount() ==
                                                                    initial_count +
                                                                        1;
                                                            connect(
                                                                this,
                                                                &MainWindow::
                                                                    RemoveFavoriteRequested,
                                                                this,
                                                                [this,
                                                                 root_removed,
                                                                 initial_count,
                                                                 completion =
                                                                     std::move(
                                                                         completion)](
                                                                    const QList<
                                                                        int>&
                                                                        folder_remove_path) mutable {
                                                                    completion(
                                                                        root_removed &&
                                                                        folder_remove_path ==
                                                                            QList<
                                                                                int>{
                                                                                initial_count} &&
                                                                        favorites_tree_
                                                                                ->topLevelItemCount() ==
                                                                            initial_count);
                                                                },
                                                                Qt::SingleShotConnection);
                                                            emit RemoveFavoriteRequested(
                                                                {initial_count});
                                                        },
                                                        Qt::SingleShotConnection);
                                                    emit
                                                        RemoveFavoriteRequested(
                                                            {initial_count +
                                                             1});
                                                },
                                                Qt::SingleShotConnection);
                                            emit MoveFavoriteToRootRequested(
                                                {initial_count, 0});
                                        },
                                        Qt::SingleShotConnection);
                                    favorites_tree_->setCurrentItem(
                                        folder_after_add->child(1));
                                    move_favorite_up_action_->trigger();
                                },
                                Qt::SingleShotConnection);
                            emit AddFavoriteRequested(
                                QStringLiteral("second-entry"),
                                {initial_count});
                        },
                        Qt::SingleShotConnection);
                    emit RenameFavoriteRequested(
                        {initial_count, 0}, QStringLiteral("renamed-entry"));
                },
                Qt::SingleShotConnection);
            query_->setText(expected);
            emit AddFavoriteRequested(expected, SelectedFavoriteFolderPath());
        },
        Qt::SingleShotConnection);
    emit AddFavoriteFolderRequested(QStringLiteral("Smoke Folder"), {});
}

void MainWindow::RunFavoritesCrossFolderMoveSmokeCheck(
    std::function<void(bool)> completion) {
    while (favorites_tree_->topLevelItemCount() > 0) {
        emit RemoveFavoriteRequested(
            {favorites_tree_->topLevelItemCount() - 1});
    }
    const int initial_count = favorites_tree_->topLevelItemCount();
    emit AddFavoriteFolderRequested(QStringLiteral("Source"), {});
    emit AddFavoriteFolderRequested(QStringLiteral("Target"), {});
    emit AddFavoriteFolderRequested(QStringLiteral("Subtree"), {initial_count});
    emit AddFavoriteRequested(QStringLiteral("nested-entry"),
                              {initial_count, 0});

    auto* source = favorites_tree_->topLevelItem(initial_count);
    auto* target = favorites_tree_->topLevelItem(initial_count + 1);
    if (source == nullptr || target == nullptr || source->childCount() != 1) {
        completion(false);
        return;
    }
    source->setExpanded(true);
    source->child(0)->setExpanded(true);
    connect(
        this, &MainWindow::MoveFavoriteAcrossFoldersRequested, this,
        [this, initial_count, completion = std::move(completion)](
            const QList<int>&, const QList<int>&, int,
            const QList<QList<int>>&) mutable {
            auto* refreshed_target =
                favorites_tree_->topLevelItem(initial_count + 1);
            const bool subtree_moved =
                refreshed_target != nullptr &&
                refreshed_target->childCount() == 1 &&
                refreshed_target->child(0)->text(0) ==
                    QStringLiteral("Subtree") &&
                refreshed_target->child(0)->isExpanded() &&
                favorites_tree_->currentItem() == refreshed_target->child(0);
            if (!subtree_moved) {
                completion(false);
                return;
            }
            connect(
                this, &MainWindow::MoveFavoriteAcrossFoldersRequested, this,
                [this, initial_count, completion = std::move(completion)](
                    const QList<int>&, const QList<int>&, int,
                    const QList<QList<int>>&) mutable {
                    auto* root_word =
                        favorites_tree_->topLevelItem(initial_count + 2);
                    completion(root_word != nullptr &&
                               root_word->text(0) ==
                                   QStringLiteral("nested-entry") &&
                               favorites_tree_->currentItem() == root_word &&
                               favorites_tree_->topLevelItem(initial_count + 1)
                                       ->child(0)
                                       ->childCount() == 0);
                },
                Qt::SingleShotConnection);
            emit MoveFavoriteAcrossFoldersRequested(
                {initial_count + 1, 0, 0}, {}, initial_count + 2,
                ExpandedFavoriteFolderPaths());
        },
        Qt::SingleShotConnection);
    emit MoveFavoriteAcrossFoldersRequested({initial_count, 0},
                                            {initial_count + 1}, 0,
                                            ExpandedFavoriteFolderPaths());
}

void MainWindow::RunFavoritesTransferSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    const int initial_count = favorites_tree_->topLevelItemCount();
    connect(
        this, &MainWindow::AddFavoriteFolderRequested, this,
        [this, path, initial_count, completion = std::move(completion)](
            const QString& folder_name, const QList<int>& parent_path) mutable {
            auto* folder = favorites_tree_->topLevelItem(initial_count);
            const bool folder_added =
                folder_name == QStringLiteral("Transfer Folder") &&
                parent_path.isEmpty() && folder != nullptr;
            connect(
                this, &MainWindow::ExportFavoritesRequested, this,
                [this, path, folder_added, folder, initial_count,
                 completion = std::move(completion)](
                    const QString& export_path) mutable {
                    const bool exported = folder_added && export_path == path &&
                                          QFileInfo(export_path).size() > 0;
                    connect(
                        this, &MainWindow::RemoveFavoriteRequested, this,
                        [this, path, exported, initial_count,
                         completion = std::move(completion)](
                            const QList<int>& remove_path) mutable {
                            const bool removed =
                                exported &&
                                remove_path == QList<int>{initial_count} &&
                                favorites_tree_->topLevelItemCount() ==
                                    initial_count;
                            connect(
                                this, &MainWindow::ImportFavoritesRequested,
                                this,
                                [this, path, removed, initial_count,
                                 completion = std::move(completion)](
                                    const QString& import_path) mutable {
                                    auto* restored =
                                        favorites_tree_->topLevelItem(
                                            initial_count);
                                    completion(
                                        removed && import_path == path &&
                                        favorites_tree_->topLevelItemCount() ==
                                            initial_count + 1 &&
                                        restored != nullptr &&
                                        restored->text(0) ==
                                            QStringLiteral("Transfer Folder"));
                                },
                                Qt::SingleShotConnection);
                            emit ImportFavoritesRequested(path);
                        },
                        Qt::SingleShotConnection);
                    favorites_tree_->setCurrentItem(folder);
                    favorite_removal_confirmation_ = []() {
                        return true;
                    };
                    remove_favorite_action_->trigger();
                    favorite_removal_confirmation_ = {};
                },
                Qt::SingleShotConnection);
            emit ExportFavoritesRequested(path);
        },
        Qt::SingleShotConnection);
    emit AddFavoriteFolderRequested(QStringLiteral("Transfer Folder"), {});
}

void MainWindow::RunDictionaryBrowserSmokeCheck(
    std::function<void(bool)> completion) {
    auto lookup_passed = std::make_shared<bool>(false);
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [lookup_passed](const QString& submitted, std::uint32_t) {
            *lookup_passed = submitted == QStringLiteral("application");
        },
        Qt::SingleShotConnection);
    ShowDictionaryBrowser();
    dictionary_browser_->RunSmokeCheck(
        QStringLiteral("Fixture Dictionary"), QStringLiteral("app"),
        QStringLiteral("application"),
        [lookup_passed,
         completion = std::move(completion)](bool browser_passed) mutable {
            completion(browser_passed && *lookup_passed);
        });
}

void MainWindow::RunDictionaryBrowserExportSmokeCheck(
    const QString& path, std::function<void(bool)> completion) {
    ShowDictionaryBrowser();
    auto* quiesced = new DictionaryBrowser(this);
    quiesced->SetBindingRegistry(facade_binding_registry_.get());
    quiesced->SetFacade(facade_);
    const bool quiesce_started = quiesced->StartLifecycleExportForTest(
        path + QStringLiteral(".quiesce"));
    quiesced->QuiesceBindingConsumer(false);
    quiesced->QuiesceBindingConsumer(false);
    delete quiesced;
    auto* destroyed = new DictionaryBrowser(this);
    destroyed->SetBindingRegistry(facade_binding_registry_.get());
    destroyed->SetFacade(facade_);
    const bool destruction_started = destroyed->StartLifecycleExportForTest(
        path + QStringLiteral(".destruction"));
    delete destroyed;
    facade_binding_registry_->ReclaimRetired();
    if (!quiesce_started || !destruction_started ||
        !facade_binding_registry_->AuditClosedLeaseProtocol()) {
        completion(false);
        return;
    }
    dictionary_browser_->RunExportSmokeCheck(
        path, QStringLiteral("app"),
        QByteArray::fromHex("efbbbf") + "00databaseshort\napple\napplication\n",
        [this, path, completion = std::move(completion)](bool passed) mutable {
            const bool shutdown_export_started =
                dictionary_browser_->StartLifecycleExportForTest(
                    path + QStringLiteral(".shutdown"));
            completion(passed && shutdown_export_started);
        });
}

MainWindow::~MainWindow() {
    if (widgets_maintenance_active_) {
        if (widgets_publication_decided_)
            FinishPublishedFacadeCommitInternal();
        else
            AbortMaintainedFacadeCommitInternal();
    }
    facade_preparation_shutdown_ = true;
    ++facade_preparation_generation_;
    AdvancePresentationMutationEpoch();
    if (facade_candidate_reclaimer_ != nullptr)
        facade_candidate_reclaimer_->stop();
    if (facade_binding_reclaimer_ != nullptr)
        facade_binding_reclaimer_->stop();
    if (facade_preparation_record_ != nullptr)
        facade_preparation_record_->owner.store(nullptr,
                                                std::memory_order_release);
    ReclaimFacadeCandidate(true);
    qApp->removeEventFilter(this);
    if (rendered_text_match_plan_controller_ != nullptr)
        rendered_text_match_plan_controller_->DetachConsumer();
    pending_rendered_text_match_plan_.reset();
    rendered_text_match_plans_.clear();
    rendered_page_text_transports_.clear();
    article_navigation_generations_.clear();
    pending_article_scroll_restorations_.clear();
    if (dictionary_browser_ != nullptr)
        dictionary_browser_->QuiesceBindingConsumer(true);
    if (full_text_search_dialog_ != nullptr)
        full_text_search_dialog_->DetachController();
    StopSuggestionWorker();
    for (auto& [id, request] : requests_) {
        static_cast<void>(id);
        request->Cancel();
    }
    QWebEngineProfile::defaultProfile()->removeUrlSchemeHandler(
        scheme_handler_);
    scheme_handler_->SetBindingRegistry(nullptr);
    if (facade_binding_registry_ != nullptr) {
        facade_binding_registry_->ClearPublished();
        facade_binding_registry_->ReclaimRetired();
        facade_binding_registry_->Shutdown();
    }
    if (active_facade_resources_ != nullptr) {
        active_facade_resources_->published = false;
        active_facade_resources_.reset();
    }
}

goldendict::core::ArticleTabId MainWindow::TabIdAt(int index) const {
    if (index < 0 || index >= article_tabs_->count())
        return 0U;
    return article_tabs_->widget(index)->property("articleTabId").toULongLong();
}

ArticleView* MainWindow::ArticleViewForTab(
    goldendict::core::ArticleTabId tab_id) const {
    for (int index = 0; index < article_tabs_->count(); ++index) {
        if (TabIdAt(index) == tab_id) {
            return qobject_cast<ArticleView*>(article_tabs_->widget(index));
        }
    }
    return nullptr;
}

ArticleView* MainWindow::CreateArticleView(
    goldendict::core::ArticleTabId tab_id) {
    auto* view = new ArticleView(article_tabs_);
    connect(view, &ArticleView::PageReplaced, this, [this, tab_id, view]() {
        if (ArticleViewForTab(tab_id) == view) {
            pending_article_scroll_restorations_.erase(tab_id);
            InvalidateArticleOutputOwnership(tab_id, view);
        }
    });
    view->SetFacade(facade_);
    view->SetClickPreferences(preferences_.double_click_translates,
                              preferences_.select_word_by_single_click);
    view->setProperty("articleTabId", QVariant::fromValue<qulonglong>(tab_id));
    auto* page = new ArticlePage(view);
    page->SetFacade(facade_);
    page->SetOpenNewTabsInBackground(preferences_.open_new_tabs_in_background);
    view->setPage(page);
    connect(page, &QWebEnginePage::scrollPositionChanged, this,
            [this](const QPointF&) { AdvancePresentationMutationEpoch(); });
    article_navigation_generations_.try_emplace(tab_id, 0U);
    connect(view, &ArticleView::loadStarted, this, [this, tab_id, view]() {
        if (ArticleViewForTab(tab_id) != view)
            return;
        InvalidateArticleOutputOwnership(tab_id, view);
        HandleArticleReloadStarted(tab_id, view);
        InvalidateRenderedTextMatchPlan(tab_id);
        ++article_navigation_generations_[tab_id];
        rendered_page_text_transports_.erase(tab_id);
    });
    connect(page, &ArticlePage::LookupRequested, this,
            [this, tab_id](const QString& text, const QString& internal_url,
                           ArticleLinkDisposition disposition) {
                static_cast<void>(text);
                OpenArticleLink(tab_id, QUrl(internal_url), disposition);
            });
    connect(page, &ArticlePage::ExternalUrlRequested, this,
            [](const QUrl& url) { QDesktopServices::openUrl(url); });
    connect(
        view, &ArticleView::LinkRequested, this,
        [this, tab_id](const QUrl& url, ArticleLinkDisposition disposition) {
            if (disposition == ArticleLinkDisposition::kNewForegroundTab &&
                preferences_.open_new_tabs_in_background) {
                disposition = ArticleLinkDisposition::kNewBackgroundTab;
            }
            OpenArticleLink(tab_id, url, disposition);
        });
    connect(view, &ArticleView::SelectionLookupRequested, this,
            [this, tab_id](const QString& text,
                           ArticleLinkDisposition disposition) {
                if (disposition == ArticleLinkDisposition::kNewForegroundTab &&
                    preferences_.open_new_tabs_in_background) {
                    disposition = ArticleLinkDisposition::kNewBackgroundTab;
                }
                LookupArticleSelection(tab_id, text, disposition);
            });
    connect(view, &ArticleView::SelectionToInputRequested, this,
            [this](const QString& text) { query_->setText(text); });
    connect(view, &ArticleView::ExternalUrlRequested, this,
            [](const QUrl& url) { QDesktopServices::openUrl(url); });
    connect(view, &ArticleView::DictionaryResultRequested, this,
            [this, tab_id, view](const QString& dictionary_id,
                                 int first_result_index, quint64 generation) {
                const auto found = lookup_results_.find(tab_id);
                if (found == lookup_results_.end() ||
                    found->second.generation != generation ||
                    ArticleViewForTab(tab_id) != view) {
                    return;
                }
                const auto row = std::find_if(
                    found->second.rows.begin(), found->second.rows.end(),
                    [&](const auto& item) {
                        return item.dictionary_id ==
                                   dictionary_id.toStdString() &&
                               item.first_result_index == first_result_index;
                    });
                if (row != found->second.rows.end())
                    NavigateToArticleResult(view, first_result_index);
            });
    connect(view, &ArticleView::DictionaryResultsPaneRequested, this,
            [this, tab_id, view](quint64 generation) {
                ShowDictionaryResultsPane(tab_id, view, generation);
            });
    connect(view, &ArticleView::urlChanged, this,
            &MainWindow::UpdateNavigationActions);
    connect(view, &ArticleView::loadFinished, this,
            [this, tab_id, view](bool success) {
                HandleArticleLoadFinished(tab_id, view, success);
            });
    connect(view, &ArticleView::HtmlNavigationFinished, this,
            [this, tab_id, view](quint64 navigation_token, bool success) {
                HandleArticleHtmlNavigationFinished(tab_id, view,
                                                    navigation_token, success);
            });
    connect(view, &ArticleView::printFinished, this,
            [this, view](bool success) { FinishPrinterRender(view, success); });
    connect(view, &ArticleView::FullTextNavigationRequested, this,
            [this, tab_id](ArticleHighlightNavigationDirection direction) {
                NavigateFullTextHighlight(tab_id, direction);
            });
    return view;
}

void MainWindow::ReloadCurrentArticle() {
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    auto* view = article_view_;
    if (tab_id == 0U || view == nullptr)
        return;
    pending_article_scroll_restorations_.erase(tab_id);
    auto& reload = article_reload_states_[tab_id];
    ++reload.generation;
    reload.view = view;
    if (!reload.in_flight_generation.has_value())
        StartPendingArticleReload(tab_id, view);
}

void MainWindow::StartPendingArticleReload(
    goldendict::core::ArticleTabId tab_id, ArticleView* view) {
    const auto reload = article_reload_states_.find(tab_id);
    if (reload == article_reload_states_.end() || reload->second.view != view ||
        ArticleViewForTab(tab_id) != view ||
        reload->second.in_flight_generation.has_value()) {
        return;
    }
    reload->second.in_flight_generation = reload->second.generation;
    reload->second.load_started = false;
    view->reload();
}

void MainWindow::HandleArticleReloadStarted(
    goldendict::core::ArticleTabId tab_id, ArticleView* view) {
    const auto reload = article_reload_states_.find(tab_id);
    if (reload == article_reload_states_.end() || reload->second.view != view ||
        !reload->second.in_flight_generation.has_value()) {
        return;
    }
    reload->second.load_started = true;
}

void MainWindow::HandleArticleLoadFinished(
    goldendict::core::ArticleTabId tab_id, ArticleView* view, bool success) {
    const auto reload = article_reload_states_.find(tab_id);
    if (reload != article_reload_states_.end()) {
        // A terminal completion for the current WebEngine load cannot arrive
        // while that page still reports loading.
        const bool current_load_finished =
            reload->second.in_flight_generation.has_value() &&
            reload->second.load_started && !view->page()->isLoading();
        if (reload->second.view != view || ArticleViewForTab(tab_id) != view) {
            article_reload_states_.erase(reload);
        } else if (current_load_finished) {
            const std::uint64_t completed_generation =
                *reload->second.in_flight_generation;
            reload->second.in_flight_generation.reset();
            reload->second.load_started = false;
            if (completed_generation != reload->second.generation) {
                const std::uint64_t pending_generation =
                    reload->second.generation;
                QTimer::singleShot(
                    0, this, [this, tab_id, view, pending_generation]() {
                        const auto pending =
                            article_reload_states_.find(tab_id);
                        if (pending == article_reload_states_.end() ||
                            pending->second.generation != pending_generation ||
                            pending->second.in_flight_generation.has_value() ||
                            pending->second.view != view ||
                            ArticleViewForTab(tab_id) != view) {
                            return;
                        }
                        StartPendingArticleReload(tab_id, view);
                    });
            } else {
                const auto search = article_search_presentations_.find(tab_id);
                if (success && search != article_search_presentations_.end() &&
                    !search->second.query.isEmpty()) {
                    const QString query = search->second.query;
                    const std::uint64_t generation =
                        ++search->second.generation;
                    DispatchArticleSearch(tab_id, view, query, generation,
                                          false);
                }
            }
        }
    }
}

void MainWindow::HandleArticleHtmlNavigationFinished(
    goldendict::core::ArticleTabId tab_id, ArticleView* view,
    quint64 navigation_token, bool success) {
    const auto position = TakePendingArticleScrollRestoration(
        tab_id, view, navigation_token, success);
    if (!position.has_value())
        return;
    const auto css_position =
        QtPageScrollToCssScroll(*position, view->zoomFactor());
    if (!css_position.has_value())
        return;
    view->page()->runJavaScript(QStringLiteral("window.scrollTo(%1,%2)")
                                    .arg(css_position->x(), 0, 'f', 6)
                                    .arg(css_position->y(), 0, 'f', 6));
    const auto search = article_search_presentations_.find(tab_id);
    if (search == article_search_presentations_.end() ||
        search->second.query.isEmpty()) {
        return;
    }
    const QString query = search->second.query;
    const std::uint64_t generation = ++search->second.generation;
    const auto navigation = article_navigation_generations_.find(tab_id);
    if (view->page() == nullptr ||
        navigation == article_navigation_generations_.end()) {
        return;
    }
    const QPointer<MainWindow> guarded_window(this);
    const QPointer<ArticleView> guarded_view(view);
    const QPointer<QWebEnginePage> guarded_page(view->page());
    const std::uint64_t navigation_generation = navigation->second;
    view->findText(
        query, {},
        [guarded_window, tab_id, guarded_view, guarded_page, query, generation,
         navigation_generation](const QWebEngineFindTextResult& result) {
            if (guarded_window.isNull())
                return;
            guarded_window->FinishArticleSearch(
                tab_id, guarded_view, guarded_page, navigation_generation,
                query, generation, result.activeMatch(),
                result.numberOfMatches());
        });
}

void MainWindow::BindPendingArticleScrollRestoration(
    goldendict::core::ArticleTabId tab_id, ArticleView* view,
    quint64 navigation_token) {
    const auto pending = pending_article_scroll_restorations_.find(tab_id);
    if (pending == pending_article_scroll_restorations_.end() ||
        view == nullptr || view->page() == nullptr ||
        ArticleViewForTab(tab_id) != view) {
        return;
    }
    pending->second.view = view;
    pending->second.page = view->page();
    pending->second.navigation_token = navigation_token;
}

void MainWindow::DeferPendingArticleScrollRestoration(
    goldendict::core::ArticleTabId tab_id, ArticleView* view) {
    const auto pending = pending_article_scroll_restorations_.find(tab_id);
    if (pending == pending_article_scroll_restorations_.end())
        return;
    pending->second.view = view;
    pending->second.page = view == nullptr ? nullptr : view->page();
    pending->second.navigation_token = 0U;
}

std::optional<QPointF> MainWindow::TakePendingArticleScrollRestoration(
    goldendict::core::ArticleTabId tab_id, ArticleView* view,
    quint64 navigation_token, bool success) {
    const auto pending = pending_article_scroll_restorations_.find(tab_id);
    if (pending == pending_article_scroll_restorations_.end())
        return std::nullopt;
    auto* const current_view = ArticleViewForTab(tab_id);
    if (current_view == nullptr || pending->second.view.isNull() ||
        pending->second.page.isNull() || pending->second.view != current_view ||
        pending->second.page != current_view->page()) {
        pending_article_scroll_restorations_.erase(pending);
        return std::nullopt;
    }
    if (current_view != view || pending->second.navigation_token == 0U ||
        pending->second.navigation_token != navigation_token) {
        return std::nullopt;
    }
    const QPointF position = pending->second.position;
    pending_article_scroll_restorations_.erase(pending);
    return success ? std::optional<QPointF>(position) : std::nullopt;
}

void MainWindow::PublishArticleHtml(goldendict::core::ArticleTabId tab_id,
                                    ArticleView* view, const QString& html,
                                    const QUrl& base_url) {
    if (view == nullptr)
        return;
    const quint64 navigation_token = view->ReserveHtmlNavigation();
    BindPendingArticleScrollRestoration(tab_id, view, navigation_token);
    view->SetHtmlNavigation(navigation_token, html, base_url);
}

void MainWindow::OpenArticleLink(goldendict::core::ArticleTabId tab_id,
                                 const QUrl& url,
                                 ArticleLinkDisposition disposition) {
    if (facade_ == nullptr)
        return;
    const auto resolved =
        facade_->ResolveArticleUrl(url.toEncoded().toStdString());
    if (!resolved.has_value() ||
        resolved->kind != goldendict::core::ArticleUrlKind::kLookup) {
        return;
    }
    const auto state = facade_->GetArticleTabsState();
    const auto found =
        std::find_if(state.tabs.begin(), state.tabs.end(),
                     [tab_id](const auto& tab) { return tab.id == tab_id; });
    if (found == state.tabs.end())
        return;
    goldendict::core::TabNavigationState navigation;
    navigation.kind = goldendict::core::TabNavigationKind::kInternalLink;
    navigation.query = resolved->lookup_text;
    navigation.group_id = found->navigation.group_id;
    navigation.title = navigation.query;
    navigation.internal_url = url.toEncoded().toStdString();
    const bool new_tab = disposition != ArticleLinkDisposition::kCurrentTab;
    const bool background =
        disposition == ArticleLinkDisposition::kNewBackgroundTab;
    const auto result = facade_->OpenArticleTab(
        navigation,
        new_tab ? goldendict::core::TabOpenPolicy::kNewTab
                : goldendict::core::TabOpenPolicy::kCurrentTab,
        background ? goldendict::core::TabActivationPolicy::kKeepActive
                   : goldendict::core::TabActivationPolicy::kActivate,
        NewTabPlacementPolicy());
    if (!result) {
        status_->setText(
            result.error == goldendict::core::TabOperationError::
                                kInvalidNavigation &&
                    preferences_.limit_input_phrase_length
                ? QStringLiteral(
                      "Input phrase exceeds the configured %1-symbol limit")
                      .arg(preferences_.input_phrase_length_limit)
                : QStringLiteral("Unable to open article tab"));
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    StartNavigationLookup(result.tab_id, navigation, true);
}

void MainWindow::LookupArticleSelection(goldendict::core::ArticleTabId tab_id,
                                        const QString& text,
                                        ArticleLinkDisposition disposition) {
    const QString phrase = text.trimmed();
    if (facade_ == nullptr || phrase.isEmpty()) {
        return;
    }
    const auto state = facade_->GetArticleTabsState();
    const auto found =
        std::find_if(state.tabs.begin(), state.tabs.end(),
                     [tab_id](const auto& tab) { return tab.id == tab_id; });
    if (found == state.tabs.end())
        return;
    goldendict::core::TabNavigationState navigation;
    navigation.kind = goldendict::core::TabNavigationKind::kLookup;
    navigation.query = text.toStdString();
    navigation.group_id = found->navigation.group_id;
    navigation.title = navigation.query;
    const bool new_tab = disposition != ArticleLinkDisposition::kCurrentTab;
    const bool background =
        disposition == ArticleLinkDisposition::kNewBackgroundTab;
    const auto result = facade_->OpenArticleTab(
        navigation,
        new_tab ? goldendict::core::TabOpenPolicy::kNewTab
                : goldendict::core::TabOpenPolicy::kCurrentTab,
        background ? goldendict::core::TabActivationPolicy::kKeepActive
                   : goldendict::core::TabActivationPolicy::kActivate,
        NewTabPlacementPolicy());
    if (!result) {
        if (preferences_.limit_input_phrase_length) {
            status_->setText(
                QStringLiteral("Input phrase exceeds the configured "
                               "%1-symbol limit")
                    .arg(preferences_.input_phrase_length_limit));
        }
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    StartNavigationLookup(result.tab_id, navigation, true);
}

void MainWindow::SyncArticleTabs() {
    if (facade_ == nullptr)
        return;
    const auto state = facade_->GetArticleTabsState();
    const QSignalBlocker blocker(article_tabs_);
    for (int index = article_tabs_->count() - 1; index >= 0; --index) {
        const auto id = TabIdAt(index);
        const bool retained =
            std::any_of(state.tabs.begin(), state.tabs.end(),
                        [id](const auto& tab) { return tab.id == id; });
        if (!retained) {
            if (auto request = requests_.find(id); request != requests_.end()) {
                request->second->Cancel();
                requests_.erase(request);
            }
            lookup_results_.erase(id);
            suggestions_.erase(id);
            article_search_presentations_.erase(id);
            article_reload_states_.erase(id);
            pending_article_search_handoffs_.erase(id);
            InvalidateRenderedTextMatchPlan(id);
            article_navigation_generations_.erase(id);
            rendered_page_text_transports_.erase(id);
            pending_article_scroll_restorations_.erase(id);
            QWidget* widget = article_tabs_->widget(index);
            article_tabs_->removeTab(index);
            widget->deleteLater();
        }
    }
    for (std::size_t desired = 0; desired < state.tabs.size(); ++desired) {
        const auto& tab = state.tabs[desired];
        auto* view = ArticleViewForTab(tab.id);
        bool created = false;
        if (view == nullptr) {
            view = CreateArticleView(tab.id);
            article_tabs_->insertTab(
                static_cast<int>(desired), view,
                QString::fromStdString(tab.navigation.title));
            created = true;
        }
        const int current = article_tabs_->indexOf(view);
        if (current != static_cast<int>(desired)) {
            article_tabs_->removeTab(current);
            article_tabs_->insertTab(
                static_cast<int>(desired), view,
                QString::fromStdString(tab.navigation.title));
        }
        article_tabs_->setTabText(
            static_cast<int>(desired),
            QString::fromStdString(tab.navigation.title).replace('&', "&&"));
        if (created) {
            PublishArticleHtml(
                tab.id, view,
                QStringLiteral("<!doctype html><html><body><h1>GoldenDict</h1>"
                               "<p>Choose a dictionary folder to "
                               "begin.</p></body></html>"));
        }
    }
    const auto active = std::find_if(
        state.tabs.begin(), state.tabs.end(),
        [&](const auto& tab) { return tab.id == state.active_tab_id; });
    if (active == state.tabs.end())
        return;
    auto* active_view = ArticleViewForTab(active->id);
    article_tabs_->setCurrentWidget(active_view);
    article_view_ = active_view;
    article_page_ = qobject_cast<ArticlePage*>(active_view->page());
    RefreshArticleSearch();
    query_->setText(QString::fromStdString(active->navigation.query));
    SelectGroup(active->navigation.group_id);
    setWindowTitle(
        active->navigation.kind == goldendict::core::TabNavigationKind::kEmpty
            ? QStringLiteral("GoldenDict")
            : QStringLiteral("%1 — GoldenDict")
                  .arg(QString::fromStdString(active->navigation.title)));
    UpdateNavigationActions();
    RefreshResultsNavigation();
    RefreshSuggestions();
    ReconcileMruTabIds();
}

void MainWindow::ReconcileMruTabIds(bool rebuild) {
    if (facade_ == nullptr) {
        mru_tab_ids_.clear();
        mru_traversal_ids_.clear();
        mru_traversal_active_ = false;
        return;
    }
    const auto state = facade_->GetArticleTabsState();
    const auto is_valid = [&state](goldendict::core::ArticleTabId id) {
        return std::any_of(state.tabs.begin(), state.tabs.end(),
                           [id](const auto& tab) { return tab.id == id; });
    };
    if (rebuild) {
        mru_tab_ids_.clear();
        mru_traversal_ids_.clear();
        mru_traversal_active_ = false;
    } else {
        mru_tab_ids_.erase(
            std::remove_if(mru_tab_ids_.begin(), mru_tab_ids_.end(),
                           [&](auto id) { return !is_valid(id); }),
            mru_tab_ids_.end());
        mru_traversal_ids_.erase(
            std::remove_if(mru_traversal_ids_.begin(), mru_traversal_ids_.end(),
                           [&](auto id) { return !is_valid(id); }),
            mru_traversal_ids_.end());
    }
    for (const auto& tab : state.tabs) {
        if (std::find(mru_tab_ids_.begin(), mru_tab_ids_.end(), tab.id) ==
            mru_tab_ids_.end()) {
            mru_tab_ids_.push_back(tab.id);
        }
    }
    if (!mru_traversal_active_) {
        const auto active = std::find(mru_tab_ids_.begin(), mru_tab_ids_.end(),
                                      state.active_tab_id);
        if (active != mru_tab_ids_.end()) {
            std::rotate(mru_tab_ids_.begin(), active, std::next(active));
        }
    }
    Q_ASSERT(mru_tab_ids_.size() == state.tabs.size());
    Q_ASSERT(mru_tab_ids_.size() <= goldendict::core::kMaximumArticleTabs);
}

void MainWindow::TraverseArticleTabs(bool forward) {
    if (facade_ == nullptr || article_tabs_->count() < 2)
        return;
    if (!preferences_.mru_tab_order) {
        const int count = article_tabs_->count();
        const int offset = forward ? 1 : count - 1;
        ActivateArticleTab((article_tabs_->currentIndex() + offset) % count);
        return;
    }
    ReconcileMruTabIds();
    if (!mru_traversal_active_) {
        mru_traversal_ids_ = mru_tab_ids_;
        mru_traversal_active_ = true;
    }
    const auto active_id = facade_->GetArticleTabsState().active_tab_id;
    const auto active = std::find(mru_traversal_ids_.begin(),
                                  mru_traversal_ids_.end(), active_id);
    if (active == mru_traversal_ids_.end() || mru_traversal_ids_.empty()) {
        FinishMruTraversal();
        return;
    }
    const auto current = static_cast<std::size_t>(
        std::distance(mru_traversal_ids_.begin(), active));
    const auto count = mru_traversal_ids_.size();
    const auto target =
        forward ? (current + 1U) % count : (current + count - 1U) % count;
    const auto state = facade_->GetArticleTabsState();
    const auto valid_tab = std::find_if(
        state.tabs.begin(), state.tabs.end(), [&](const auto& item) {
            return item.id == mru_traversal_ids_[target];
        });
    if (valid_tab != state.tabs.end() &&
        facade_->ActivateArticleTab(valid_tab->id)) {
        SyncArticleTabs();
        emit ArticleTabSessionMutated();
    }
}

void MainWindow::FinishMruTraversal() {
    if (!mru_traversal_active_)
        return;
    mru_traversal_active_ = false;
    mru_traversal_ids_.clear();
    ReconcileMruTabIds();
}

void MainWindow::RebuildArticleTabs() {
    if (facade_ == nullptr)
        return;
    SyncArticleTabs();
    const auto state = facade_->GetArticleTabsState();
    for (const auto& tab : state.tabs) {
        if (tab.navigation.kind !=
            goldendict::core::TabNavigationKind::kEmpty) {
            StartNavigationLookup(tab.id, tab.navigation, false);
        }
    }
}

void MainWindow::ActivateArticleTab(int index) {
    if (facade_ == nullptr || index < 0)
        return;
    const auto id = TabIdAt(index);
    if (id != 0U && facade_->ActivateArticleTab(id)) {
        SyncArticleTabs();
        emit ArticleTabSessionMutated();
    }
}

void MainWindow::CloseArticleTab(int index) {
    if (facade_ == nullptr || index < 0)
        return;
    const auto id = TabIdAt(index);
    if (!facade_->CloseArticleTab(id)) {
        status_->setText(QStringLiteral("Unable to close article tab"));
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
}

void MainWindow::CloseOtherArticleTabs(int index) {
    if (facade_ == nullptr || index < 0)
        return;
    if (facade_->CloseOtherArticleTabs(TabIdAt(index))) {
        SyncArticleTabs();
        emit ArticleTabSessionMutated();
    }
}

void MainWindow::CreateEmptyArticleTab(bool activate) {
    if (facade_ == nullptr)
        return;
    goldendict::core::TabNavigationState navigation;
    navigation.title = "(untitled)";
    const auto result = facade_->OpenArticleTab(
        navigation, goldendict::core::TabOpenPolicy::kNewTab,
        activate ? goldendict::core::TabActivationPolicy::kActivate
                 : goldendict::core::TabActivationPolicy::kKeepActive,
        NewTabPlacementPolicy());
    if (!result) {
        status_->setText(QStringLiteral("Article tab limit reached"));
        return;
    }
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
}

goldendict::core::TabPlacementPolicy MainWindow::NewTabPlacementPolicy() const {
    return preferences_.open_new_tabs_after_current
               ? goldendict::core::TabPlacementPolicy::kAfterActive
               : goldendict::core::TabPlacementPolicy::kAppend;
}

void MainWindow::NavigateArticleTab(bool forward) {
    if (facade_ == nullptr)
        return;
    const auto id = TabIdAt(article_tabs_->currentIndex());
    const auto result = forward ? facade_->GoForwardInArticleTab(id)
                                : facade_->GoBackInArticleTab(id);
    if (!result)
        return;
    const auto state = facade_->GetArticleTabsState();
    const auto tab =
        std::find_if(state.tabs.begin(), state.tabs.end(),
                     [id](const auto& item) { return item.id == id; });
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    if (tab != state.tabs.end() &&
        tab->navigation.kind != goldendict::core::TabNavigationKind::kEmpty) {
        StartNavigationLookup(id, tab->navigation, false);
    } else {
        if (auto request = requests_.find(id); request != requests_.end()) {
            request->second->Cancel();
            requests_.erase(request);
        }
        if (auto* view = ArticleViewForTab(id); view != nullptr) {
            view->setHtml(QStringLiteral(
                "<!doctype html><html><body><h1>GoldenDict</h1>"
                "<p>Choose a dictionary folder to begin.</p></body></html>"));
        }
    }
}

void MainWindow::ShowTabContextMenu(const QPoint& position) {
    const int index = article_tabs_->tabBar()->tabAt(position);
    if (index < 0)
        return;
    QMenu menu(this);
    auto* close = menu.addAction(QStringLiteral("Close Tab"));
    auto* close_others = menu.addAction(QStringLiteral("Close Other Tabs"));
    close_others->setEnabled(article_tabs_->count() > 1);
    QAction* selected =
        menu.exec(article_tabs_->tabBar()->mapToGlobal(position));
    if (selected == close)
        CloseArticleTab(index);
    if (selected == close_others)
        CloseOtherArticleTabs(index);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (WidgetsInteractionBlocked()) {
        const auto type = event->type();
        if (type == QEvent::KeyPress || type == QEvent::KeyRelease ||
            type == QEvent::MouseButtonPress ||
            type == QEvent::MouseButtonRelease || type == QEvent::Wheel ||
            type == QEvent::Shortcut || type == QEvent::ShortcutOverride ||
            type == QEvent::FocusIn || type == QEvent::FocusOut) {
            return true;
        }
    }
    const auto* watched_widget = qobject_cast<QWidget*>(watched);
    const bool main_window_key =
        watched_widget != nullptr && watched_widget->window() == this;
    if (main_window_key && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Tab &&
            key->modifiers() == Qt::ControlModifier) {
            TraverseArticleTabs(true);
            return true;
        }
        if ((key->key() == Qt::Key_Backtab || key->key() == Qt::Key_Tab) &&
            key->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
            TraverseArticleTabs(false);
            return true;
        }
    }
    if (main_window_key && event->type() == QEvent::KeyRelease) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Control)
            FinishMruTraversal();
    }
    if (watched == query_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::MoveToNextLine) &&
            suggestions_list_->count() > 0) {
            suggestions_list_->setFocus(Qt::ShortcutFocusReason);
            suggestions_list_->setCurrentRow(0);
            return true;
        }
    }
    if (watched == suggestions_list_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::MoveToPreviousLine) &&
            suggestions_list_->currentRow() == 0) {
            suggestions_list_->clearSelection();
            query_->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            suggestions_list_->currentItem() != nullptr) {
            ActivateSuggestion();
            return true;
        }
    }
    if (watched == article_tabs_->tabBar() &&
        event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::MiddleButton) {
            const int index =
                article_tabs_->tabBar()->tabAt(mouse->position().toPoint());
            if (index >= 0)
                CloseArticleTab(index);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape &&
        event->modifiers() == Qt::NoModifier &&
        preferences_.escape_hides_main_window) {
        hide();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
#if defined(Q_OS_LINUX)
    if (event->button() == Qt::MiddleButton &&
        HandleLinuxPrimarySelectionMousePress(
            event, QApplication::clipboard()->text(QClipboard::Selection))) {
        return;
    }
#endif
    QMainWindow::mousePressEvent(event);
}

#if defined(Q_OS_LINUX)
bool MainWindow::HandleLinuxPrimarySelectionMousePress(
    QMouseEvent* event, const QString& selection) {
    if (event->button() != Qt::MiddleButton) {
        return false;
    }
    SubmitInitialLookup(selection);
    event->accept();
    return true;
}
#endif

void MainWindow::SetDictionaryGroups(
    const std::vector<goldendict::core::DictionaryGroupConfiguration>& groups) {
    AdvancePresentationMutationEpoch();
    groups_ = groups;
    RefreshGroupSelector();
    RefreshDictionaryBar();
}

void MainWindow::SetSourceDirectories(
    const std::vector<std::string>& dictionary_paths,
    const std::vector<goldendict::core::SoundDirectoryConfiguration>&
        sound_directories) {
    dictionary_paths_ = dictionary_paths;
    sound_directories_ = sound_directories;
}

void MainWindow::SetOnlineSources(
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
    SourceApplyCallback apply_callback) {
    mediawiki_sources_ = mediawiki_sources;
    website_sources_ = website_sources;
    forvo_sources_ = forvo_sources;
    forvo_credentials_ = forvo_credentials;
    dict_server_sources_ = dict_server_sources;
    external_program_sources_ = external_program_sources;
    if (apply_callback)
        source_apply_callback_ = std::move(apply_callback);
}

void MainWindow::SetHistoryExportCallback(HistoryExportCallback callback) {
    history_export_callback_ = std::move(callback);
}

const std::vector<goldendict::core::DictionaryGroupConfiguration>&
MainWindow::DictionaryGroups() const noexcept {
    return groups_;
}

void MainWindow::SelectGroup(std::uint32_t group_id) {
    for (int index = 0; index < group_selector_->count(); ++index) {
        if (group_selector_->itemData(index).toUInt() == group_id) {
            group_selector_->setCurrentIndex(index);
            selected_group_id_ = group_id;
            return;
        }
    }
    group_selector_->setCurrentIndex(0);
    selected_group_id_ = 0U;
}

void MainWindow::RefreshGroupSelector() {
    const std::uint32_t previous = selected_group_id_;
    for (auto* shortcut : group_shortcuts_)
        delete shortcut;
    group_shortcuts_.clear();
    group_selector_->clear();
    group_selector_->addItem(QStringLiteral("All Dictionaries"),
                             QVariant::fromValue<quint32>(0U));
    for (const auto& group : groups_) {
        QIcon icon;
        if (!group.encoded_icon_data.empty()) {
            const QByteArray bytes = QByteArray::fromBase64(
                QByteArray::fromStdString(group.encoded_icon_data));
            QPixmap pixmap;
            if (pixmap.loadFromData(bytes))
                icon = QIcon(pixmap);
        }
        group_selector_->addItem(icon, QString::fromStdString(group.name),
                                 QVariant::fromValue<quint32>(group.id));
        const QKeySequence sequence(QString::fromStdString(group.shortcut));
        if (!sequence.isEmpty()) {
            auto* shortcut = new QShortcut(sequence, this);
            const std::uint32_t id = group.id;
            connect(shortcut, &QShortcut::activated, this,
                    [this, id]() { SelectGroup(id); });
            group_shortcuts_.push_back(shortcut);
        }
    }
    SelectGroup(previous);
    group_selector_host_->RefreshPreservedWidth();
}

std::vector<std::string> MainWindow::ParticipatingDictionaryIds(
    std::uint32_t group_id) const {
    const auto found = participating_ids_.find(group_id);
    return found == participating_ids_.end() ? std::vector<std::string>{}
                                             : found->second;
}

void MainWindow::RefreshDictionaryBar() {
    if (dictionary_bar_ == nullptr)
        return;
    const auto catalog =
        facade_ == nullptr ? std::vector<goldendict::core::DictionaryIdentity>{}
                           : facade_->GetDictionaryService().GetCatalog();
    std::map<std::string, goldendict::core::DictionaryIdentity> identities;
    for (const auto& dictionary : catalog)
        identities.emplace(dictionary.id, dictionary);

    std::vector<std::string> members;
    std::vector<std::string> baseline;
    if (selected_group_id_ == 0U) {
        for (const auto& dictionary : catalog) {
            members.push_back(dictionary.id);
            baseline.push_back(dictionary.id);
        }
    } else {
        const auto group = std::find_if(
            groups_.begin(), groups_.end(), [this](const auto& candidate) {
                return candidate.id == selected_group_id_;
            });
        if (group == groups_.end()) {
            SelectGroup(0U);
            return;
        }
        for (const auto& id : group->dictionary_ids) {
            if (identities.count(id) == 0U)
                continue;
            members.push_back(id);
            if (std::find(group->muted_dictionary_ids.begin(),
                          group->muted_dictionary_ids.end(),
                          id) == group->muted_dictionary_ids.end()) {
                baseline.push_back(id);
            }
        }
    }

    auto previous = participating_ids_.find(selected_group_id_);
    const auto previous_members = dictionary_members_.find(selected_group_id_);
    std::vector<std::string> reconciled;
    for (const auto& id : members) {
        const bool was_member = previous_members != dictionary_members_.end() &&
                                std::find(previous_members->second.begin(),
                                          previous_members->second.end(),
                                          id) != previous_members->second.end();
        const bool was_enabled =
            previous != participating_ids_.end() &&
            std::find(previous->second.begin(), previous->second.end(), id) !=
                previous->second.end();
        const bool baseline_enabled =
            std::find(baseline.begin(), baseline.end(), id) != baseline.end();
        if ((was_member && was_enabled) || (!was_member && baseline_enabled))
            reconciled.push_back(id);
    }
    participating_ids_[selected_group_id_] = reconciled;
    dictionary_members_[selected_group_id_] = members;

    dictionary_bar_host_->ClearActive();
    for (const auto& id : members) {
        const auto& identity = identities.at(id);
        const QString label = QString::fromStdString(
            identity.name.empty() ? identity.id : identity.name);
        auto* action = dictionary_bar_host_->AddAction(
            dictionary_bar_host_->ActivePage(), label);
        action->setCheckable(true);
        action->setChecked(std::find(reconciled.begin(), reconciled.end(),
                                     id) != reconciled.end());
        action->setData(QString::fromStdString(id));
        action->setToolTip(label);
        action->setWhatsThis(
            tr("Include %1 in dictionary lookups and suggestions").arg(label));
        if (auto* widget =
                dictionary_bar_host_->ActiveWidgetForAction(action)) {
            widget->setAccessibleName(label);
            widget->setToolTip(label);
        }
        connect(action, &QAction::triggered, this, [this, id](bool checked) {
            ApplyPreparedDictionaryAction(id, checked,
                                          QApplication::keyboardModifiers());
        });
    }
}

void MainWindow::ApplyPreparedDictionaryAction(
    const std::string& dictionary_id, bool checked,
    Qt::KeyboardModifiers modifiers) {
    auto& enabled = participating_ids_[selected_group_id_];
    if (modifiers.testFlag(Qt::ControlModifier) ||
        modifiers.testFlag(Qt::ShiftModifier)) {
        const bool was_solo =
            enabled.size() == 1U && enabled.front() == dictionary_id;
        if (was_solo) {
            if (modifiers.testFlag(Qt::ShiftModifier)) {
                enabled = solo_restore_ids_[selected_group_id_];
            } else {
                enabled.clear();
                for (auto* candidate : dictionary_bar_host_->ActiveActions())
                    enabled.push_back(
                        candidate->data().toString().toStdString());
            }
            solo_restore_ids_.erase(selected_group_id_);
        } else {
            solo_restore_ids_[selected_group_id_] = enabled;
            enabled = {dictionary_id};
        }
    } else {
        solo_restore_ids_.erase(selected_group_id_);
        const auto found =
            std::find(enabled.begin(), enabled.end(), dictionary_id);
        if (checked && found == enabled.end())
            enabled.push_back(dictionary_id);
        else if (!checked && found != enabled.end())
            enabled.erase(found);
    }
    const auto ordered_actions = dictionary_bar_host_->ActiveActions();
    std::vector<std::string> ordered;
    for (auto* candidate : ordered_actions) {
        const auto candidate_id = candidate->data().toString().toStdString();
        const bool participates = std::find(enabled.begin(), enabled.end(),
                                            candidate_id) != enabled.end();
        candidate->setChecked(participates);
        if (participates)
            ordered.push_back(candidate_id);
    }
    enabled = std::move(ordered);
    AdvancePresentationMutationEpoch();
    ApplyDictionaryParticipation();
}

void MainWindow::ApplyDictionaryFilter(
    goldendict::core::LookupQuery* query) const {
    if (dictionary_bar_ == nullptr || !dictionary_bar_->isVisible())
        return;
    query->dictionary_filter_active = true;
    query->dictionary_ids = ParticipatingDictionaryIds(query->group_id);
}

void MainWindow::ApplyNavigationDictionaryFilter(
    goldendict::core::LookupQuery* query,
    const goldendict::core::TabNavigationState& navigation) const {
    if (navigation.dictionary_filter_active) {
        query->dictionary_filter_active = true;
        query->dictionary_ids = navigation.dictionary_ids;
        return;
    }
    ApplyDictionaryFilter(query);
}

void MainWindow::ApplyDictionaryFilter(
    goldendict::core::SuggestionQuery* query) const {
    if (dictionary_bar_ == nullptr || !dictionary_bar_->isVisible())
        return;
    query->dictionary_filter_active = true;
    query->dictionary_ids = ParticipatingDictionaryIds(query->group_id);
}

goldendict::core::FullTextQuery MainWindow::ComposeFullTextQuery(
    const goldendict::app::FullTextQueryComposer& composer) const {
    const auto catalog =
        facade_ == nullptr ? std::vector<goldendict::core::DictionaryIdentity>{}
                           : facade_->GetDictionaryService().GetCatalog();
    const goldendict::core::DictionaryGroupConfiguration* selected_group =
        nullptr;
    if (selected_group_id_ != 0U) {
        const auto group = std::find_if(
            groups_.begin(), groups_.end(), [this](const auto& candidate) {
                return candidate.id == selected_group_id_;
            });
        if (group != groups_.end()) {
            selected_group = &*group;
        }
    }
    return goldendict::app::ProjectFullTextDictionaries(
        composer.Compose(), catalog, selected_group,
        dictionary_bar_ != nullptr && dictionary_bar_->isVisible(),
        ParticipatingDictionaryIds(selected_group_id_));
}

void MainWindow::ShowFullTextSearch() {
    if (facade_ == nullptr)
        return;
    if (full_text_search_dialog_ == nullptr) {
        full_text_search_dialog_ = new goldendict::app::FullTextSearchDialog(
            preferences_, &facade_->GetDictionaryService(),
            full_text_dialog_geometry_, this);
        full_text_search_dialog_->SetBindingRegistry(
            facade_binding_registry_.get());
        connect(
            full_text_search_dialog_,
            &goldendict::app::FullTextSearchDialog::ResultActivationRequested,
            this, &MainWindow::NavigateToFullTextResult);
        connect(
            full_text_search_dialog_,
            &goldendict::app::FullTextSearchDialog::AcceptedQueryInvalidated,
            this, [this]() {
                pending_article_search_handoffs_.clear();
                rendered_page_text_transports_.clear();
                InvalidateRenderedTextMatchPlan();
                for (auto& [tab_id, presentation] :
                     article_search_presentations_) {
                    static_cast<void>(tab_id);
                    presentation.accepted_query_generation = 0U;
                    ++presentation.generation;
                }
            });
        connect(full_text_search_dialog_,
                &goldendict::app::FullTextSearchDialog::GeometryCaptured, this,
                [this](std::string geometry) {
                    emit FullTextDialogGeometryCaptured(std::move(geometry));
                });
        full_text_search_dialog_->InitializeQuery(query_->text());
    }
#if defined(Q_OS_LINUX)
    if (help_connected_full_text_dialog_ != full_text_search_dialog_) {
        connect(full_text_search_dialog_,
                &goldendict::app::FullTextSearchDialog::HelpRequested, this,
                [this]() {
                    ShowHelp(goldendict::app::HelpIntent::kFullTextSearch);
                });
        help_connected_full_text_dialog_ = full_text_search_dialog_;
    }
#endif
    auto* composer = full_text_search_dialog_
                         ->findChild<goldendict::app::FullTextQueryComposer*>(
                             QStringLiteral("fullTextQueryComposer"));
    full_text_search_dialog_->SetProjectedQuery(
        ComposeFullTextQuery(*composer));
    full_text_search_dialog_->show();
    full_text_search_dialog_->raise();
    full_text_search_dialog_->activateWindow();
}

#if defined(Q_OS_LINUX)
void MainWindow::ShowHelp(goldendict::app::HelpIntent intent) {
    bool created = false;
    if (help_window_ == nullptr) {
        const QString help_directory =
            help_directory_override_.isEmpty()
                ? goldendict::app::InstalledHelpDirectory()
                : help_directory_override_;
        auto* window = new goldendict::app::HelpWindow(
            help_directory, QString::fromStdString(preferences_.help_language),
            QString::fromStdString(preferences_.interface_language),
            QLocale::system().name(), this);
        if (!window->IsReady()) {
            window->deleteLater();
            return;
        }
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        help_window_ = window;
        created = true;
    }
    if ((created || intent != goldendict::app::HelpIntent::kReference) &&
        !help_window_->ShowIdentifier(goldendict::app::HelpIdentifier(intent)))
        return;
    help_window_->show();
    help_window_->raise();
    help_window_->activateWindow();
}
#endif

void MainWindow::SetFullTextDialogGeometry(std::string geometry) {
    full_text_dialog_geometry_ = std::move(geometry);
}

void MainWindow::NavigateToFullTextResult(
    goldendict::app::FullTextResultActivationIntent intent) {
    if (facade_ == nullptr || full_text_search_dialog_ == nullptr)
        return;
    InvalidateRenderedTextMatchPlan();
    rendered_page_text_transports_.erase(
        TabIdAt(article_tabs_->currentIndex()));

    goldendict::core::TabNavigationState navigation;
    navigation.kind = goldendict::core::TabNavigationKind::kLookup;
    navigation.query = intent.result.headword;
    navigation.group_id = selected_group_id_;
    navigation.title = intent.result.headword;
    navigation.dictionary_filter_active = intent.dictionary_filter_active;
    navigation.dictionary_ids = std::move(intent.dictionary_ids);
    navigation.exact_target = goldendict::core::ExactArticleTarget{
        intent.result.dictionary.id, intent.result.document_id};
    const QString main_query = query_->text();
    const int cursor_position = query_->cursorPosition();
    const int selection_start = query_->selectionStart();
    const int selection_length = query_->selectedText().size();
    const auto tab_result = facade_->OpenArticleTab(
        navigation, goldendict::core::TabOpenPolicy::kCurrentTab,
        goldendict::core::TabActivationPolicy::kActivate,
        NewTabPlacementPolicy());
    if (!tab_result) {
        status_->setText(QStringLiteral("Unable to update article state"));
        return;
    }
    SyncArticleTabs();
    query_->setText(main_query);
    query_->setCursorPosition(cursor_position);
    if (selection_start >= 0)
        query_->setSelection(selection_start, selection_length);
    emit ArticleTabSessionMutated();
    StartNavigationLookup(tab_result.tab_id, navigation, true);
    auto* target_view = ArticleViewForTab(tab_result.tab_id);
    if (target_view == nullptr)
        return;
    auto& search = article_search_presentations_[tab_result.tab_id];
    search.query =
        QString::fromUtf8(intent.query_text.data(),
                          static_cast<qsizetype>(intent.query_text.size()));
    search.status.clear();
    const std::uint64_t search_generation = ++search.generation;
    search.accepted_query_generation = intent.accepted_query_generation;
    search.mode = intent.mode;
    search.match_case = intent.match_case;
    search.ignore_word_order = intent.ignore_word_order;
    search.maximum_word_distance = intent.maximum_word_distance;
    search.ignore_diacritics = intent.ignore_diacritics;
    pending_article_search_handoffs_[tab_result.tab_id] = {
        search.query,
        lookup_results_[tab_result.tab_id].generation,
        search_generation,
        target_view,
        intent.mode,
        intent.match_case,
        intent.ignore_word_order,
        intent.maximum_word_distance,
        intent.ignore_diacritics,
        intent.accepted_query_generation};
    if (TabIdAt(article_tabs_->currentIndex()) == tab_result.tab_id)
        RefreshArticleSearch();
}

void MainWindow::ApplyDictionaryParticipation() {
    if (restoring_main_window_state_ || facade_ == nullptr ||
        article_tabs_ == nullptr)
        return;
    StartSuggestionLookup();
    const auto active_id = TabIdAt(article_tabs_->currentIndex());
    const auto tabs = facade_->GetArticleTabsState();
    const auto active = std::find_if(
        tabs.tabs.begin(), tabs.tabs.end(),
        [active_id](const auto& tab) { return tab.id == active_id; });
    if (active != tabs.tabs.end() &&
        active->navigation.kind !=
            goldendict::core::TabNavigationKind::kEmpty &&
        !active->navigation.query.empty()) {
        StartNavigationLookup(active_id, active->navigation, false);
    }
}

void MainWindow::EditDictionaryGroups() {
    std::vector<goldendict::core::DictionaryIdentity> catalog;
    if (facade_ != nullptr) {
        catalog = facade_->GetDictionaryService().GetCatalog();
    }
    GroupEditor editor(groups_, std::move(catalog), this);
    if (editor.exec() != QDialog::Accepted)
        return;
    groups_ = editor.Groups();
    emit DictionaryGroupsEdited();
}

void MainWindow::RunDictionaryGroupsSmokeCheck(
    std::function<void(std::size_t)> before_attempt,
    std::function<void(bool)> completion) {
    if (facade_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().empty()) {
        completion(false);
        return;
    }
    const auto catalog = facade_->GetDictionaryService().GetCatalog();
    const std::string first_id = catalog.front().id;
    const auto original_groups = groups_;
    bool edited = true;
    for (std::size_t attempt = 0U; attempt < 10U; ++attempt) {
        GroupEditor editor({}, catalog, this);
        edited = edited && editor.RunSmokeEdits();
        groups_ = editor.Groups();
        if (attempt == 9U && !groups_.empty())
            groups_.front().name = "Forward Published";
        if (before_attempt)
            before_attempt(attempt);
        QTimer::singleShot(10, this, []() {
            auto* warning =
                qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (warning != nullptr)
                warning->accept();
        });
        emit DictionaryGroupsEdited();
        if (attempt < 8U) {
            edited = edited && groups_ == original_groups &&
                     group_selector_->count() == 1;
        }
    }
    if (before_attempt)
        before_attempt(10U);
    const bool saved = edited && groups_.size() == 1U &&
                       groups_.front().id == 7U &&
                       groups_.front().name == "Forward Published" &&
                       groups_.front().dictionary_ids.size() == 2U &&
                       groups_.front().muted_dictionary_ids ==
                           std::vector<std::string>{first_id} &&
                       groups_.front().popup_muted_dictionary_ids ==
                           std::vector<std::string>{"unavailable-id"} &&
                       group_selector_->count() == 2;
    SetHistoryItems({{QStringLiteral("missing"), 999U}});
    emit history_list_->itemActivated(history_list_->item(0));
    const bool fallback = selected_group_id_ == 0U;
    SetHistoryItems({{QStringLiteral("application"), 7U}});
    emit history_list_->itemActivated(history_list_->item(0));
    const bool restored = selected_group_id_ == 7U;
    auto lookup_group = std::make_shared<std::uint32_t>(0U);
    connect(
        this, &MainWindow::LookupSubmitted, this,
        [lookup_group](const QString&, std::uint32_t group_id) {
            *lookup_group = group_id;
        },
        Qt::SingleShotConnection);
    query_->setText(QStringLiteral("application"));
    StartLookup();
    completion(saved && restored && fallback && *lookup_group == 7U);
}

void MainWindow::RunDictionaryBarSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr || dictionary_bar_ == nullptr ||
        facade_->GetDictionaryService().GetCatalog().size() < 2U) {
        completion(false);
        return;
    }
    show();
    QApplication::processEvents();
    const auto catalog = facade_->GetDictionaryService().GetCatalog();
    const auto toolbar_actions = dictionary_bar_host_->ActiveActions();
    bool identities =
        toolbar_actions.size() == static_cast<qsizetype>(catalog.size());
    for (qsizetype index = 0; identities && index < toolbar_actions.size();
         ++index) {
        const auto& dictionary = catalog[static_cast<std::size_t>(index)];
        const QString label = QString::fromStdString(
            dictionary.name.empty() ? dictionary.id : dictionary.name);
        auto* widget =
            dictionary_bar_host_->ActiveWidgetForAction(toolbar_actions[index]);
        identities = toolbar_actions[index]->isCheckable() &&
                     toolbar_actions[index]->isChecked() &&
                     toolbar_actions[index]->data().toString() ==
                         QString::fromStdString(dictionary.id) &&
                     toolbar_actions[index]->text() == label &&
                     toolbar_actions[index]->toolTip() == label &&
                     widget != nullptr && widget->accessibleName() == label;
    }
    const bool hierarchy =
        dictionary_bar_->objectName() == QStringLiteral("dictionaryBar") &&
        dictionary_bar_->toggleViewAction() != nullptr &&
        toolBarArea(dictionary_bar_) == Qt::TopToolBarArea &&
        dictionary_bar_->isVisible() && dictionary_bar_->isMovable() &&
        dictionary_bar_->isFloatable() &&
        dictionary_bar_->allowedAreas() == Qt::AllToolBarAreas;

    SetDictionaryGroups({{7U,
                          "Smoke Group",
                          "",
                          {catalog[1].id, catalog[0].id},
                          {catalog[0].id}}});
    SelectGroup(7U);
    RefreshDictionaryBar();
    const auto group_actions = dictionary_bar_host_->ActiveActions();
    const bool group_baseline = group_actions.size() == 2 &&
                                group_actions[0]->isChecked() &&
                                !group_actions[1]->isChecked() &&
                                group_actions[0]->data().toString() ==
                                    QString::fromStdString(catalog[1].id) &&
                                group_actions[1]->data().toString() ==
                                    QString::fromStdString(catalog[0].id);
    SelectGroup(0U);
    RefreshDictionaryBar();
    const auto all_actions = dictionary_bar_host_->ActiveActions();
    if (all_actions.empty()) {
        completion(false);
        return;
    }
    all_actions.front()->trigger();
    SelectGroup(7U);
    RefreshDictionaryBar();
    const bool group_isolation =
        dictionary_bar_host_->ActiveActions()[0]->isChecked() &&
        !dictionary_bar_host_->ActiveActions()[1]->isChecked();
    SelectGroup(0U);
    RefreshDictionaryBar();
    const bool all_scope_retained =
        !dictionary_bar_host_->ActiveActions()[0]->isChecked();
    for (auto* action : dictionary_bar_host_->ActiveActions()) {
        if (action->isChecked())
            action->trigger();
    }
    query_->setText(QStringLiteral("application"));
    StartLookup();
    auto poll = std::make_shared<std::function<void()>>();
    auto attempts = std::make_shared<int>(0);
    *poll = [this, identities, hierarchy, group_baseline, group_isolation,
             all_scope_retained, completion = std::move(completion), poll,
             attempts]() mutable {
        FinishLookup();
        if ((!requests_.empty() || ++*attempts < 5) && *attempts < 100) {
            QTimer::singleShot(10, this, *poll);
            return;
        }
        const bool all_off = requests_.empty() && results_list_->count() == 0 &&
                             suggestions_list_->count() == 0;
        dictionary_bar_->hide();
        StartLookup();
        auto hidden_poll = std::make_shared<std::function<void()>>();
        *hidden_poll = [this, identities, hierarchy, group_baseline,
                        group_isolation, all_scope_retained, all_off,
                        completion = std::move(completion),
                        hidden_poll]() mutable {
            FinishLookup();
            if (!requests_.empty()) {
                QTimer::singleShot(10, this, *hidden_poll);
                return;
            }
            const bool hidden_unfiltered = results_list_->count() > 0;
            dictionary_bar_->show();
            completion(identities && hierarchy && group_baseline &&
                       group_isolation && all_scope_retained && all_off &&
                       hidden_unfiltered);
        };
        QTimer::singleShot(10, this, *hidden_poll);
    };
    QTimer::singleShot(10, this, *poll);
}

void MainWindow::RunWidgetsFacadePreparationSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr || facade_candidate_reclaimer_ == nullptr) {
        completion(false);
        return;
    }
    const auto initial_session = facade_->ExportArticleTabSession();
    const QString initial_query = query_->text();
    const auto initial_group = selected_group_id_;
    const int initial_tabs = article_tabs_->count();
    const int initial_actions = dictionary_bar_host_->ActiveActions().size();
    const QByteArray initial_window_state = saveState(kMainWindowStateVersion);
    const auto initial_toolbars =
        findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
    QAction* const initial_dictionary_toggle =
        dictionary_bar_->toggleViewAction();
    QWidget* const initial_focus_successor =
        group_selector_->nextInFocusChain();
    bool passed = !facade_candidate_reclaimer_->isActive() &&
                  facade_preparation_record_ == nullptr;
    auto facade = std::shared_ptr<goldendict::core::DesktopFacade>(
        facade_, [](goldendict::core::DesktopFacade*) {});

    PreparedWidgetsFacadeCandidate wrong_thread;
    std::thread preparer([this, facade, &wrong_thread]() mutable {
        wrong_thread = PrepareFacadeCandidate(facade, preferences_, groups_);
    });
    preparer.join();
    passed = passed && !wrong_thread && facade_preparation_record_ == nullptr &&
             !facade_candidate_reclaimer_->isActive();

    for (int failure_step = 0; failure_step < 19; ++failure_step) {
        facade_preparation_failure_step_ = failure_step;
        auto injected = PrepareFacadeCandidate(facade, preferences_, groups_);
        passed = passed && !injected && facade_preparation_record_ == nullptr &&
                 !facade_candidate_reclaimer_->isActive() &&
                 group_selector_host_->InactivePage() == nullptr &&
                 article_tabs_host_->InactivePage() == nullptr &&
                 !dictionary_bar_host_->Prepared() &&
                 group_selector_host_->ActivePage() == group_selector_ &&
                 article_tabs_host_->ActivePage() == article_tabs_ &&
                 saveState(kMainWindowStateVersion) == initial_window_state &&
                 facade_binding_registry_->AuditClosedLeaseProtocol();
    }
    facade_preparation_failure_step_ = -1;

    auto candidate = PrepareFacadeCandidate(facade, preferences_, groups_);
    auto moved_candidate = std::move(candidate);
    passed = passed && !candidate && moved_candidate;
    candidate = std::move(moved_candidate);
    passed = passed && candidate && !moved_candidate;
    const auto record = candidate.record_;
    const auto* staged = record == nullptr ? nullptr : record->resources;
    const auto active_catalog = facade_->GetDictionaryService().GetCatalog();
    const bool catalog_matches =
        staged != nullptr && staged->catalog.size() == active_catalog.size() &&
        std::equal(staged->catalog.begin(), staged->catalog.end(),
                   active_catalog.begin(),
                   [](const auto& left, const auto& right) {
                       return left.id == right.id && left.name == right.name;
                   });
    const bool prepared_window_state_unchanged =
        saveState(kMainWindowStateVersion) == initial_window_state;
    const bool maintenance_switches_without_topology_change =
        group_selector_host_->AuditMaintenanceSwitch() &&
        article_tabs_host_->AuditMaintenanceSwitch() &&
        dictionary_bar_host_->AuditMaintenanceSwitch();
    passed =
        passed && candidate && IsFacadeCandidateCurrent(candidate) &&
        facade_candidate_reclaimer_->isActive() && staged != nullptr &&
        staged->facade.get() == facade_ && catalog_matches &&
        staged->tabs.tabs.size() ==
            facade_->GetArticleTabsState().tabs.size() &&
        staged->query_text == initial_query &&
        staged->selected_group_id == initial_group && staged->root != nullptr &&
        !staged->root->isVisible() && !staged->root->isEnabled() &&
        staged->group_selector != nullptr &&
        staged->dictionary_bar != nullptr && staged->article_tabs != nullptr &&
        group_selector_host_->ActivePage() == group_selector_ &&
        group_selector_host_->InactivePage() == staged->group_selector &&
        group_selector_host_->Prepared() &&
        group_selector_host_->parentWidget()->layout()->indexOf(
            group_selector_host_) == 0 &&
        article_tabs_host_->ActivePage() == article_tabs_ &&
        article_tabs_host_->InactivePage() == staged->article_tabs &&
        article_tabs_host_->Prepared() &&
        centralWidget()->layout()->indexOf(article_tabs_host_) >= 0 &&
        dictionary_bar_host_->ActivePage() != staged->dictionary_bar &&
        dictionary_bar_host_->Prepared() &&
        dictionary_bar_->actions().size() == 1 &&
        dictionary_bar_->widgetForAction(dictionary_bar_->actions().front()) ==
            dictionary_bar_host_ &&
        maintenance_switches_without_topology_change &&
        staged->full_text_dialog != nullptr &&
        !staged->full_text_dialog->isVisible() &&
        staged->suggestion_worker != nullptr &&
        staged->match_controller != nullptr && staged->relay != nullptr &&
        staged->relay->IsComplete() && !staged->relay->IsEnabled() &&
        staged->relay->CanPublishWithoutAllocation(this, record->generation,
                                                   record->epoch) &&
        article_tabs_->count() == initial_tabs &&
        dictionary_bar_host_->ActiveActions().size() == initial_actions &&
        prepared_window_state_unchanged &&
        findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly) ==
            initial_toolbars &&
        dictionary_bar_->toggleViewAction() == initial_dictionary_toggle &&
        group_selector_->nextInFocusChain() == initial_focus_successor &&
        staged->binding_slot.has_value() &&
        facade_binding_registry_->Ready(*staged->binding_slot) &&
        facade_binding_registry_->AuditClosedLeaseProtocol() &&
        goldendict::widgets::WidgetsFacadeBindingRegistry::
            AuditPublicationOperations() &&
        goldendict::widgets::WidgetsFacadeBindingRegistry::
            RunClosedLeaseProtocolSmokeCheck() &&
        facade_->ExportArticleTabSession() == initial_session;

    const auto epoch_before_inert_emissions = presentation_mutation_epoch_;
    const auto requests_before_inert_emissions = requests_.size();
    const auto suppressed_before = staged->relay->SuppressedDeliveries();
    auto* staged_action = staged->root->findChild<QAction*>(
        QStringLiteral("widgetsFacadeCandidateBack"));
    auto* staged_view =
        staged->article_tabs->count() == 0
            ? nullptr
            : qobject_cast<ArticleView*>(staged->article_tabs->widget(0));
    auto* staged_page = staged_view == nullptr
                            ? nullptr
                            : qobject_cast<ArticlePage*>(staged_view->page());
    if (staged_action != nullptr) {
        staged_action->setEnabled(true);
        staged_action->trigger();
        staged_action->setEnabled(false);
    }
    if (staged_page != nullptr) {
        emit staged_page->LookupRequested(QStringLiteral("inert"),
                                          QStringLiteral("goldendict://inert"),
                                          ArticleLinkDisposition::kCurrentTab);
    }
    if (staged_view != nullptr)
        emit staged_view->SelectionToInputRequested(QStringLiteral("inert"));
    emit staged->group_selector->currentIndexChanged(
        staged->group_selector->currentIndex());
    emit staged->article_tabs->currentChanged(
        staged->article_tabs->currentIndex());
    emit staged->article_tabs->tabCloseRequested(0);
    emit staged->article_tabs->tabBar()->tabMoved(0, 0);
    emit staged->article_tabs->tabBar()->customContextMenuRequested(QPoint{});
    staged->relay->SuggestionFinished(1U, 1U,
                                      goldendict::core::SuggestionResponse{});
    staged->relay->RenderedMatchFinished(
        1U, goldendict::core::RenderedTextMatchPlanResult{});
    emit staged->full_text_dialog->AcceptedQueryInvalidated();
    emit staged->full_text_dialog->GeometryCaptured("inert");
    passed = passed && staged_action != nullptr && staged_view != nullptr &&
             staged_page != nullptr &&
             staged->relay->SuppressedDeliveries() >= suppressed_before + 12U &&
             presentation_mutation_epoch_ == epoch_before_inert_emissions &&
             requests_.size() == requests_before_inert_emissions &&
             selected_group_id_ == initial_group &&
             article_tabs_->count() == initial_tabs &&
             query_->text() == initial_query &&
             facade_->ExportArticleTabSession() == initial_session &&
             IsFacadeCandidateCurrent(candidate);

    record->owner.store(nullptr, std::memory_order_release);
    passed = passed && !IsFacadeCandidateCurrent(candidate);
    record->owner.store(this, std::memory_order_release);
    passed = passed && IsFacadeCandidateCurrent(candidate);
    facade_preparation_shutdown_ = true;
    passed = passed && !IsFacadeCandidateCurrent(candidate);
    facade_preparation_shutdown_ = false;
    passed = passed && IsFacadeCandidateCurrent(candidate);

    query_->setText(initial_query + QStringLiteral("x"));
    passed = passed && !IsFacadeCandidateCurrent(candidate) &&
             query_->text() == initial_query + QStringLiteral("x") &&
             facade_->ExportArticleTabSession() == initial_session;
    candidate.Abandon();
    passed = passed && facade_preparation_record_ == nullptr &&
             !facade_candidate_reclaimer_->isActive();
    query_->setText(initial_query);

    auto invalid_groups = groups_;
    invalid_groups.push_back({999U, "Invalid", "", {}});
    invalid_groups.back().encoded_icon_data = "!";
    auto failed = PrepareFacadeCandidate(facade, preferences_, invalid_groups);
    passed = passed && !failed && facade_preparation_record_ == nullptr &&
             !facade_candidate_reclaimer_->isActive() &&
             facade_->ExportArticleTabSession() == initial_session;

    auto stale = PrepareFacadeCandidate(facade, preferences_, groups_);
    const bool stale_was_ready = static_cast<bool>(stale);
    auto superseding = PrepareFacadeCandidate(facade, preferences_, groups_);
    passed = passed && stale_was_ready && !stale && superseding &&
             !IsFacadeCandidateCurrent(stale) &&
             IsFacadeCandidateCurrent(superseding) &&
             facade_candidate_reclaimer_->isActive();
    stale.Abandon();
    passed = passed && IsFacadeCandidateCurrent(superseding);
    superseding.Abandon();
    passed = passed && facade_preparation_record_ == nullptr &&
             !facade_candidate_reclaimer_->isActive();

    auto abandoned = PrepareFacadeCandidate(facade, preferences_, groups_);
    const auto abandoned_record = abandoned.record_;
    passed = passed && abandoned && facade_candidate_reclaimer_->isActive();
    std::thread abandoner(
        [candidate = std::move(abandoned)]() mutable { candidate.Abandon(); });
    abandoner.join();
    passed = passed && facade_preparation_record_ != nullptr &&
             facade_candidate_reclaimer_->isActive();

    auto* deadline = new QTimer(this);
    deadline->setSingleShot(true);
    deadline->setInterval(250);
    connect(
        deadline, &QTimer::timeout, this,
        [this, passed, abandoned_record, deadline,
         completion = std::move(completion)]() mutable {
            const bool reclaimed =
                abandoned_record->reclaimed.load(std::memory_order_acquire);
            passed = passed && reclaimed &&
                     abandoned_record->reclaimed_on_owner_thread.load(
                         std::memory_order_acquire) &&
                     facade_preparation_record_ == nullptr &&
                     !facade_candidate_reclaimer_->isActive();

            auto current_facade =
                std::shared_ptr<goldendict::core::DesktopFacade>(
                    facade_, [](goldendict::core::DesktopFacade*) {});
            auto restarted =
                PrepareFacadeCandidate(current_facade, preferences_, groups_);
            passed =
                passed && restarted && facade_candidate_reclaimer_->isActive();
            const auto restarted_record = restarted.record_;
            restarted_record->owner.store(nullptr, std::memory_order_release);
            passed = passed && !IsFacadeCandidateCurrent(restarted);
            restarted_record->owner.store(this, std::memory_order_release);
            passed = passed && IsFacadeCandidateCurrent(restarted);
            restarted.Abandon();
            passed = passed && facade_preparation_record_ == nullptr &&
                     !facade_candidate_reclaimer_->isActive();

            // Phase A is rejectable at every injected boundary and must leave
            // the active presentation and interaction gate exactly restored.
            for (int failure_step = 0; failure_step < 8; ++failure_step) {
                auto rejected = PrepareFacadeCandidate(current_facade,
                                                       preferences_, groups_);
                facade_maintenance_failure_step_ = failure_step;
                auto maintenance = BeginFacadeCandidateMaintenance(rejected);
                passed =
                    passed && rejected && !maintenance.maintained &&
                    maintenance.outcome ==
                        WidgetsCommitOutcome::kRejectedBeforePublication &&
                    !WidgetsInteractionBlocked() &&
                    group_selector_host_->ActivePage() == group_selector_ &&
                    article_tabs_host_->ActivePage() == article_tabs_;
                rejected.Abandon();
            }
            facade_maintenance_failure_step_ = -1;

            auto abort_candidate =
                PrepareFacadeCandidate(current_facade, preferences_, groups_);
            auto abortable = BeginFacadeCandidateMaintenance(abort_candidate);
            passed = passed && !abort_candidate && abortable.maintained &&
                     WidgetsInteractionBlocked();
            AbortMaintainedFacadeCommit(std::move(abortable.maintained));
            passed = passed && !WidgetsInteractionBlocked() &&
                     facade_preparation_record_ == nullptr;

            goldendict::core::TabNavigationState restored_navigation;
            restored_navigation.kind =
                goldendict::core::TabNavigationKind::kLookup;
            restored_navigation.query = "application";
            restored_navigation.title = "application";
            const goldendict::core::ArticleTabSession restored_session = {
                {{91U, {restored_navigation}, 0U}}, 91U};
            passed =
                passed && facade_->RestoreArticleTabSession(restored_session);
            RebuildArticleTabs();
            const auto restoration_tab_id = TabIdAt(0);
            const auto finish_restoration_lookup = [this,
                                                    restoration_tab_id]() {
                QElapsedTimer timer;
                timer.start();
                while (requests_.count(restoration_tab_id) == 1U &&
                       !requests_.at(restoration_tab_id)->IsFinished() &&
                       timer.elapsed() < 2000) {
                    QApplication::processEvents(QEventLoop::AllEvents, 25);
                }
                if (requests_.count(restoration_tab_id) == 1U &&
                    requests_.at(restoration_tab_id)->IsFinished()) {
                    FinishLookup();
                    return true;
                }
                return requests_.count(restoration_tab_id) == 0U;
            };
            passed = passed && finish_restoration_lookup();
            auto* restoration_old_view = ArticleViewForTab(restoration_tab_id);
            passed = passed && restoration_old_view != nullptr;
            bool fixture_navigation_finished = false;
            bool fixture_navigation_succeeded = false;
            quint64 fixture_navigation_token = 0U;
            QMetaObject::Connection fixture_navigation_connection;
            if (restoration_old_view != nullptr) {
                QElapsedTimer load_timer;
                load_timer.start();
                while (restoration_old_view->page()->isLoading() &&
                       load_timer.elapsed() < 2000) {
                    QApplication::processEvents(QEventLoop::AllEvents, 25);
                }
                restoration_old_view->setZoomFactor(5.0);
                fixture_navigation_connection = connect(
                    restoration_old_view, &ArticleView::HtmlNavigationFinished,
                    this,
                    [&fixture_navigation_finished,
                     &fixture_navigation_succeeded, &fixture_navigation_token](
                        quint64 navigation_token, bool success) {
                        fixture_navigation_finished = true;
                        fixture_navigation_succeeded = success;
                        fixture_navigation_token = navigation_token;
                    });
                PublishArticleHtml(
                    restoration_tab_id, restoration_old_view,
                    QStringLiteral(
                        "<!doctype html><html><body style=\"margin:0\">"
                        "<div style=\"height:3000px\"></div>"
                        "</body></html>"),
                    QUrl(QStringLiteral("https://prepared-scroll.test/")));
            }
            QElapsedTimer fixture_navigation_timer;
            fixture_navigation_timer.start();
            while (!fixture_navigation_finished &&
                   fixture_navigation_timer.elapsed() < 2000) {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            QObject::disconnect(fixture_navigation_connection);
            passed = passed && fixture_navigation_finished &&
                     fixture_navigation_succeeded &&
                     fixture_navigation_token != 0U;
            bool scroll_ready = false;
            if (restoration_old_view != nullptr) {
                restoration_old_view->page()->runJavaScript(
                    QStringLiteral("window.scrollTo(0,75); window.scrollY;"),
                    [&scroll_ready](const QVariant&) { scroll_ready = true; });
            }
            QElapsedTimer scroll_timer;
            scroll_timer.start();
            while (
                restoration_old_view != nullptr &&
                (!scroll_ready ||
                 restoration_old_view->page()->scrollPosition().y() < 300.0) &&
                scroll_timer.elapsed() < 2000) {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            const QPointF saved_scroll =
                restoration_old_view == nullptr
                    ? QPointF{}
                    : restoration_old_view->page()->scrollPosition();
            passed = passed && scroll_ready && saved_scroll.y() >= 300.0;
            auto& restoration_search =
                article_search_presentations_[restoration_tab_id];
            restoration_search.query =
                QStringLiteral("absent-prepared-scroll-query");
            const std::uint64_t restoration_search_generation =
                restoration_search.generation;
            QWebEngineScript scroll_observer;
            scroll_observer.setName(
                QStringLiteral("prepared-scroll-restoration-smoke"));
            scroll_observer.setInjectionPoint(QWebEngineScript::DocumentReady);
            scroll_observer.setWorldId(QWebEngineScript::MainWorld);
            scroll_observer.setRunsOnSubFrames(false);
            scroll_observer.setSourceCode(QStringLiteral(
                "globalThis.__goldendictPreparedRestoredScroll = null;"
                "const preparedOriginalScrollTo = window.scrollTo.bind(window);"
                "window.scrollTo = (x, y) => {"
                "globalThis.__goldendictPreparedRestoredScroll = [x, y];"
                "return preparedOriginalScrollTo(x, y);};"));
            QWebEngineProfile::defaultProfile()->scripts()->insert(
                scroll_observer);

            auto publish_candidate =
                PrepareFacadeCandidate(current_facade, preferences_, groups_);
            auto maintained =
                BeginFacadeCandidateMaintenance(publish_candidate);
            passed =
                passed && maintained.maintained && WidgetsInteractionBlocked();
            auto held_old_binding = facade_binding_registry_->Acquire();
            auto published =
                PublishMaintainedFacadeCommit(std::move(maintained.maintained));
            passed = passed && published && !WidgetsInteractionBlocked() &&
                     held_old_binding;
            auto* restoration_view = ArticleViewForTab(restoration_tab_id);
            if (restoration_view != nullptr)
                restoration_view->setZoomFactor(5.0);
            const auto first_outcome =
                FinishPublishedFacadeCommit(std::move(published));
            passed = passed &&
                     first_outcome == WidgetsCommitOutcome::kPublished &&
                     facade_binding_registry_->NeedsReclaim();
            passed = passed && finish_restoration_lookup();
            const auto pending_restoration =
                pending_article_scroll_restorations_.find(restoration_tab_id);
            const quint64 matching_navigation_token =
                pending_restoration ==
                        pending_article_scroll_restorations_.end()
                    ? 0U
                    : pending_restoration->second.navigation_token;
            QElapsedTimer restoration_timer;
            restoration_timer.start();
            while ((pending_article_scroll_restorations_.count(
                        restoration_tab_id) == 1U ||
                    (restoration_view != nullptr &&
                     restoration_view->page()->scrollPosition().y() < 300.0)) &&
                   restoration_timer.elapsed() < 2000) {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            QPointF requested_scroll;
            QPointF actual_javascript_scroll;
            bool observed_scroll = false;
            if (restoration_view != nullptr) {
                restoration_view->page()->runJavaScript(
                    QStringLiteral(
                        "[globalThis.__goldendictPreparedRestoredScroll,"
                        "window.scrollX,window.scrollY]"),
                    [&requested_scroll, &actual_javascript_scroll,
                     &observed_scroll](const QVariant& value) {
                        const QVariantList result = value.toList();
                        const QVariantList coordinates =
                            result.value(0).toList();
                        if (coordinates.size() == 2) {
                            requested_scroll =
                                QPointF(coordinates[0].toDouble(),
                                        coordinates[1].toDouble());
                        }
                        if (result.size() == 3) {
                            actual_javascript_scroll = QPointF(
                                result[1].toDouble(), result[2].toDouble());
                        }
                        observed_scroll = true;
                    });
            }
            QElapsedTimer observer_timer;
            observer_timer.start();
            while (!observed_scroll && observer_timer.elapsed() < 2000)
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            QWebEngineProfile::defaultProfile()->scripts()->remove(
                scroll_observer);
            const QPointF expected_css_scroll(saved_scroll.x() / 5.0,
                                              saved_scroll.y() / 5.0);
            passed =
                passed && matching_navigation_token != 0U &&
                pending_article_scroll_restorations_.count(
                    restoration_tab_id) == 0U &&
                restoration_search.generation ==
                    restoration_search_generation + 1U &&
                restoration_view != nullptr && observed_scroll &&
                qAbs(restoration_view->page()->scrollPosition().x() -
                     saved_scroll.x()) < 0.5 &&
                qAbs(restoration_view->page()->scrollPosition().y() -
                     saved_scroll.y()) < 0.5 &&
                qAbs(requested_scroll.x() - expected_css_scroll.x()) < 0.5 &&
                qAbs(requested_scroll.y() - expected_css_scroll.y()) < 0.5 &&
                qAbs(actual_javascript_scroll.x() - expected_css_scroll.x()) <
                    0.5 &&
                qAbs(actual_javascript_scroll.y() - expected_css_scroll.y()) <
                    0.5;
            if (restoration_view != nullptr) {
                emit restoration_view->HtmlNavigationFinished(
                    matching_navigation_token, true);
            }
            passed = passed && restoration_search.generation ==
                                   restoration_search_generation + 1U;
            held_old_binding = {};
            facade_binding_registry_->ReclaimRetired();
            passed = passed && !facade_binding_registry_->NeedsReclaim();

            // A second immediate generation proves slot reuse, forward-only
            // destructor cleanup, and deterministic cleanup diagnostics.
            current_facade = std::shared_ptr<goldendict::core::DesktopFacade>(
                facade_, [](goldendict::core::DesktopFacade*) {});
            auto second_candidate =
                PrepareFacadeCandidate(current_facade, preferences_, groups_);
            auto second_maintained =
                BeginFacadeCandidateMaintenance(second_candidate);
            auto second_published = PublishMaintainedFacadeCommit(
                std::move(second_maintained.maintained));
            widgets_cleanup_failure_injected_ = true;
            const auto second_outcome =
                FinishPublishedFacadeCommit(std::move(second_published));
            widgets_cleanup_failure_injected_ = false;
            passed = passed &&
                     second_outcome ==
                         WidgetsCommitOutcome::kPublishedWithCleanupFailure &&
                     facade_ == current_facade.get() &&
                     !WidgetsInteractionBlocked();

            auto* const compatibility_facade = facade_;
            SetFacade(compatibility_facade);
            SetFacade(compatibility_facade);
            facade_binding_registry_->ReclaimRetired();
            const auto binding = facade_binding_registry_->Acquire();
            passed = passed && binding &&
                     binding->facade == compatibility_facade &&
                     !facade_binding_registry_->NeedsReclaim() &&
                     facade_binding_registry_->AuditClosedLeaseProtocol();
            deadline->deleteLater();
            completion(passed);
        });
    deadline->start();
}

void MainWindow::RunFullTextDictionaryProjectionSmokeCheck(
    std::function<void(bool)> completion) {
    if (facade_ == nullptr || dictionary_bar_ == nullptr) {
        completion(false);
        return;
    }
    show();
    QApplication::processEvents();
    const auto catalog = facade_->GetDictionaryService().GetCatalog();
    std::vector<std::string> supported;
    for (const auto& identity : catalog) {
        if (identity.supports_full_text_search) {
            supported.push_back(identity.id);
        }
    }
    if (supported.empty()) {
        completion(false);
        return;
    }

    goldendict::app::FullTextQueryComposer composer(preferences_, this);
    const auto request_count = requests_.size();
    SelectGroup(0U);
    RefreshDictionaryBar();
    dictionary_bar_->show();
    QApplication::processEvents();
    const auto all = ComposeFullTextQuery(composer);

    QAction* supported_action = nullptr;
    for (auto* action : dictionary_bar_host_->ActiveActions()) {
        if (action->data().toString().toStdString() == supported.front()) {
            supported_action = action;
            break;
        }
    }
    if (supported_action == nullptr) {
        completion(false);
        return;
    }
    supported_action->trigger();
    const auto unchecked = ComposeFullTextQuery(composer);
    dictionary_bar_->hide();
    QApplication::processEvents();
    const auto hidden = ComposeFullTextQuery(composer);

    SetDictionaryGroups({{7U,
                          "Full Text Projection Smoke",
                          "",
                          {supported.front(), "unresolved.dictionary"},
                          {supported.front()}}});
    SelectGroup(7U);
    const auto muted = ComposeFullTextQuery(composer);
    QApplication::processEvents();
    completion(all.dictionary_filter_active &&
               all.dictionary_ids == supported &&
               unchecked.dictionary_filter_active &&
               std::find(unchecked.dictionary_ids.begin(),
                         unchecked.dictionary_ids.end(),
                         supported.front()) == unchecked.dictionary_ids.end() &&
               hidden.dictionary_ids == supported &&
               muted.dictionary_filter_active && muted.dictionary_ids.empty() &&
               requests_.size() == request_count && !composer.isVisible());
}

void MainWindow::RunFullTextDialogSmokeCheck(
    std::function<void(bool)> completion) {
    auto* search_menu = findChild<QMenu*>(QStringLiteral("menuSearch"));
    if (facade_ == nullptr || search_menu == nullptr ||
        full_text_search_action_ == nullptr || dictionary_bar_ == nullptr) {
        completion(false);
        return;
    }
    show();
    QApplication::processEvents();
    const auto catalog = facade_->GetDictionaryService().GetCatalog();
    std::vector<std::string> supported;
    for (const auto& identity : catalog) {
        if (identity.supports_full_text_search)
            supported.push_back(identity.id);
    }
    if (supported.empty()) {
        completion(false);
        return;
    }

    class CapturingDesktopFacade final
        : public goldendict::core::DesktopFacade {
       public:
        explicit CapturingDesktopFacade(goldendict::core::DesktopFacade* inner)
            : inner_(inner) {}

        goldendict::core::DictionaryService& GetDictionaryService() noexcept
            override {
            return inner_->GetDictionaryService();
        }

        const goldendict::core::DictionaryService& GetDictionaryService()
            const noexcept override {
            return inner_->GetDictionaryService();
        }

        std::unique_ptr<goldendict::core::HeadwordExportOperation>
        StartHeadwordExport(
            goldendict::core::HeadwordExportRequest request) const override {
            return inner_->StartHeadwordExport(std::move(request));
        }

        goldendict::core::ArticleContent ComposeLookupPage(
            const goldendict::core::LookupResponse& response) const override {
            return inner_->ComposeLookupPage(response);
        }

        std::optional<goldendict::core::ArticleUrl> ResolveArticleUrl(
            const std::string& url) const override {
            return inner_->ResolveArticleUrl(url);
        }

        goldendict::core::ResolvedExactArticleTarget ResolveExactArticleTarget(
            const goldendict::core::ExactArticleTarget& target) const override {
            return inner_->ResolveExactArticleTarget(target);
        }

        goldendict::core::RenderedTextMatchPlanResult
        BuildRenderedTextMatchPlan(
            const goldendict::core::RenderedTextMatchPlanRequest& request,
            const goldendict::core::CancellationToken* cancellation)
            const override {
            return inner_->BuildRenderedTextMatchPlan(request, cancellation);
        }

        goldendict::core::ArticleTabsState GetArticleTabsState()
            const override {
            return inner_->GetArticleTabsState();
        }

        goldendict::core::ArticleTabSession ExportArticleTabSession()
            const override {
            return inner_->ExportArticleTabSession();
        }

        goldendict::core::TabOperationResult RestoreArticleTabSession(
            const goldendict::core::ArticleTabSession& session) override {
            return inner_->RestoreArticleTabSession(session);
        }

        goldendict::core::TabOperationResult OpenArticleTab(
            const goldendict::core::TabNavigationState& navigation,
            goldendict::core::TabOpenPolicy open_policy,
            goldendict::core::TabActivationPolicy activation_policy,
            goldendict::core::TabPlacementPolicy placement_policy) override {
            last_navigation = navigation;
            ++open_count;
            if (forced_error.has_value())
                return {*forced_error, 0U};
            return inner_->OpenArticleTab(navigation, open_policy,
                                          activation_policy, placement_policy);
        }

        goldendict::core::TabOperationResult ActivateArticleTab(
            goldendict::core::ArticleTabId tab_id) override {
            return inner_->ActivateArticleTab(tab_id);
        }

        goldendict::core::TabOperationResult CloseArticleTab(
            goldendict::core::ArticleTabId tab_id) override {
            return inner_->CloseArticleTab(tab_id);
        }

        goldendict::core::TabOperationResult CloseOtherArticleTabs(
            goldendict::core::ArticleTabId tab_id) override {
            return inner_->CloseOtherArticleTabs(tab_id);
        }

        goldendict::core::TabOperationResult GoBackInArticleTab(
            goldendict::core::ArticleTabId tab_id) override {
            return inner_->GoBackInArticleTab(tab_id);
        }

        goldendict::core::TabOperationResult GoForwardInArticleTab(
            goldendict::core::ArticleTabId tab_id) override {
            return inner_->GoForwardInArticleTab(tab_id);
        }

        goldendict::core::DesktopFacade* inner_ = nullptr;
        std::optional<goldendict::core::TabOperationError> forced_error;
        goldendict::core::TabNavigationState last_navigation;
        std::size_t open_count = 0U;
    };

    CapturingDesktopFacade capturing_facade(facade_);

    const auto actions = search_menu->actions();
    const auto preferences_before = preferences_;
    std::vector<std::string> captured_geometries;
    const auto geometry_connection =
        connect(this, &MainWindow::FullTextDialogGeometryCaptured, this,
                [&captured_geometries](std::string geometry) {
                    captured_geometries.push_back(std::move(geometry));
                });
    bool passed =
        actions.size() == 2 && actions[0] == search_in_page_action_ &&
        actions[1] == full_text_search_action_ &&
        full_text_search_action_->text() ==
            QStringLiteral("Full-text search") &&
        full_text_search_action_->shortcut() ==
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F) &&
        full_text_search_action_->shortcutContext() ==
            Qt::WidgetWithChildrenShortcut &&
        full_text_search_action_->menuRole() == QAction::TextHeuristicRole &&
        full_text_search_action_->isEnabled();

    SelectGroup(0U);
    dictionary_bar_->hide();
    query_->setText(QStringLiteral("first shell query"));
    full_text_search_action_->trigger();
    QApplication::processEvents();
    auto* first = full_text_search_dialog_.data();
    auto* query =
        first == nullptr
            ? nullptr
            : first->findChild<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    passed = passed && first != nullptr && first->isVisible() &&
             !first->isModal() &&
             first->windowTitle() == QStringLiteral("Full-text search") &&
             !(first->windowFlags() & Qt::WindowContextHelpButtonHint) &&
             query != nullptr &&
             query->text() == QStringLiteral("first shell query") &&
             query->selectedText() == QStringLiteral("first shell query") &&
             first->ProjectedQuery().dictionary_ids == supported;

    full_text_search_action_->trigger();
    QApplication::processEvents();
    passed = passed && full_text_search_dialog_ == first && first->isVisible();

    SetDictionaryGroups({{7U,
                          "Full Text Dialog Smoke",
                          "",
                          {supported.front()},
                          {supported.front()}}});
    SelectGroup(7U);
    full_text_search_action_->trigger();
    QApplication::processEvents();
    passed = passed && first->ProjectedQuery().dictionary_filter_active &&
             first->ProjectedQuery().dictionary_ids.empty();

    goldendict::core::ArticleTabSession activation_session;
    activation_session.active_tab_id = TabIdAt(article_tabs_->currentIndex());
    activation_session.tabs.push_back({activation_session.active_tab_id,
                                       {goldendict::core::TabNavigationState{}},
                                       0U});
    passed = passed && facade_->RestoreArticleTabSession(activation_session);
    SyncArticleTabs();
    SelectGroup(7U);
    query_->setText(QStringLiteral("first shell query"));

    QStringList activation_events;
    const auto session_connection =
        connect(this, &MainWindow::ArticleTabSessionMutated, this,
                [&activation_events]() {
                    activation_events.push_back(QStringLiteral("session"));
                });
    const auto lookup_connection =
        connect(this, &MainWindow::LookupSubmitted, this,
                [&activation_events](const QString&, std::uint32_t) {
                    activation_events.push_back(QStringLiteral("lookup"));
                });
    const int tab_count_before_activation = article_tabs_->count();
    const auto tab_id_before_activation =
        TabIdAt(article_tabs_->currentIndex());
    query_->setSelection(2, 5);
    const QString main_query_before_activation = query_->text();
    const int main_query_cursor_before_activation = query_->cursorPosition();
    const int main_query_selection_before_activation = query_->selectionStart();
    const QString main_query_selected_text_before_activation =
        query_->selectedText();
    auto& prior_article_search =
        article_search_presentations_[tab_id_before_activation];
    prior_article_search.query = QStringLiteral("prior article query");
    prior_article_search.status = QStringLiteral("prior article status");
    ++prior_article_search.generation;
    RefreshArticleSearch();
    std::optional<goldendict::core::FullTextResult> exact_result;
    for (const auto& text : {"Fixture", "fruit", "program", "A"}) {
        goldendict::core::FullTextQuery exact_query;
        exact_query.text = text;
        exact_query.mode = goldendict::core::FullTextQueryMode::kPlainText;
        const auto exact_response =
            facade_->GetDictionaryService().SearchFullText(exact_query);
        if (!exact_response.results.empty()) {
            exact_result = exact_response.results.front();
            break;
        }
    }
    passed = passed && exact_result.has_value();
    goldendict::app::FullTextResultActivationIntent ordered_intent;
    if (exact_result.has_value())
        ordered_intent.result = *exact_result;
    ordered_intent.result.excerpt = "must-not-highlight-excerpt";
    ordered_intent.result.matches = {{1U, 2U, "must-not-highlight-match"}};
    ordered_intent.dictionary_filter_active = true;
    ordered_intent.dictionary_ids = {ordered_intent.result.dictionary.id,
                                     ordered_intent.result.dictionary.id};
    ordered_intent.query_text = u8"exact accepted 😀 query";
    ordered_intent.ignore_diacritics = true;
    ordered_intent.mode =
        goldendict::core::FullTextQueryMode::kRegularExpression;
    ordered_intent.match_case = true;
    ordered_intent.ignore_word_order = true;
    ordered_intent.maximum_word_distance = 23U;
    ordered_intent.accepted_query_generation = 41U;
    facade_ = &capturing_facade;
    first->ResultActivationRequested(ordered_intent);
    const auto ordered_state = facade_->GetArticleTabsState();
    const auto ordered_session = facade_->ExportArticleTabSession();
    passed =
        passed &&
        activation_events ==
            QStringList{QStringLiteral("session"), QStringLiteral("lookup")} &&
        article_tabs_->count() == tab_count_before_activation &&
        ordered_state.active_tab_id == tab_id_before_activation &&
        !ordered_state.tabs.empty() &&
        ordered_state.tabs.front().navigation.kind ==
            goldendict::core::TabNavigationKind::kLookup &&
        ordered_state.tabs.front().navigation.query ==
            ordered_intent.result.headword &&
        ordered_state.tabs.front().navigation.title ==
            ordered_intent.result.headword &&
        ordered_state.tabs.front().navigation.group_id == 7U &&
        ordered_state.tabs.front().navigation.dictionary_filter_active &&
        ordered_state.tabs.front().navigation.dictionary_ids ==
            ordered_intent.dictionary_ids &&
        ordered_state.tabs.front().navigation.exact_target ==
            std::optional<goldendict::core::ExactArticleTarget>(
                goldendict::core::ExactArticleTarget{
                    ordered_intent.result.dictionary.id,
                    ordered_intent.result.document_id}) &&
        capturing_facade.last_navigation ==
            ordered_state.tabs.front().navigation &&
        ordered_state.tabs.front().navigation.source_dictionary_id.empty() &&
        ordered_state.tabs.front().navigation.source_article_id.empty() &&
        ordered_state.tabs.front().navigation.target_article_id.empty() &&
        ordered_state.tabs.front().navigation.target_anchor.empty() &&
        query_->text() == main_query_before_activation &&
        query_->cursorPosition() == main_query_cursor_before_activation &&
        query_->selectionStart() == main_query_selection_before_activation &&
        query_->selectedText() == main_query_selected_text_before_activation &&
        article_search_presentations_[tab_id_before_activation].query ==
            QString::fromStdString(ordered_intent.query_text) &&
        article_search_presentations_[tab_id_before_activation]
            .status.isEmpty() &&
        article_search_->text() ==
            QString::fromStdString(ordered_intent.query_text) &&
        article_search_status_->text().isEmpty() &&
        pending_article_search_handoffs_.count(tab_id_before_activation) ==
            1U &&
        pending_article_search_handoffs_[tab_id_before_activation].query ==
            QString::fromStdString(ordered_intent.query_text) &&
        pending_article_search_handoffs_[tab_id_before_activation]
                .lookup_generation ==
            lookup_results_[tab_id_before_activation].generation &&
        pending_article_search_handoffs_[tab_id_before_activation].view ==
            ArticleViewForTab(tab_id_before_activation) &&
        pending_article_search_handoffs_[tab_id_before_activation].mode ==
            ordered_intent.mode &&
        pending_article_search_handoffs_[tab_id_before_activation].match_case ==
            ordered_intent.match_case &&
        pending_article_search_handoffs_[tab_id_before_activation]
                .ignore_word_order == ordered_intent.ignore_word_order &&
        pending_article_search_handoffs_[tab_id_before_activation]
                .maximum_word_distance ==
            ordered_intent.maximum_word_distance &&
        pending_article_search_handoffs_[tab_id_before_activation]
                .ignore_diacritics == ordered_intent.ignore_diacritics &&
        pending_article_search_handoffs_[tab_id_before_activation]
                .accepted_query_generation ==
            ordered_intent.accepted_query_generation &&
        ordered_session.tabs.size() == 1U &&
        ordered_session.tabs.front().history.back() ==
            ordered_state.tabs.front().navigation &&
        requests_.count(tab_id_before_activation) == 1U;

    const auto wait_for = [](const std::function<bool()>& predicate) {
        QElapsedTimer timer;
        timer.start();
        while (!predicate() && timer.elapsed() < 5000) {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        return predicate();
    };
    const bool extracted = wait_for([this, tab_id_before_activation]() {
        return rendered_page_text_transports_.count(tab_id_before_activation) ==
               1U;
    });
    QString independently_extracted_text;
    bool independent_extraction_finished = false;
    if (auto* extraction_view = ArticleViewForTab(tab_id_before_activation)) {
        extraction_view->page()->toPlainText(
            [&independently_extracted_text,
             &independent_extraction_finished](const QString& text) {
                independently_extracted_text = text;
                independent_extraction_finished = true;
            });
    }
    passed =
        passed && extracted && wait_for([&independent_extraction_finished]() {
            return independent_extraction_finished;
        });
    if (extracted && independent_extraction_finished) {
        const auto& transport =
            rendered_page_text_transports_.at(tab_id_before_activation);
        passed =
            passed && transport.text == independently_extracted_text &&
            transport.accepted_query_generation ==
                ordered_intent.accepted_query_generation &&
            transport.lookup_generation ==
                lookup_results_[tab_id_before_activation].generation &&
            transport.search_generation ==
                article_search_presentations_[tab_id_before_activation]
                    .generation &&
            transport.view == ArticleViewForTab(tab_id_before_activation) &&
            transport.page == transport.view->page();
    }
    const bool planned = wait_for([this, tab_id_before_activation]() {
        return rendered_text_match_plans_.count(tab_id_before_activation) == 1U;
    });
    const QString status_before_plan_checks = status_->text();
    const QString article_status_before_plan_checks =
        article_search_status_->text();
    const QString article_query_before_plan_checks = article_search_->text();
    passed =
        passed && planned &&
        pending_rendered_text_match_plan_ == std::nullopt &&
        rendered_text_match_plans_.at(tab_id_before_activation)
            .identity.request.ignore_diacritics &&
        status_->text() == status_before_plan_checks &&
        article_search_status_->text() == article_status_before_plan_checks &&
        article_search_->text() == article_query_before_plan_checks;
    if (planned) {
        const auto accepted_state =
            rendered_text_match_plans_.at(tab_id_before_activation);
        const auto reject_match_plan =
            [this, tab_id_before_activation, &accepted_state](
                const std::function<void()>& invalidate,
                const std::function<void()>& restore) {
                rendered_text_match_plans_.erase(tab_id_before_activation);
                pending_rendered_text_match_plan_ = accepted_state.identity;
                const auto generation =
                    pending_rendered_text_match_plan_->work_generation;
                invalidate();
                FinishRenderedTextMatchPlan(generation, {});
                const bool rejected = rendered_text_match_plans_.count(
                                          tab_id_before_activation) == 0U;
                restore();
                return rejected;
            };
        passed = passed &&
                 reject_match_plan(
                     [this]() {
                         ++pending_rendered_text_match_plan_->work_generation;
                     },
                     []() {});
        passed =
            passed &&
            reject_match_plan(
                [this, tab_id_before_activation]() {
                    ++article_search_presentations_[tab_id_before_activation]
                          .accepted_query_generation;
                },
                [this, tab_id_before_activation, &accepted_state]() {
                    article_search_presentations_[tab_id_before_activation]
                        .accepted_query_generation =
                        accepted_state.identity.accepted_query_generation;
                });
        const auto saved_query =
            article_search_presentations_[tab_id_before_activation].query;
        passed = passed &&
                 reject_match_plan(
                     [this, tab_id_before_activation]() {
                         article_search_presentations_[tab_id_before_activation]
                             .query.append(QStringLiteral("x"));
                     },
                     [this, tab_id_before_activation, saved_query]() {
                         article_search_presentations_[tab_id_before_activation]
                             .query = saved_query;
                     });
        const auto saved_mode =
            article_search_presentations_[tab_id_before_activation].mode;
        passed = passed &&
                 reject_match_plan(
                     [this, tab_id_before_activation]() {
                         article_search_presentations_[tab_id_before_activation]
                             .mode =
                             goldendict::core::FullTextQueryMode::kPlainText;
                     },
                     [this, tab_id_before_activation, saved_mode]() {
                         article_search_presentations_[tab_id_before_activation]
                             .mode = saved_mode;
                     });
        passed =
            passed &&
            reject_match_plan(
                [this, tab_id_before_activation]() {
                    auto& value =
                        article_search_presentations_[tab_id_before_activation]
                            .match_case;
                    value = !value;
                },
                [this, tab_id_before_activation]() {
                    auto& value =
                        article_search_presentations_[tab_id_before_activation]
                            .match_case;
                    value = !value;
                });
        passed =
            passed &&
            reject_match_plan(
                [this, tab_id_before_activation]() {
                    auto& value =
                        article_search_presentations_[tab_id_before_activation]
                            .ignore_word_order;
                    value = !value;
                },
                [this, tab_id_before_activation]() {
                    auto& value =
                        article_search_presentations_[tab_id_before_activation]
                            .ignore_word_order;
                    value = !value;
                });
        const auto saved_distance =
            article_search_presentations_[tab_id_before_activation]
                .maximum_word_distance;
        passed = passed &&
                 reject_match_plan(
                     [this, tab_id_before_activation]() {
                         article_search_presentations_[tab_id_before_activation]
                             .maximum_word_distance = 24U;
                     },
                     [this, tab_id_before_activation, saved_distance]() {
                         article_search_presentations_[tab_id_before_activation]
                             .maximum_word_distance = saved_distance;
                     });
        const bool saved_ignore_diacritics =
            article_search_presentations_[tab_id_before_activation]
                .ignore_diacritics;
        passed =
            passed &&
            reject_match_plan(
                [this, tab_id_before_activation, &accepted_state]() {
                    article_search_presentations_[tab_id_before_activation]
                        .ignore_diacritics =
                        !accepted_state.identity.request.ignore_diacritics;
                },
                [this, tab_id_before_activation, saved_ignore_diacritics]() {
                    article_search_presentations_[tab_id_before_activation]
                        .ignore_diacritics = saved_ignore_diacritics;
                });
        passed = passed &&
                 reject_match_plan(
                     [this, tab_id_before_activation]() {
                         ++lookup_results_[tab_id_before_activation].generation;
                     },
                     [this, tab_id_before_activation, &accepted_state]() {
                         lookup_results_[tab_id_before_activation].generation =
                             accepted_state.identity.lookup_generation;
                     });
        passed =
            passed &&
            reject_match_plan(
                [this, tab_id_before_activation]() {
                    ++article_search_presentations_[tab_id_before_activation]
                          .generation;
                },
                [this, tab_id_before_activation, &accepted_state]() {
                    article_search_presentations_[tab_id_before_activation]
                        .generation = accepted_state.identity.search_generation;
                });
        passed =
            passed &&
            reject_match_plan(
                [this, tab_id_before_activation]() {
                    ++article_navigation_generations_[tab_id_before_activation];
                },
                [this, tab_id_before_activation, &accepted_state]() {
                    article_navigation_generations_[tab_id_before_activation] =
                        accepted_state.identity.navigation_generation;
                });
        passed =
            passed &&
            reject_match_plan(
                [this]() { pending_rendered_text_match_plan_->tab_id = 0U; },
                []() {});
        passed =
            passed &&
            reject_match_plan(
                [this]() { pending_rendered_text_match_plan_->view = nullptr; },
                []() {});
        passed =
            passed &&
            reject_match_plan(
                [this]() { pending_rendered_text_match_plan_->page = nullptr; },
                []() {});
        rendered_text_match_plans_[tab_id_before_activation] = accepted_state;
        FinishRenderedTextMatchPlan(accepted_state.identity.work_generation,
                                    {});
        passed = passed &&
                 rendered_text_match_plans_.at(tab_id_before_activation)
                         .result.error == accepted_state.result.error &&
                 status_->text() == status_before_plan_checks &&
                 article_search_status_->text() ==
                     article_status_before_plan_checks &&
                 article_search_->text() == article_query_before_plan_checks;
    }
    const auto reject_stale_extraction =
        [this,
         tab_id_before_activation](const std::function<void()>& invalidate) {
            auto* stale_view = ArticleViewForTab(tab_id_before_activation);
            if (stale_view == nullptr)
                return false;
            auto& stale_search =
                article_search_presentations_[tab_id_before_activation];
            const auto stale_lookup_generation =
                lookup_results_[tab_id_before_activation].generation;
            const auto stale_navigation_generation =
                article_navigation_generations_[tab_id_before_activation];
            rendered_page_text_transports_.erase(tab_id_before_activation);
            ExtractRenderedPageText(
                tab_id_before_activation, stale_view,
                stale_search.accepted_query_generation, stale_lookup_generation,
                stale_search.generation, stale_navigation_generation);
            invalidate();
            QElapsedTimer callback_timer;
            callback_timer.start();
            while (callback_timer.elapsed() < 100)
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            return rendered_page_text_transports_.count(
                       tab_id_before_activation) == 0U;
        };
    const auto saved_lookup_generation =
        lookup_results_[tab_id_before_activation].generation;
    const auto saved_search_generation =
        article_search_presentations_[tab_id_before_activation].generation;
    const auto saved_accepted_query_generation =
        article_search_presentations_[tab_id_before_activation]
            .accepted_query_generation;
    const auto saved_navigation_generation =
        article_navigation_generations_[tab_id_before_activation];
    passed =
        passed && reject_stale_extraction([this, tab_id_before_activation]() {
            ++lookup_results_[tab_id_before_activation].generation;
        });
    passed =
        passed && reject_stale_extraction([this, tab_id_before_activation]() {
            ++article_search_presentations_[tab_id_before_activation]
                  .generation;
        });
    passed =
        passed && reject_stale_extraction([this, tab_id_before_activation]() {
            ++article_search_presentations_[tab_id_before_activation]
                  .accepted_query_generation;
        });
    passed =
        passed && reject_stale_extraction([this, tab_id_before_activation]() {
            ++article_navigation_generations_[tab_id_before_activation];
        });
    passed = passed && reject_stale_extraction(
                           [first]() { first->AcceptedQueryInvalidated(); });
    lookup_results_[tab_id_before_activation].generation =
        saved_lookup_generation;
    article_search_presentations_[tab_id_before_activation].generation =
        saved_search_generation;
    article_search_presentations_[tab_id_before_activation]
        .accepted_query_generation = saved_accepted_query_generation;
    article_navigation_generations_[tab_id_before_activation] =
        saved_navigation_generation;
    if (auto request = requests_.find(tab_id_before_activation);
        request != requests_.end()) {
        request->second->Cancel();
        requests_.erase(request);
    }

    activation_events.clear();
    auto second_intent = ordered_intent;
    second_intent.result.headword = "second-exact-headword";
    second_intent.ignore_diacritics = false;
    first->ResultActivationRequested(second_intent);
    const auto second_state = facade_->GetArticleTabsState();
    passed = passed &&
             activation_events == QStringList{QStringLiteral("session"),
                                              QStringLiteral("lookup")} &&
             second_state.tabs.size() == 1U &&
             second_state.tabs.front().navigation.exact_target ==
                 ordered_state.tabs.front().navigation.exact_target &&
             second_state.tabs.front().navigation.dictionary_ids ==
                 ordered_intent.dictionary_ids &&
             query_->text() == main_query_before_activation;
    if (auto request = requests_.find(tab_id_before_activation);
        request != requests_.end()) {
        request->second->Cancel();
        requests_.erase(request);
    }
    auto* second_view = ArticleViewForTab(tab_id_before_activation);
    auto& second_search =
        article_search_presentations_[tab_id_before_activation];
    rendered_page_text_transports_.erase(tab_id_before_activation);
    ExtractRenderedPageText(
        tab_id_before_activation, second_view,
        second_search.accepted_query_generation,
        lookup_results_[tab_id_before_activation].generation,
        second_search.generation,
        article_navigation_generations_[tab_id_before_activation]);
    const bool second_planned = wait_for([this, tab_id_before_activation]() {
        const auto plan =
            rendered_text_match_plans_.find(tab_id_before_activation);
        return plan != rendered_text_match_plans_.end() &&
               !plan->second.identity.request.ignore_diacritics;
    });
    passed =
        passed && second_view != nullptr && second_planned &&
        pending_rendered_text_match_plan_ == std::nullopt &&
        rendered_page_text_transports_.count(tab_id_before_activation) == 1U;

    activation_events.clear();
    NavigateArticleTab(false);
    const auto replayed_state = facade_->GetArticleTabsState();
    passed = passed &&
             activation_events == QStringList{QStringLiteral("session")} &&
             replayed_state.tabs.front().navigation ==
                 ordered_state.tabs.front().navigation &&
             requests_.count(tab_id_before_activation) == 1U;
    if (auto request = requests_.find(tab_id_before_activation);
        request != requests_.end()) {
        request->second->Cancel();
        requests_.erase(request);
    }
    const std::array rejection_errors{
        goldendict::core::TabOperationError::kInvalidExactTarget,
        goldendict::core::TabOperationError::kExactTargetDictionaryUnavailable,
        goldendict::core::TabOperationError::kExactTargetDocumentNotFound,
        goldendict::core::TabOperationError::kInvalidNavigation,
        goldendict::core::TabOperationError::kTabLimitReached,
        goldendict::core::TabOperationError::kNavigationLimitReached};
    for (const auto error : rejection_errors) {
        activation_events.clear();
        const auto state_before_rejection = facade_->GetArticleTabsState();
        const auto session_before_rejection =
            facade_->ExportArticleTabSession();
        const auto search_before_rejection =
            article_search_presentations_[tab_id_before_activation];
        const auto pending_before_rejection =
            pending_article_search_handoffs_.count(tab_id_before_activation);
        const auto request_count_before_rejection = requests_.size();
        const auto open_count_before_rejection = capturing_facade.open_count;
        const QString query_before_rejection = query_->text();
        const int cursor_before_rejection = query_->cursorPosition();
        const int selection_before_rejection = query_->selectionStart();
        const QString selected_text_before_rejection = query_->selectedText();
        capturing_facade.forced_error = error;
        first->ResultActivationRequested(ordered_intent);
        capturing_facade.forced_error.reset();
        passed =
            passed && activation_events.empty() &&
            capturing_facade.open_count == open_count_before_rejection + 1U &&
            facade_->GetArticleTabsState().active_tab_id ==
                state_before_rejection.active_tab_id &&
            facade_->GetArticleTabsState().tabs.size() ==
                state_before_rejection.tabs.size() &&
            facade_->GetArticleTabsState().tabs.front().navigation ==
                state_before_rejection.tabs.front().navigation &&
            facade_->ExportArticleTabSession() == session_before_rejection &&
            requests_.size() == request_count_before_rejection &&
            pending_article_search_handoffs_.count(tab_id_before_activation) ==
                pending_before_rejection &&
            article_search_presentations_[tab_id_before_activation].query ==
                search_before_rejection.query &&
            article_search_presentations_[tab_id_before_activation].status ==
                search_before_rejection.status &&
            article_search_presentations_[tab_id_before_activation]
                    .generation == search_before_rejection.generation &&
            query_->text() == query_before_rejection &&
            query_->cursorPosition() == cursor_before_rejection &&
            query_->selectionStart() == selection_before_rejection &&
            query_->selectedText() == selected_text_before_rejection &&
            status_->text() == QStringLiteral("Unable to update article state");
    }
    auto run_highlight_smoke = [this, &wait_for]() {
        bool highlight_passed = true;
        const QString ordinary_query = article_search_->text();
        const bool ordinary_enabled = article_search_->isEnabled();
        const auto window_actions = findChildren<QAction*>();
        const auto f3_count = std::count_if(
            window_actions.cbegin(), window_actions.cend(),
            [](const QAction* action) {
                return action->shortcuts().contains(QKeySequence(Qt::Key_F3));
            });
        const auto shift_f3_count =
            std::count_if(window_actions.cbegin(), window_actions.cend(),
                          [](const QAction* action) {
                              return action->shortcuts().contains(
                                  QKeySequence(Qt::SHIFT | Qt::Key_F3));
                          });
        highlight_passed =
            dictionaries_action_->shortcut() == QKeySequence(Qt::Key_F3) &&
            f3_count == 1 && shift_f3_count == 0;
        auto* highlight_view = new ArticleView;
        auto* web_content = highlight_view->findChild<QWebEngineView*>(
            QStringLiteral("articleWebContent"));
        auto* navigation_row = highlight_view->findChild<QWidget*>(
            QStringLiteral("fullTextNavigationRow"));
        auto* previous_button = highlight_view->findChild<QPushButton*>(
            QStringLiteral("fullTextPrevious"));
        auto* next_button = highlight_view->findChild<QPushButton*>(
            QStringLiteral("fullTextNext"));
        auto* navigation_status = highlight_view->findChild<QLabel*>(
            QStringLiteral("fullTextNavigationStatus"));
        auto* view_layout =
            qobject_cast<QVBoxLayout*>(highlight_view->layout());
        highlight_passed =
            web_content != nullptr && navigation_row != nullptr &&
            previous_button != nullptr && next_button != nullptr &&
            navigation_status != nullptr && view_layout != nullptr &&
            view_layout->indexOf(web_content) == 0 &&
            view_layout->indexOf(navigation_row) == 1 &&
            navigation_row->isHidden() &&
            previous_button->text() == QStringLiteral("&Previous") &&
            next_button->text() == QStringLiteral("&Next") &&
            !previous_button->isEnabled() && !next_button->isEnabled() &&
            navigation_status->text().isEmpty();

        ArticleView page_replacement_view;
        auto* replacement_row = page_replacement_view.findChild<QWidget*>(
            QStringLiteral("fullTextNavigationRow"));
        auto* replacement_previous =
            page_replacement_view.findChild<QPushButton*>(
                QStringLiteral("fullTextPrevious"));
        auto* replacement_next = page_replacement_view.findChild<QPushButton*>(
            QStringLiteral("fullTextNext"));
        auto* replacement_status = page_replacement_view.findChild<QLabel*>(
            QStringLiteral("fullTextNavigationStatus"));
        int replacement_requests = 0;
        connect(&page_replacement_view,
                &ArticleView::FullTextNavigationRequested,
                &page_replacement_view,
                [&replacement_requests](ArticleHighlightNavigationDirection) {
                    ++replacement_requests;
                });
        page_replacement_view.PublishFullTextNavigationSnapshot(
            {true, QStringLiteral("replaced-owner"), 1, 3, true, true});
        page_replacement_view.setPage(
            new QWebEnginePage(&page_replacement_view));
        replacement_previous->click();
        replacement_next->click();
        highlight_passed = highlight_passed && replacement_row->isHidden() &&
                           !replacement_previous->isEnabled() &&
                           !replacement_next->isEnabled() &&
                           replacement_status->text().isEmpty() &&
                           replacement_requests == 0;

        int navigation_requests = 0;
        QList<ArticleHighlightNavigationDirection> requested_directions;
        connect(highlight_view, &ArticleView::FullTextNavigationRequested,
                highlight_view,
                [&navigation_requests, &requested_directions](
                    ArticleHighlightNavigationDirection direction) {
                    ++navigation_requests;
                    requested_directions.push_back(direction);
                });
        highlight_view->PublishFullTextNavigationSnapshot(
            {true, QStringLiteral("ui-owner"), 0, 1, false, false});
        highlight_passed =
            highlight_passed && !navigation_row->isHidden() &&
            navigation_status->text() == QStringLiteral("1 of 1 matches") &&
            !previous_button->isEnabled() && !next_button->isEnabled();
        highlight_view->PublishFullTextNavigationSnapshot(
            {true, QStringLiteral("ui-owner"), 1, 3, true, true});
        highlight_passed =
            highlight_passed &&
            navigation_status->text() == QStringLiteral("2 of 3 matches") &&
            previous_button->isEnabled() && next_button->isEnabled();
        previous_button->click();
        next_button->click();
        highlight_passed =
            highlight_passed && navigation_requests == 2 &&
            requested_directions ==
                QList<ArticleHighlightNavigationDirection>{
                    ArticleHighlightNavigationDirection::kPrevious,
                    ArticleHighlightNavigationDirection::kNext} &&
            navigation_status->text() == QStringLiteral("2 of 3 matches");
        highlight_view->PublishFullTextNavigationSnapshot(
            {false, QStringLiteral("rejected"), 0, 3, false, true});
        highlight_view->ClearFullTextNavigation(QStringLiteral("stale"));
        highlight_passed =
            highlight_passed && !navigation_row->isHidden() &&
            navigation_status->text() == QStringLiteral("2 of 3 matches") &&
            previous_button->isEnabled() && next_button->isEnabled();
        highlight_view->PublishFullTextNavigationSnapshot(
            {true, QStringLiteral("ui-owner"), 0, 3, false, true});
        highlight_passed =
            highlight_passed &&
            navigation_status->text() == QStringLiteral("1 of 3 matches") &&
            !previous_button->isEnabled() && next_button->isEnabled();
        highlight_view->PublishFullTextNavigationSnapshot(
            {true, QStringLiteral("ui-owner"), 2, 3, true, false});
        highlight_passed =
            highlight_passed &&
            navigation_status->text() == QStringLiteral("3 of 3 matches") &&
            previous_button->isEnabled() && !next_button->isEnabled();

        auto* independent_view = new ArticleView;
        auto* independent_row = independent_view->findChild<QWidget*>(
            QStringLiteral("fullTextNavigationRow"));
        independent_view->PublishFullTextNavigationSnapshot(
            {true, QStringLiteral("independent-owner"), 0, 2, false, true});
        QTabWidget tab_exposure;
        tab_exposure.addTab(highlight_view, QStringLiteral("first"));
        tab_exposure.addTab(independent_view, QStringLiteral("second"));
        tab_exposure.show();
        QApplication::processEvents();
        highlight_passed = highlight_passed && highlight_view->isVisible() &&
                           navigation_row->isVisible() &&
                           !independent_view->isVisible() &&
                           !independent_row->isHidden();
        tab_exposure.setCurrentWidget(independent_view);
        QApplication::processEvents();
        highlight_passed = highlight_passed && !highlight_view->isVisible() &&
                           independent_view->isVisible() &&
                           independent_row->isVisible();
        tab_exposure.setCurrentWidget(highlight_view);
        QApplication::processEvents();
        bool highlight_loaded = false;
        connect(
            highlight_view, &ArticleView::loadFinished, this,
            [&highlight_loaded](bool ok) { highlight_loaded = ok; },
            Qt::SingleShotConnection);
        const QString highlight_html = QStringLiteral(
            "<!doctype html><html><body><a id='kept' "
            "href='goldendict://lookup/x'>"
            "Alpha <span>Be</span><span>ta</span> alpha "
            "Beta</a></body></html>");
        highlight_view->setHtml(highlight_html);
        const bool initial_load_ready =
            wait_for([&highlight_loaded]() { return highlight_loaded; });
        highlight_passed = highlight_passed && initial_load_ready &&
                           navigation_row->isHidden() &&
                           navigation_status->text().isEmpty();
        QString highlight_text;
        bool highlight_text_ready = false;
        highlight_view->page()->toPlainText(
            [&highlight_text, &highlight_text_ready](const QString& text) {
                highlight_text = text;
                highlight_text_ready = true;
            });
        const bool initial_text_ready = wait_for(
            [&highlight_text_ready]() { return highlight_text_ready; });
        highlight_passed = highlight_passed && initial_text_ready;
        const qsizetype first_beta =
            highlight_text.indexOf(QStringLiteral("Beta"));
        const qsizetype first_alpha =
            highlight_text.indexOf(QStringLiteral("Alpha"));
        std::vector<ArticleHighlightRange> highlight_ranges;
        if (first_beta >= 0 && first_alpha >= 0) {
            highlight_ranges.push_back(
                {static_cast<std::size_t>(
                     highlight_text.left(first_beta).toUtf8().size()),
                 std::size_t{4}, QStringLiteral("Beta")});
            highlight_ranges.push_back(
                {static_cast<std::size_t>(
                     highlight_text.left(first_alpha).toUtf8().size()),
                 std::size_t{5}, QStringLiteral("Alpha")});
        }
        ArticleHighlightResult generation_a;
        bool generation_a_ready = false;
        highlight_view->ApplyFullTextHighlights(
            QStringLiteral("smoke-generation-a"), highlight_text,
            highlight_ranges, false,
            [&generation_a,
             &generation_a_ready](ArticleHighlightResult result) {
                generation_a = std::move(result);
                generation_a_ready = true;
            });
        highlight_passed =
            highlight_passed &&
            wait_for([&generation_a_ready]() { return generation_a_ready; }) &&
            generation_a.applied && generation_a.ordered_count == 2 &&
            generation_a.occurrence_count == 4 &&
            generation_a.current_position == 0;

        ArticleHighlightResult generation_b;
        bool generation_b_ready = false;
        highlight_view->ApplyFullTextHighlights(
            QStringLiteral("smoke-generation-b"), highlight_text,
            highlight_ranges, false,
            [&generation_b,
             &generation_b_ready](ArticleHighlightResult result) {
                generation_b = std::move(result);
                generation_b_ready = true;
            });
        highlight_passed =
            highlight_passed &&
            wait_for([&generation_b_ready]() { return generation_b_ready; }) &&
            generation_b.applied;
        highlight_view->ClearFullTextHighlights(
            QStringLiteral("smoke-generation-a"));

        bool instrumentation_ready = false;
        highlight_view->page()->runJavaScript(
            QStringLiteral(R"JS(
(() => {
  globalThis.__goldendictNavigationSelectionChanges = 0;
  globalThis.__goldendictNavigationScrolls = 0;
  const originalRemoveAllRanges = Selection.prototype.removeAllRanges;
  Selection.prototype.removeAllRanges = function(...args) {
    ++globalThis.__goldendictNavigationSelectionChanges;
    return originalRemoveAllRanges.apply(this, args);
  };
  const originalAddRange = Selection.prototype.addRange;
  Selection.prototype.addRange = function(...args) {
    ++globalThis.__goldendictNavigationSelectionChanges;
    return originalAddRange.apply(this, args);
  };
  const original = window.scrollTo.bind(window);
  window.scrollTo = (...args) => {
    ++globalThis.__goldendictNavigationScrolls;
    return original(...args);
  };
  return true;
})()
)JS"),
            QWebEngineScript::ApplicationWorld,
            [&instrumentation_ready](const QVariant&) {
                instrumentation_ready = true;
            });
        highlight_passed =
            highlight_passed && wait_for([&instrumentation_ready]() {
                return instrumentation_ready;
            });

        auto navigate = [highlight_view, &wait_for](
                            const QString& token,
                            ArticleHighlightNavigationDirection direction) {
            ArticleHighlightNavigationSnapshot snapshot;
            bool ready = false;
            highlight_view->NavigateFullTextHighlight(
                token, direction,
                [&snapshot, &ready](ArticleHighlightNavigationSnapshot result) {
                    snapshot = std::move(result);
                    ready = true;
                });
            wait_for([&ready]() { return ready; });
            return snapshot;
        };
        auto presentation_counts = [highlight_view, &wait_for]() {
            QPair<int, int> counts{-1, -1};
            bool ready = false;
            highlight_view->page()->runJavaScript(
                QStringLiteral(R"JS([
  globalThis.__goldendictNavigationSelectionChanges,
  globalThis.__goldendictNavigationScrolls
])JS"),
                QWebEngineScript::ApplicationWorld,
                [&counts, &ready](const QVariant& value) {
                    const QVariantList values = value.toList();
                    if (values.size() == 2)
                        counts = {values[0].toInt(), values[1].toInt()};
                    ready = true;
                });
            wait_for([&ready]() { return ready; });
            return counts;
        };
        const auto next = navigate(QStringLiteral("smoke-generation-b"),
                                   ArticleHighlightNavigationDirection::kNext);
        highlight_passed =
            highlight_passed && next.accepted && next.current_position == 1 &&
            next.ordered_count == 2 && next.can_previous && !next.can_next;
        const auto before_last_boundary = presentation_counts();
        const auto last_boundary =
            navigate(QStringLiteral("smoke-generation-b"),
                     ArticleHighlightNavigationDirection::kNext);
        highlight_passed = highlight_passed && last_boundary.accepted &&
                           last_boundary.current_position == 1 &&
                           last_boundary.can_previous &&
                           !last_boundary.can_next;
        const auto after_last_boundary = presentation_counts();
        highlight_passed =
            highlight_passed && before_last_boundary == after_last_boundary;
        const auto previous =
            navigate(QStringLiteral("smoke-generation-b"),
                     ArticleHighlightNavigationDirection::kPrevious);
        highlight_passed = highlight_passed && previous.accepted &&
                           previous.current_position == 0 &&
                           !previous.can_previous && previous.can_next;
        const auto before_first_boundary = presentation_counts();
        const auto first_boundary =
            navigate(QStringLiteral("smoke-generation-b"),
                     ArticleHighlightNavigationDirection::kPrevious);
        highlight_passed = highlight_passed && first_boundary.accepted &&
                           first_boundary.current_position == 0 &&
                           !first_boundary.can_previous &&
                           first_boundary.can_next;
        const auto after_first_boundary = presentation_counts();
        highlight_passed =
            highlight_passed && before_first_boundary == after_first_boundary;
        const auto wrong_token =
            navigate(QStringLiteral("smoke-generation-a"),
                     ArticleHighlightNavigationDirection::kNext);
        highlight_passed =
            highlight_passed && !wrong_token.accepted &&
            wrong_token.token == QStringLiteral("smoke-generation-a") &&
            wrong_token.current_position == -1 &&
            wrong_token.ordered_count == 0 && !wrong_token.can_previous &&
            !wrong_token.can_next;

        ArticleHighlightResult stale_failure;
        bool stale_failure_ready = false;
        auto invalid_ranges = highlight_ranges;
        if (!invalid_ranges.empty())
            invalid_ranges.front().literal = QStringLiteral("mismatch");
        highlight_view->ApplyFullTextHighlights(
            QStringLiteral("smoke-generation-a-failed"), highlight_text,
            invalid_ranges, false,
            [&stale_failure,
             &stale_failure_ready](ArticleHighlightResult result) {
                stale_failure = std::move(result);
                stale_failure_ready = true;
            });
        highlight_passed = highlight_passed &&
                           wait_for([&stale_failure_ready]() {
                               return stale_failure_ready;
                           }) &&
                           !stale_failure.applied;

        QVariantMap highlight_snapshot;
        bool highlight_snapshot_ready = false;
        highlight_view->page()->runJavaScript(
            QStringLiteral(R"JS(
(() => {
  const state = globalThis.__goldendictFullTextHighlightState;
  const published = state && state.published;
  const selection = window.getSelection();
  return {
    token: published ? published.token : '',
    ordered: published ? published.ordered.length : -1,
    position: published ? published.position : -1,
    selectionChanges: globalThis.__goldendictNavigationSelectionChanges,
    scrolls: globalThis.__goldendictNavigationScrolls,
    occurrences: published ? published.highlight.size : -1,
    selected: selection ? selection.toString() : '',
    styled: published ? document.adoptedStyleSheets.includes(published.sheet) &&
        published.sheet.cssRules.length === 1 &&
        published.sheet.cssRules[0].cssText.toLowerCase().includes('highlight') : false,
    registryOwned: published ?
        CSS.highlights.get('goldendict-full-text-match') === published.highlight : false,
    body: document.body.innerHTML,
    link: document.getElementById('kept').getAttribute('href'),
    spans: document.querySelectorAll('span').length
  };
})()
)JS"),
            QWebEngineScript::ApplicationWorld,
            [&highlight_snapshot,
             &highlight_snapshot_ready](const QVariant& value) {
                highlight_snapshot = value.toMap();
                highlight_snapshot_ready = true;
            });
        highlight_passed =
            highlight_passed && wait_for([&highlight_snapshot_ready]() {
                return highlight_snapshot_ready;
            }) &&
            highlight_snapshot.value(QStringLiteral("token")).toString() ==
                QStringLiteral("smoke-generation-b") &&
            highlight_snapshot.value(QStringLiteral("ordered")).toInt() == 2 &&
            highlight_snapshot.value(QStringLiteral("position")).toInt() == 0 &&
            highlight_snapshot.value(QStringLiteral("selectionChanges"))
                    .toInt() >= 0 &&
            highlight_snapshot.value(QStringLiteral("scrolls")).toInt() >= 2 &&
            highlight_snapshot.value(QStringLiteral("occurrences")).toInt() ==
                4 &&
            highlight_snapshot.value(QStringLiteral("selected")).toString() ==
                QStringLiteral("Beta") &&
            highlight_snapshot.value(QStringLiteral("styled")).toBool() &&
            highlight_snapshot.value(QStringLiteral("registryOwned"))
                .toBool() &&
            highlight_snapshot.value(QStringLiteral("link")).toString() ==
                QStringLiteral("goldendict://lookup/x") &&
            highlight_snapshot.value(QStringLiteral("spans")).toInt() == 2 &&
            highlight_snapshot.value(QStringLiteral("body"))
                .toString()
                .contains(QStringLiteral("<span>Be</span><span>ta</span>"));

        ArticleHighlightResult one_range;
        bool one_range_ready = false;
        if (!highlight_ranges.empty()) {
            highlight_view->ApplyFullTextHighlights(
                QStringLiteral("smoke-one-range"), highlight_text,
                std::vector<ArticleHighlightRange>{highlight_ranges.front()},
                false,
                [&one_range, &one_range_ready](ArticleHighlightResult result) {
                    one_range = std::move(result);
                    one_range_ready = true;
                });
        }
        highlight_passed =
            highlight_passed &&
            wait_for([&one_range_ready]() { return one_range_ready; }) &&
            one_range.applied && one_range.current_position == 0;
        const auto one_previous =
            navigate(QStringLiteral("smoke-one-range"),
                     ArticleHighlightNavigationDirection::kPrevious);
        const auto one_next =
            navigate(QStringLiteral("smoke-one-range"),
                     ArticleHighlightNavigationDirection::kNext);
        highlight_passed =
            highlight_passed && one_previous.accepted && one_next.accepted &&
            one_previous.current_position == 0 &&
            one_next.current_position == 0 && !one_previous.can_previous &&
            !one_previous.can_next && !one_next.can_previous &&
            !one_next.can_next;
        bool corrupt_ready = false;
        highlight_view->page()->runJavaScript(
            QStringLiteral(R"JS(
(() => {
  const state = globalThis.__goldendictFullTextHighlightState;
  if (state && state.published) state.published.position = 0.5;
  return true;
})()
)JS"),
            QWebEngineScript::ApplicationWorld,
            [&corrupt_ready](const QVariant&) { corrupt_ready = true; });
        highlight_passed = highlight_passed && wait_for([&corrupt_ready]() {
                               return corrupt_ready;
                           });
        const auto malformed =
            navigate(QStringLiteral("smoke-one-range"),
                     ArticleHighlightNavigationDirection::kNext);
        highlight_passed = highlight_passed && !malformed.accepted &&
                           malformed.current_position == -1 &&
                           malformed.ordered_count == 0;
        highlight_view->ClearFullTextHighlights(
            QStringLiteral("smoke-one-range"), true);
        const auto empty = navigate(QStringLiteral("smoke-one-range"),
                                    ArticleHighlightNavigationDirection::kNext);
        highlight_passed = highlight_passed && !empty.accepted;
        bool lifecycle_loaded = false;
        connect(
            highlight_view, &ArticleView::loadFinished, this,
            [&lifecycle_loaded](bool ok) { lifecycle_loaded = ok; },
            Qt::SingleShotConnection);
        highlight_view->setHtml(QStringLiteral(
            "<!doctype html><html><body>replacement</body></html>"));
        const bool lifecycle_ready =
            wait_for([&lifecycle_loaded]() { return lifecycle_loaded; });
        highlight_passed = highlight_passed && lifecycle_ready;
        const auto invalidated =
            navigate(QStringLiteral("smoke-one-range"),
                     ArticleHighlightNavigationDirection::kNext);
        highlight_passed = highlight_passed && !invalidated.accepted &&
                           navigation_row->isHidden() &&
                           navigation_status->text().isEmpty() &&
                           article_search_->text() == ordinary_query &&
                           article_search_->isEnabled() == ordinary_enabled &&
                           navigation_status != article_search_status_;
        return highlight_passed;
    };

    facade_ = capturing_facade.inner_;
    auto* search_button =
        first->findChild<QPushButton*>(QStringLiteral("fullTextSearchButton"));
    auto* cancel_button =
        first->findChild<QPushButton*>(QStringLiteral("fullTextCancelButton"));
    auto* progress = first->findChild<QProgressBar*>(
        QStringLiteral("fullTextSearchProgress"));
    auto* articles_found =
        first->findChild<QLabel*>(QStringLiteral("fullTextArticlesFoundLabel"));
    passed = passed && search_button != nullptr && cancel_button != nullptr &&
             progress != nullptr && articles_found != nullptr &&
             articles_found->parent() == first &&
             articles_found->text() == QStringLiteral("Articles found: 0") &&
             search_button->isEnabled() && cancel_button->isEnabled() &&
             progress->isHidden();
    if (search_button != nullptr && cancel_button != nullptr &&
        progress != nullptr) {
        search_button->click();
        passed = passed && !search_button->isEnabled() &&
                 cancel_button->isEnabled() && !progress->isHidden() &&
                 progress->minimum() == 0 && progress->maximum() == 0;
        cancel_button->click();
        passed = passed && search_button->isEnabled() &&
                 cancel_button->isEnabled() && progress->isHidden() &&
                 full_text_search_dialog_ == first &&
                 captured_geometries.empty();

        search_button->click();
        QElapsedTimer terminal_wait;
        terminal_wait.start();
        while (!search_button->isEnabled() && terminal_wait.elapsed() < 2000) {
            QApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        passed = passed && search_button->isEnabled() &&
                 cancel_button->isEnabled() && progress->isHidden();
    }
    auto* first_mode =
        first->findChild<QComboBox*>(QStringLiteral("fullTextQueryMode"));
    if (first_mode != nullptr) {
        first_mode->setCurrentIndex((first_mode->currentIndex() + 1) %
                                    first_mode->count());
    }

    auto* current_facade = facade_;
    SetFacade(current_facade);
    passed = passed && full_text_search_dialog_ == first &&
             full_text_search_action_->isEnabled() &&
             captured_geometries.empty();
    first->resize(640, 480);
    first->move(first->pos() + QPoint(23, 29));
    QApplication::processEvents();
    const QScreen* geometry_screen = first->screen();
    if (geometry_screen != nullptr) {
        const QRect available = geometry_screen->availableGeometry();
        first->move(first->pos() +
                    (available.center() - first->frameGeometry().center()));
        QApplication::processEvents();
        passed = passed && available.contains(first->frameGeometry());
    } else {
        passed = false;
    }
    const QByteArray idle_cancel_geometry = first->saveGeometry();
    cancel_button->click();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    passed =
        passed && full_text_search_dialog_ == nullptr &&
        captured_geometries.size() == 1U &&
        captured_geometries.front() ==
            std::string(idle_cancel_geometry.constData(),
                        static_cast<std::size_t>(idle_cancel_geometry.size()));

    query_->setText(QStringLiteral("fresh shell query"));
    full_text_search_action_->trigger();
    QApplication::processEvents();
    auto* reopened = full_text_search_dialog_.data();
    auto* reopened_query = reopened == nullptr
                               ? nullptr
                               : reopened->findChild<QLineEdit*>(
                                     QStringLiteral("fullTextQueryText"));
    auto* reopened_mode = reopened == nullptr
                              ? nullptr
                              : reopened->findChild<QComboBox*>(
                                    QStringLiteral("fullTextQueryMode"));
    passed =
        passed && reopened != nullptr && reopened_query != nullptr &&
        reopened_mode != nullptr &&
        reopened->saveGeometry() == idle_cancel_geometry &&
        reopened_mode->currentData().toInt() ==
            static_cast<int>(preferences_.full_text_search_mode) &&
        reopened_query->text() == QStringLiteral("fresh shell query") &&
        reopened_query->selectedText() == QStringLiteral("fresh shell query");
    if (reopened != nullptr) {
        activation_events.clear();
        auto reopened_intent = ordered_intent;
        reopened_intent.result.headword = "replacement-dialog-headword";
        reopened->ResultActivationRequested(reopened_intent);
        passed = passed &&
                 activation_events == QStringList{QStringLiteral("session"),
                                                  QStringLiteral("lookup")};
        if (auto request =
                requests_.find(TabIdAt(article_tabs_->currentIndex()));
            request != requests_.end()) {
            request->second->Cancel();
            requests_.erase(request);
        }

        goldendict::core::TabOperationResult bounded_result;
        do {
            goldendict::core::TabNavigationState bounded;
            bounded.kind = goldendict::core::TabNavigationKind::kLookup;
            bounded.query = "full-text-limit-" +
                            std::to_string(facade_->ExportArticleTabSession()
                                               .tabs.front()
                                               .history.size());
            bounded.title = bounded.query;
            bounded_result = facade_->OpenArticleTab(
                bounded, goldendict::core::TabOpenPolicy::kCurrentTab,
                goldendict::core::TabActivationPolicy::kActivate);
        } while (bounded_result);
        const auto session_before_limit_failure =
            facade_->ExportArticleTabSession();
        activation_events.clear();
        reopened->ResultActivationRequested(ordered_intent);
        passed =
            passed && activation_events.empty() && requests_.empty() &&
            facade_->ExportArticleTabSession() ==
                session_before_limit_failure &&
            status_->text() == QStringLiteral("Unable to update article state");
    }
    QByteArray window_close_geometry;
    if (reopened != nullptr) {
        window_close_geometry = reopened->saveGeometry();
        reopened->close();
    }
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    passed =
        passed && preferences_ == preferences_before &&
        captured_geometries.size() == 2U &&
        captured_geometries.back() ==
            std::string(window_close_geometry.constData(),
                        static_cast<std::size_t>(window_close_geometry.size()));
    activation_events.clear();
    NavigateToFullTextResult(ordered_intent);
    passed = passed && activation_events.empty() && requests_.empty();
    disconnect(session_connection);
    disconnect(lookup_connection);
    disconnect(geometry_connection);
    const bool row_smoke_passed = run_highlight_smoke();
    passed = passed && row_smoke_passed;
    const auto teardown_tab_id = TabIdAt(article_tabs_->currentIndex());
    if (auto* teardown_view = ArticleViewForTab(teardown_tab_id);
        teardown_view != nullptr &&
        article_search_presentations_.count(teardown_tab_id) == 1U &&
        lookup_results_.count(teardown_tab_id) == 1U &&
        article_navigation_generations_.count(teardown_tab_id) == 1U) {
        const auto& teardown_search =
            article_search_presentations_.at(teardown_tab_id);
        ExtractRenderedPageText(
            teardown_tab_id, teardown_view,
            teardown_search.accepted_query_generation,
            lookup_results_.at(teardown_tab_id).generation,
            teardown_search.generation,
            article_navigation_generations_.at(teardown_tab_id));
    }
    SetFacade(nullptr);
    QElapsedTimer teardown_timer;
    teardown_timer.start();
    while (teardown_timer.elapsed() < 100)
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    passed = passed && rendered_page_text_transports_.empty() &&
             article_navigation_generations_.empty();
    activation_events.clear();
    NavigateToFullTextResult(ordered_intent);
    passed = passed && !full_text_search_action_->isEnabled();
    completion(passed);
}

void MainWindow::AdvancePresentationMutationEpoch() noexcept {
    if (WidgetsInteractionBlocked())
        return;
    ++presentation_mutation_epoch_;
    if (presentation_mutation_epoch_ == 0U)
        presentation_mutation_epoch_ = 1U;
}

void MainWindow::ReclaimFacadeCandidate(bool force) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (facade_preparation_record_ == nullptr)
        return;
    if (!force && !facade_preparation_record_->abandoned.load(
                      std::memory_order_acquire)) {
        return;
    }
    auto record = std::move(facade_preparation_record_);
    record->ready.store(false, std::memory_order_release);
    record->owner.store(nullptr, std::memory_order_release);
    if (record->resources != nullptr && record->resources->relay != nullptr)
        record->resources->relay->Disable();
    record->reclaimed_on_owner_thread.store(
        record->resources == nullptr || record->resources->root == nullptr ||
            record->resources->root->thread() == QThread::currentThread(),
        std::memory_order_release);
    delete record->resources;
    record->resources = nullptr;
    record->reclaimed.store(true, std::memory_order_release);
    if (facade_candidate_reclaimer_->isActive())
        facade_candidate_reclaimer_->stop();
}

void MainWindow::ReclaimAbandonedFacadeCandidates() {
    ReclaimFacadeCandidate(false);
}

PreparedWidgetsFacadeCandidate MainWindow::PrepareFacadeCandidate(
    std::shared_ptr<goldendict::core::DesktopFacade> facade,
    const goldendict::core::ApplicationPreferences& preferences,
    const std::vector<goldendict::core::DictionaryGroupConfiguration>& groups) {
    if (QThread::currentThread() != thread() || qApp == nullptr ||
        thread() != qApp->thread() || facade_preparation_shutdown_ ||
        facade == nullptr) {
        return {};
    }

    ++facade_preparation_generation_;
    if (facade_preparation_generation_ == 0U)
        facade_preparation_generation_ = 1U;
    if (facade_preparation_record_ != nullptr) {
        facade_preparation_record_->abandoned.store(true,
                                                    std::memory_order_release);
        ReclaimFacadeCandidate(true);
    }

    const auto generation = facade_preparation_generation_;
    const auto epoch = presentation_mutation_epoch_;
    auto record = std::make_shared<WidgetsFacadePreparationRecord>();
    record->owner.store(this, std::memory_order_release);
    record->generation = generation;
    record->epoch = epoch;
    std::unique_ptr<WidgetsFacadePreparationResources> resources;
    try {
        resources = std::make_unique<WidgetsFacadePreparationResources>();
        resources->facade = std::move(facade);
        resources->facade_alias = resources->facade.get();
        resources->preferences = preferences;
        resources->groups = groups;
        resources->catalog =
            resources->facade->GetDictionaryService().GetCatalog();
        resources->binding_descriptor = std::make_unique<
            goldendict::widgets::WidgetsFacadeBindingDescriptor>();
        auto& binding = *resources->binding_descriptor;
        binding.facade_owner = resources->facade;
        binding.facade = resources->facade.get();
        binding.service = &resources->facade->GetDictionaryService();
        binding.catalog = resources->catalog;
        for (std::size_t index = 0U; index < binding.catalog.size(); ++index)
            binding.catalog_index.emplace(binding.catalog[index].id, index);
        int binding_failure_step = 10;
        const auto register_binding = [this, &binding, &binding_failure_step](
                                          auto consumer, bool installed) {
            if (!installed || !binding.RegisterConsumer(consumer))
                throw std::runtime_error("Incomplete facade consumer binding");
            if (facade_preparation_failure_step_ == binding_failure_step++)
                throw std::runtime_error("Injected consumer-binding failure");
        };
        register_binding(
            goldendict::widgets::FacadeBindingConsumer::kSchemeHandler,
            scheme_handler_ != nullptr && scheme_handler_->UsesBindingRegistry(
                                              facade_binding_registry_.get()));
        register_binding(
            goldendict::widgets::FacadeBindingConsumer::kDictionaryBrowser,
            dictionary_browser_ == nullptr ||
                dictionary_browser_->UsesBindingRegistry(
                    facade_binding_registry_.get()));
        resources->tabs = resources->facade->GetArticleTabsState();
        resources->query_text = query_->text();
        resources->query_cursor = query_->cursorPosition();
        resources->query_selection_start = query_->selectionStart();
        resources->query_selection_length = query_->selectedText().size();
        resources->article_search_text = article_search_->text();
        resources->selected_group_id = selected_group_id_;
        resources->participating_ids = participating_ids_;
        resources->dictionary_members = dictionary_members_;
        resources->solo_restore_ids = solo_restore_ids_;
        resources->mru_tab_ids = mru_tab_ids_;
        resources->dictionary_bar_visible = dictionary_bar_->isVisible();
        resources->focused_widget = QApplication::focusWidget();
        resources->active_dictionary_browser = dictionary_browser_;
        resources->active_full_text_dialog = full_text_search_dialog_;
        for (int index = 0; index < article_tabs_->count(); ++index) {
            const auto tab_id = TabIdAt(index);
            const auto* view =
                qobject_cast<ArticleView*>(article_tabs_->widget(index));
            if (tab_id != 0U && view != nullptr)
                resources->scroll_positions.emplace(
                    tab_id, view->page()->scrollPosition());
        }

        auto* root = new QWidget;
        resources->root = root;
        root->setObjectName(QStringLiteral("widgetsFacadeCandidateRoot"));
        root->setAttribute(Qt::WA_DontShowOnScreen, true);
        root->setEnabled(false);
        root->hide();
        auto* relay =
            new WidgetsFacadeActivationRelay(this, generation, epoch, root);
        resources->relay = relay;
        auto* layout = new QVBoxLayout(root);
        auto* staged_groups = new QComboBox;
        staged_groups->setObjectName(
            QStringLiteral("widgetsFacadeCandidateGroups"));
        staged_groups->addItem(tr("All Dictionaries"),
                               QVariant::fromValue<quint32>(0U));
        for (const auto& group : groups) {
            QIcon icon;
            if (!group.encoded_icon_data.empty()) {
                QPixmap pixmap;
                if (!pixmap.loadFromData(QByteArray::fromBase64(
                        QByteArray::fromStdString(group.encoded_icon_data)))) {
                    throw std::runtime_error("Invalid dictionary group icon");
                }
                icon = QIcon(pixmap);
            }
            staged_groups->addItem(icon, QString::fromStdString(group.name),
                                   QVariant::fromValue<quint32>(group.id));
            const QKeySequence shortcut(QString::fromStdString(group.shortcut));
            if (!shortcut.isEmpty()) {
                auto* action = new QAction(root);
                action->setShortcut(shortcut);
                action->setEnabled(false);
                const auto group_id = group.id;
                connect(action, &QAction::triggered, relay,
                        [relay, group_id]() { relay->SelectGroup(group_id); });
                root->addAction(action);
            }
        }
        resources->group_selector = staged_groups;
        resources->group_host = group_selector_host_;
        if (!group_selector_host_->AttachInactive(staged_groups))
            throw std::runtime_error("Unable to attach group presentation");
        if (facade_preparation_failure_step_ == 0)
            throw std::runtime_error("Injected group-slot failure");
        connect(staged_groups, &QComboBox::currentIndexChanged, relay,
                [relay](int index) { relay->GroupSelectionChanged(index); });
        relay->MarkConnected(WidgetsFacadeActivationRelay::kGroupSelector);
        if (facade_preparation_failure_step_ == 3)
            throw std::runtime_error("Injected group-action failure");
        for (int index = 0; index < staged_groups->count(); ++index) {
            if (staged_groups->itemData(index).toUInt() == selected_group_id_) {
                staged_groups->setCurrentIndex(index);
                break;
            }
        }

        auto* staged_bar = dictionary_bar_host_->AttachInactivePage();
        if (staged_bar == nullptr)
            throw std::runtime_error(
                "Unable to attach dictionary presentation");
        staged_bar->setObjectName(
            QStringLiteral("widgetsFacadeCandidateDictionaryBar"));
        resources->dictionary_bar = staged_bar;
        resources->dictionary_host = dictionary_bar_host_;
        if (facade_preparation_failure_step_ == 1)
            throw std::runtime_error("Injected dictionary-slot failure");
        const auto member_entry = dictionary_members_.find(selected_group_id_);
        std::vector<std::string> members;
        if (member_entry != dictionary_members_.end()) {
            members = member_entry->second;
        } else {
            for (const auto& dictionary : resources->catalog)
                members.push_back(dictionary.id);
        }
        const auto enabled_entry = participating_ids_.find(selected_group_id_);
        const auto& enabled = enabled_entry == participating_ids_.end()
                                  ? members
                                  : enabled_entry->second;
        for (const auto& dictionary_id : members) {
            const auto identity = std::find_if(
                resources->catalog.begin(), resources->catalog.end(),
                [&dictionary_id](const auto& candidate) {
                    return candidate.id == dictionary_id;
                });
            if (identity == resources->catalog.end())
                continue;
            const QString label = QString::fromStdString(
                identity->name.empty() ? identity->id : identity->name);
            auto* action = dictionary_bar_host_->AddAction(staged_bar, label);
            if (action == nullptr)
                throw std::runtime_error("Unable to add dictionary action");
            action->setCheckable(true);
            action->setChecked(std::find(enabled.begin(), enabled.end(),
                                         identity->id) != enabled.end());
            action->setData(QString::fromStdString(identity->id));
            action->setToolTip(label);
            action->setWhatsThis(
                tr("Include %1 in dictionary lookups and suggestions")
                    .arg(label));
            action->setEnabled(false);
            connect(action, &QAction::triggered, relay,
                    [relay, dictionary_id](bool checked) {
                        relay->DictionaryAction(
                            dictionary_id, checked,
                            QApplication::keyboardModifiers());
                    });
        }
        relay->MarkConnected(WidgetsFacadeActivationRelay::kActions);
        if (facade_preparation_failure_step_ == 4)
            throw std::runtime_error("Injected dictionary-action failure");

        const auto active_tab = std::find_if(
            resources->tabs.tabs.begin(), resources->tabs.tabs.end(),
            [&resources](const auto& tab) {
                return tab.id == resources->tabs.active_tab_id;
            });
        const bool has_active_tab = active_tab != resources->tabs.tabs.end();

        struct PreparedAction {
            const char* name;
            bool desired_enabled;
            WidgetsFacadeActivationRelay::Action action;
        };

        const std::array<PreparedAction, 5> action_states{{
            {"widgetsFacadeCandidateBack",
             has_active_tab && active_tab->can_go_back,
             WidgetsFacadeActivationRelay::Action::kBack},
            {"widgetsFacadeCandidateForward",
             has_active_tab && active_tab->can_go_forward,
             WidgetsFacadeActivationRelay::Action::kForward},
            {"widgetsFacadeCandidateNewTab", true,
             WidgetsFacadeActivationRelay::Action::kNewTab},
            {"widgetsFacadeCandidateFullText", true,
             WidgetsFacadeActivationRelay::Action::kFullText},
            {"widgetsFacadeCandidateSave", has_active_tab,
             WidgetsFacadeActivationRelay::Action::kSave},
        }};
        for (const auto& [name, desired_enabled, prepared_action] :
             action_states) {
            auto* action = new QAction(root);
            action->setObjectName(QString::fromLatin1(name));
            action->setProperty("desiredEnabled", desired_enabled);
            action->setEnabled(false);
            connect(action, &QAction::triggered, relay,
                    [relay, prepared_action]() {
                        relay->TriggerAction(prepared_action);
                    });
            root->addAction(action);
        }

        auto* staged_tabs = new QTabWidget;
        staged_tabs->setObjectName(
            QStringLiteral("widgetsFacadeCandidateArticleTabs"));
        staged_tabs->setTabBarAutoHide(preferences.hide_single_tab);
        staged_tabs->setTabsClosable(true);
        staged_tabs->setDocumentMode(true);
        staged_tabs->setMovable(false);
        staged_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
        resources->article_tabs = staged_tabs;
        resources->article_tabs_host = article_tabs_host_;
        if (!article_tabs_host_->AttachInactive(staged_tabs))
            throw std::runtime_error("Unable to attach article presentation");
        if (facade_preparation_failure_step_ == 2)
            throw std::runtime_error("Injected article-slot failure");
        connect(staged_tabs, &QTabWidget::currentChanged, relay,
                [relay](int index) { relay->ActivateTab(index); });
        connect(staged_tabs, &QTabWidget::tabCloseRequested, relay,
                [relay](int index) { relay->CloseTab(index); });
        connect(staged_tabs->tabBar(), &QTabBar::tabMoved, relay,
                [relay](int, int) { relay->TabMoved(); });
        connect(staged_tabs->tabBar(), &QWidget::customContextMenuRequested,
                relay, [relay](const QPoint& position) {
                    relay->TabContextMenu(position);
                });
        relay->MarkConnected(WidgetsFacadeActivationRelay::kArticleTabs);
        int active_index = -1;
        for (const auto& tab : resources->tabs.tabs) {
            auto* view = new ArticleView(staged_tabs);
            connect(view, &ArticleView::PageReplaced, relay,
                    [relay, tab_id = tab.id, view]() {
                        relay->ArticlePageReplaced(tab_id, view);
                    });
            view->SetFacade(resources->facade.get());
            view->SetClickPreferences(preferences.double_click_translates,
                                      preferences.select_word_by_single_click);
            view->setProperty("articleTabId",
                              QVariant::fromValue<qulonglong>(tab.id));
            auto* page = new ArticlePage(view);
            page->SetFacade(resources->facade.get());
            page->SetOpenNewTabsInBackground(
                preferences.open_new_tabs_in_background);
            view->setPage(page);
            connect(page, &QWebEnginePage::scrollPositionChanged, relay,
                    [relay](const QPointF&) { relay->ScrollChanged(); });
            connect(view, &ArticleView::loadStarted, relay,
                    [relay, tab_id = tab.id, view]() {
                        relay->ArticleLoadStarted(tab_id, view);
                    });
            connect(page, &ArticlePage::LookupRequested, relay,
                    [relay, tab_id = tab.id](
                        const QString&, const QString& internal_url,
                        ArticleLinkDisposition disposition) {
                        relay->PageLookup(tab_id, internal_url, disposition);
                    });
            connect(page, &ArticlePage::ExternalUrlRequested, relay,
                    [relay](const QUrl& url) { relay->ExternalUrl(url); });
            connect(view, &ArticleView::LinkRequested, relay,
                    [relay, tab_id = tab.id](
                        const QUrl& url, ArticleLinkDisposition disposition) {
                        relay->ArticleLink(tab_id, url, disposition);
                    });
            connect(
                view, &ArticleView::SelectionLookupRequested, relay,
                [relay, tab_id = tab.id](const QString& text,
                                         ArticleLinkDisposition disposition) {
                    relay->SelectionLookup(tab_id, text, disposition);
                });
            connect(view, &ArticleView::SelectionToInputRequested, relay,
                    [relay](const QString& text) {
                        relay->SelectionToInput(text);
                    });
            connect(view, &ArticleView::ExternalUrlRequested, relay,
                    [relay](const QUrl& url) { relay->ExternalUrl(url); });
            connect(view, &ArticleView::DictionaryResultRequested, relay,
                    [relay, tab_id = tab.id, view](
                        const QString& dictionary_id, int first_result_index,
                        quint64 presentation_generation) {
                        relay->DictionaryResult(tab_id, view, dictionary_id,
                                                first_result_index,
                                                presentation_generation);
                    });
            connect(view, &ArticleView::DictionaryResultsPaneRequested, relay,
                    [relay, tab_id = tab.id,
                     view](quint64 presentation_generation) {
                        relay->DictionaryResultsPane(tab_id, view,
                                                     presentation_generation);
                    });
            connect(view, &ArticleView::urlChanged, relay,
                    [relay](const QUrl&) { relay->NavigationChanged(); });
            connect(view, &ArticleView::loadFinished, relay,
                    [relay, tab_id = tab.id, view](bool success) {
                        relay->ArticleLoadFinished(tab_id, view, success);
                    });
            connect(view, &ArticleView::HtmlNavigationFinished, relay,
                    [relay, tab_id = tab.id, view](quint64 navigation_token,
                                                   bool success) {
                        relay->ArticleHtmlNavigationFinished(
                            tab_id, view, navigation_token, success);
                    });
            connect(view, &ArticleView::printFinished, relay,
                    [relay, view](bool success) {
                        relay->PrintFinished(view, success);
                    });
            connect(view, &ArticleView::FullTextNavigationRequested, relay,
                    [relay, tab_id = tab.id](
                        ArticleHighlightNavigationDirection direction) {
                        relay->FullTextNavigation(tab_id, direction);
                    });
            const QString title = QString::fromStdString(
                tab.navigation.title.empty() ? tab.navigation.query
                                             : tab.navigation.title);
            const int index = staged_tabs->addTab(view, title);
            if (tab.id == resources->tabs.active_tab_id)
                active_index = index;
        }
        relay->MarkConnected(WidgetsFacadeActivationRelay::kArticlePages);
        relay->MarkConnected(WidgetsFacadeActivationRelay::kArticleViews);
        if (facade_preparation_failure_step_ == 5)
            throw std::runtime_error("Injected article-view failure");
        if (facade_preparation_failure_step_ == 6)
            throw std::runtime_error("Injected relay-completion failure");
        register_binding(
            goldendict::widgets::FacadeBindingConsumer::kArticlePages,
            staged_tabs->count() ==
                static_cast<int>(resources->tabs.tabs.size()));
        register_binding(
            goldendict::widgets::FacadeBindingConsumer::kArticleViews,
            staged_tabs->count() ==
                static_cast<int>(resources->tabs.tabs.size()));
        if (active_index >= 0)
            staged_tabs->setCurrentIndex(active_index);
        resources->article_view_alias =
            active_index < 0
                ? nullptr
                : qobject_cast<ArticleView*>(staged_tabs->widget(active_index));
        resources->article_page_alias =
            resources->article_view_alias == nullptr
                ? nullptr
                : qobject_cast<ArticlePage*>(
                      resources->article_view_alias->page());

        resources->suggestion_worker = std::make_unique<SuggestionWorker>(
            [relay](goldendict::core::ArticleTabId tab_id,
                    std::uint64_t work_generation,
                    goldendict::core::SuggestionResponse response) {
                relay->SuggestionFinished(tab_id, work_generation,
                                          std::move(response));
            });
        relay->MarkConnected(WidgetsFacadeActivationRelay::kSuggestions);
        if (facade_preparation_failure_step_ == 7)
            throw std::runtime_error("Injected suggestion-worker failure");
        register_binding(
            goldendict::widgets::FacadeBindingConsumer::kSuggestions,
            resources->suggestion_worker != nullptr);
        resources->match_controller = new RenderedTextMatchPlanController(
            [relay](std::uint64_t work_generation,
                    goldendict::core::RenderedTextMatchPlanResult result) {
                relay->RenderedMatchFinished(work_generation,
                                             std::move(result));
            },
            root);
        relay->MarkConnected(WidgetsFacadeActivationRelay::kRenderedMatches);
        resources->match_controller->SetFacade(resources->facade.get());
        if (facade_preparation_failure_step_ == 8)
            throw std::runtime_error("Injected rendered-controller failure");
        register_binding(
            goldendict::widgets::FacadeBindingConsumer::kRenderedMatches,
            resources->match_controller != nullptr);
        resources->full_text_dialog = new goldendict::app::FullTextSearchDialog(
            preferences, &resources->facade->GetDictionaryService(), {}, root);
        resources->full_text_dialog->SetBindingRegistry(
            facade_binding_registry_.get());
        if (facade_preparation_failure_step_ == 9)
            throw std::runtime_error("Injected full-text-dialog failure");
        register_binding(
            goldendict::widgets::FacadeBindingConsumer::kFullTextDialog,
            resources->full_text_dialog != nullptr &&
                resources->full_text_dialog->UsesBindingRegistry(
                    facade_binding_registry_.get()));
        resources->full_text_dialog->setAttribute(Qt::WA_DontShowOnScreen,
                                                  true);
        resources->full_text_dialog->setEnabled(false);
        resources->full_text_dialog->hide();
        connect(
            resources->full_text_dialog,
            &goldendict::app::FullTextSearchDialog::ResultActivationRequested,
            relay,
            [relay](auto intent) { relay->FullTextResult(std::move(intent)); });
        connect(
            resources->full_text_dialog,
            &goldendict::app::FullTextSearchDialog::AcceptedQueryInvalidated,
            relay, [relay]() { relay->FullTextQueryInvalidated(); });
        connect(resources->full_text_dialog,
                &goldendict::app::FullTextSearchDialog::GeometryCaptured, relay,
                [relay](std::string geometry) {
                    relay->FullTextGeometry(std::move(geometry));
                });
        relay->MarkConnected(WidgetsFacadeActivationRelay::kFullTextOutputs);

        const auto isolate_surface = [&resources](QWidget* surface) {
            const auto record_surface = [&resources](QWidget* widget) {
                resources->surface_states.push_back(
                    {widget, widget->focusPolicy(),
                     widget->testAttribute(Qt::WA_TransparentForMouseEvents),
                     widget->isEnabled()});
            };
            record_surface(surface);
            for (auto* child : surface->findChildren<QWidget*>())
                record_surface(child);
            surface->setFocusPolicy(Qt::NoFocus);
            surface->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            for (auto* child : surface->findChildren<QWidget*>()) {
                child->setFocusPolicy(Qt::NoFocus);
                child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            }
        };
        isolate_surface(staged_groups);
        isolate_surface(staged_bar);
        isolate_surface(staged_tabs);

        auto* status = new QLabel(
            tr("%1 dictionary loaded")
                .arg(static_cast<qulonglong>(resources->catalog.size())),
            root);
        status->setObjectName(QStringLiteral("widgetsFacadeCandidateStatus"));
        layout->addWidget(status);

        resources->binding_registry = facade_binding_registry_.get();
        resources->binding_slot = facade_binding_registry_->Prepare(
            std::move(*resources->binding_descriptor));
        resources->binding_descriptor.reset();
        if (facade_preparation_failure_step_ == 17)
            throw std::runtime_error("Injected facade-binding failure");
        if (!resources->binding_slot.has_value() ||
            !facade_binding_registry_->Ready(*resources->binding_slot) ||
            !facade_binding_registry_->AuditClosedLeaseProtocol()) {
            throw std::runtime_error("Widgets facade binding is incomplete");
        }
        resources->binding_slot_alias = *resources->binding_slot;
        resources->group_selector_alias = resources->group_selector.data();
        resources->article_tabs_alias = resources->article_tabs.data();
        resources->full_text_dialog_alias = resources->full_text_dialog.data();
        resources->suggestion_worker_alias = resources->suggestion_worker.get();
        resources->match_controller_alias = resources->match_controller.data();

        if (!relay->IsComplete() ||
            !relay->CanPublishWithoutAllocation(this, generation, epoch)) {
            throw std::runtime_error(
                "Widgets facade activation relay is incomplete");
        }
        if (facade_preparation_failure_step_ == 18)
            throw std::runtime_error("Injected final-audit failure");
    } catch (...) {
        return {};
    }

    if (facade_preparation_shutdown_ ||
        facade_preparation_generation_ != generation ||
        presentation_mutation_epoch_ != epoch) {
        return {};
    }
    record->resources = resources.release();
    facade_preparation_record_ = record;
    if (!facade_candidate_reclaimer_->isActive())
        facade_candidate_reclaimer_->start();
    record->ready.store(true, std::memory_order_release);
    return PreparedWidgetsFacadeCandidate(std::move(record));
}

bool MainWindow::IsFacadeCandidateCurrent(
    const PreparedWidgetsFacadeCandidate& candidate) const noexcept {
    const auto& record = candidate.record_;
    return QThread::currentThread() == thread() &&
           !facade_preparation_shutdown_ && record != nullptr &&
           record == facade_preparation_record_ &&
           record->owner.load(std::memory_order_acquire) == this &&
           record->generation == facade_preparation_generation_ &&
           record->epoch == presentation_mutation_epoch_ &&
           record->ready.load(std::memory_order_acquire) &&
           !record->abandoned.load(std::memory_order_acquire) &&
           !record->reclaimed.load(std::memory_order_acquire);
}

bool MainWindow::WidgetsInteractionBlocked() const noexcept {
    return widgets_interaction_gate_closed_.load(std::memory_order_acquire);
}

BeginWidgetsMaintenanceResult MainWindow::BeginFacadeCandidateMaintenance(
    PreparedWidgetsFacadeCandidate& candidate) {
    BeginWidgetsMaintenanceResult result;
    if (!IsFacadeCandidateCurrent(candidate) || widgets_maintenance_active_ ||
        maintained_facade_record_ != nullptr ||
        retired_facade_resources_ != nullptr) {
        return result;
    }
    const auto& record = candidate.record_;
    auto* resources = record->resources;
    if (resources == nullptr || resources->binding_registry == nullptr ||
        !resources->binding_slot.has_value() ||
        !resources->binding_registry->CanPublish(*resources->binding_slot) ||
        resources->relay == nullptr ||
        !resources->relay->CanPublishWithoutAllocation(this, record->generation,
                                                       record->epoch) ||
        resources->group_host != group_selector_host_ ||
        resources->dictionary_host != dictionary_bar_host_ ||
        resources->article_tabs_host != article_tabs_host_ ||
        group_selector_host_->InactivePage() != resources->group_selector ||
        article_tabs_host_->InactivePage() != resources->article_tabs ||
        !group_selector_host_->Prepared() ||
        !dictionary_bar_host_->Prepared() || !article_tabs_host_->Prepared()) {
        return result;
    }

    bool expected = false;
    if (!widgets_interaction_gate_closed_.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return result;
    }
    widgets_maintenance_active_ = true;
    record->state.store(WidgetsFacadePreparationRecord::State::kMaintaining,
                        std::memory_order_release);
    resources->old_updates_enabled = updatesEnabled();
    resources->old_focus = QApplication::focusWidget();
    resources->old_group_selector = group_selector_;
    resources->old_dictionary_bar = dictionary_bar_host_->ActivePage();
    resources->old_article_tabs = article_tabs_;
    setUpdatesEnabled(false);

    const auto injected = [this](int step) {
        return facade_maintenance_failure_step_ == step;
    };
    bool maintained = !injected(0);
    if (maintained) {
        for (const auto& state : resources->surface_states) {
            if (state.widget == nullptr) {
                maintained = false;
                break;
            }
            state.widget->setFocusPolicy(state.focus_policy);
            state.widget->setAttribute(Qt::WA_TransparentForMouseEvents,
                                       state.transparent_for_mouse);
            state.widget->setEnabled(state.enabled);
        }
    }
    maintained = maintained && !injected(1);
    if (maintained) {
        resources->group_switched =
            group_selector_host_->BeginMaintenanceSwitch();
        maintained = resources->group_switched;
    }
    maintained = maintained && !injected(2);
    if (maintained) {
        resources->dictionary_switched =
            dictionary_bar_host_->BeginMaintenanceSwitch();
        maintained = resources->dictionary_switched;
    }
    maintained = maintained && !injected(3);
    if (maintained) {
        resources->article_switched =
            article_tabs_host_->BeginMaintenanceSwitch();
        maintained = resources->article_switched;
    }
    maintained = maintained && !injected(4);
    if (maintained) {
        query_->setText(resources->query_text);
        query_->setCursorPosition(resources->query_cursor);
        if (resources->query_selection_start >= 0) {
            query_->setSelection(resources->query_selection_start,
                                 resources->query_selection_length);
        }
        article_search_->setText(resources->article_search_text);
        if (resources->dictionary_bar_visible)
            dictionary_bar_->show();
        else
            dictionary_bar_->hide();
    }
    maintained = maintained && !injected(5);
    if (maintained) {
        auto* focus = resources->focused_widget.data();
        if (focus == nullptr || focus == resources->old_group_selector ||
            (resources->old_article_tabs != nullptr &&
             resources->old_article_tabs->isAncestorOf(focus))) {
            focus = query_;
        }
        focus->setFocus(Qt::OtherFocusReason);
        group_selector_host_->updateGeometry();
        dictionary_bar_host_->updateGeometry();
        article_tabs_host_->updateGeometry();
        update();
    }
    maintained = maintained && !injected(6);
    if (maintained) {
        retired_suggestion_worker_ = std::move(suggestion_worker_owner_);
        suggestion_worker_owner_ = std::move(resources->suggestion_worker);
        retired_rendered_text_match_plan_controller_ =
            std::move(rendered_text_match_plan_controller_owner_);
        retired_facade_resources_ = std::move(active_facade_resources_);
        resources->ownership_staged = true;
    }
    setUpdatesEnabled(resources->old_updates_enabled);

    maintained =
        maintained && !injected(7) && !facade_final_validation_failure_ &&
        !facade_preparation_shutdown_ && facade_preparation_record_ == record &&
        record->owner.load(std::memory_order_acquire) == this &&
        record->generation == facade_preparation_generation_ &&
        record->epoch == presentation_mutation_epoch_ &&
        resources->binding_registry->CanPublish(*resources->binding_slot) &&
        resources->relay->CanPublishWithoutAllocation(this, record->generation,
                                                      record->epoch) &&
        group_selector_host_->currentWidget() == resources->group_selector &&
        dictionary_bar_host_->currentWidget() == resources->dictionary_bar &&
        article_tabs_host_->currentWidget() == resources->article_tabs;
    if (!maintained) {
        AbortMaintainedFacadeCommitInternal();
        record->state.store(WidgetsFacadePreparationRecord::State::kPrepared,
                            std::memory_order_release);
        return result;
    }

    maintained_facade_record_ = std::move(candidate.record_);
    auto* const maintained_record = maintained_facade_record_.get();
    maintained_record->state.store(
        WidgetsFacadePreparationRecord::State::kMaintained,
        std::memory_order_release);
    result.outcome = WidgetsCommitOutcome::kMaintainedAbortable;
    result.maintained.owner_ = this;
    result.maintained.record_ = maintained_record;
    result.maintained.generation_ = maintained_record->generation;
    return result;
}

void MainWindow::AbortMaintainedFacadeCommitInternal() noexcept {
    auto* record = maintained_facade_record_ != nullptr
                       ? maintained_facade_record_.get()
                       : facade_preparation_record_.get();
    auto* resources = record == nullptr ? nullptr : record->resources;
    if (resources != nullptr) {
        setUpdatesEnabled(false);
        if (resources->ownership_staged) {
            active_facade_resources_ = std::move(retired_facade_resources_);
            rendered_text_match_plan_controller_owner_ =
                std::move(retired_rendered_text_match_plan_controller_);
            resources->suggestion_worker = std::move(suggestion_worker_owner_);
            suggestion_worker_owner_ = std::move(retired_suggestion_worker_);
            suggestion_worker_ = suggestion_worker_owner_.get();
            rendered_text_match_plan_controller_ =
                rendered_text_match_plan_controller_owner_.get();
            resources->ownership_staged = false;
        }
        if (resources->article_switched)
            article_tabs_host_->ReverseMaintenanceSwitch();
        if (resources->dictionary_switched)
            dictionary_bar_host_->ReverseMaintenanceSwitch();
        if (resources->group_switched)
            group_selector_host_->ReverseMaintenanceSwitch();
        resources->article_switched = false;
        resources->dictionary_switched = false;
        resources->group_switched = false;
        for (const auto& state : resources->surface_states) {
            if (state.widget != nullptr) {
                state.widget->setEnabled(false);
                state.widget->setFocusPolicy(Qt::NoFocus);
                state.widget->setAttribute(Qt::WA_TransparentForMouseEvents,
                                           true);
            }
        }
        if (resources->old_focus != nullptr)
            resources->old_focus->setFocus(Qt::OtherFocusReason);
        setUpdatesEnabled(resources->old_updates_enabled);
    }
    widgets_maintenance_active_ = false;
    widgets_publication_decided_ = false;
    widgets_interaction_gate_closed_.store(false, std::memory_order_release);
}

void MainWindow::AbortMaintainedFacadeCommit(
    MaintainedWidgetsCommit&& maintained) noexcept {
    if (maintained.owner_ != this || QThread::currentThread() != thread() ||
        maintained.record_ != maintained_facade_record_.get() ||
        maintained.generation_ != facade_preparation_generation_) {
        std::terminate();
    }
    auto record = maintained_facade_record_;
    AbortMaintainedFacadeCommitInternal();
    record->state.store(WidgetsFacadePreparationRecord::State::kAborted,
                        std::memory_order_release);
    record->abandoned.store(true, std::memory_order_release);
    maintained.owner_ = nullptr;
    maintained.record_ = nullptr;
    maintained.generation_ = 0U;
    maintained_facade_record_.reset();
    ReclaimFacadeCandidate(true);
}

PublishedWidgetsCommit MainWindow::PublishMaintainedFacadeCommit(
    MaintainedWidgetsCommit&& maintained) noexcept {
    if (maintained.owner_ != this || QThread::currentThread() != thread() ||
        maintained.record_ != maintained_facade_record_.get() ||
        maintained.generation_ != facade_preparation_generation_ ||
        maintained.record_->state.load(std::memory_order_acquire) !=
            WidgetsFacadePreparationRecord::State::kMaintained) {
        std::terminate();
    }
    auto* const record = maintained.record_;
    auto* const resources = record->resources;
    widgets_publication_decided_ = true;

    // Irreversible publication boundary: fixed pointer, integer and atomic
    // stores only. Keep this sequence aligned with kPublicationOperations.
    resources->binding_registry->PublishPreparedUnchecked(
        resources->binding_slot_alias);
    facade_ = resources->facade_alias;
    group_selector_ = resources->group_selector_alias;
    article_tabs_ = resources->article_tabs_alias;
    article_view_ = resources->article_view_alias;
    article_page_ = resources->article_page_alias;
    published_full_text_search_dialog_ = resources->full_text_dialog_alias;
    suggestion_worker_ = resources->suggestion_worker_alias;
    rendered_text_match_plan_controller_ = resources->match_controller_alias;
    group_selector_host_->PublishMaintenanceSwitch();
    dictionary_bar_host_->PublishMaintenanceSwitch();
    article_tabs_host_->PublishMaintenanceSwitch();
    if (!resources->relay->PublishAndEnable(this, record->generation,
                                            record->epoch, record->generation,
                                            record->epoch)) {
        std::terminate();
    }
    presentation_mutation_epoch_ = record->epoch;
    widgets_interaction_gate_closed_.store(false, std::memory_order_release);
    record->state.store(WidgetsFacadePreparationRecord::State::kPublished,
                        std::memory_order_release);

    PublishedWidgetsCommit published;
    published.owner_ = this;
    published.record_ = record;
    published.generation_ = record->generation;
    maintained.owner_ = nullptr;
    maintained.record_ = nullptr;
    maintained.generation_ = 0U;
    return published;
}

WidgetsCommitOutcome
MainWindow::FinishPublishedFacadeCommitInternal() noexcept {
    auto record = maintained_facade_record_;
    if (record == nullptr || record->resources == nullptr)
        return WidgetsCommitOutcome::kPublishedWithCleanupFailure;
    record->state.store(WidgetsFacadePreparationRecord::State::kCleaning,
                        std::memory_order_release);
    auto* resources = record->resources;
    bool failed = widgets_cleanup_failure_injected_;
    try {
        SetPreferences(resources->preferences);
        full_text_search_dialog_ = published_full_text_search_dialog_;
        if (retired_suggestion_worker_ != nullptr)
            retired_suggestion_worker_->Stop();
        if (retired_rendered_text_match_plan_controller_ != nullptr)
            retired_rendered_text_match_plan_controller_->Stop();
        retired_suggestion_worker_.reset();
        retired_rendered_text_match_plan_controller_.reset();
        if (retired_facade_resources_ != nullptr) {
            retired_facade_resources_->published = false;
            retired_facade_resources_.reset();
        } else {
            group_selector_host_->DetachInactive(resources->old_group_selector);
            dictionary_bar_host_->DetachInactivePage(
                resources->old_dictionary_bar);
            article_tabs_host_->DetachInactive(resources->old_article_tabs);
            delete resources->old_group_selector;
            delete resources->old_dictionary_bar;
            delete resources->old_article_tabs;
        }
        pending_article_scroll_restorations_.clear();
        for (const auto& [tab_id, position] : resources->scroll_positions) {
            if (ArticleViewForTab(tab_id) != nullptr) {
                pending_article_scroll_restorations_.emplace(
                    tab_id, PendingArticleScrollRestoration{position, nullptr,
                                                            nullptr, 0U});
            }
        }
        RebuildArticleTabs();
        for (const auto& tab : resources->tabs.tabs) {
            if (tab.navigation.kind !=
                goldendict::core::TabNavigationKind::kEmpty) {
                continue;
            }
            auto* view = ArticleViewForTab(tab.id);
            PublishArticleHtml(
                tab.id, view,
                QStringLiteral("<!doctype html><html><body><h1>GoldenDict</h1>"
                               "<p>Choose a dictionary folder to "
                               "begin.</p></body></html>"));
        }
        resources->published = true;
        active_facade_resources_.reset(resources);
        record->resources = nullptr;
        rendered_text_match_plan_controller_owner_.reset();
        facade_binding_registry_->ReclaimRetired();
        if (facade_binding_registry_->NeedsReclaim() &&
            !facade_binding_reclaimer_->isActive()) {
            facade_binding_reclaimer_->start();
        }
    } catch (...) {
        failed = true;
    }
    record->ready.store(false, std::memory_order_release);
    record->state.store(WidgetsFacadePreparationRecord::State::kFinished,
                        std::memory_order_release);
    widgets_maintenance_active_ = false;
    widgets_publication_decided_ = false;
    facade_preparation_record_.reset();
    maintained_facade_record_.reset();
    if (facade_candidate_reclaimer_->isActive())
        facade_candidate_reclaimer_->stop();
    return failed ? WidgetsCommitOutcome::kPublishedWithCleanupFailure
                  : WidgetsCommitOutcome::kPublished;
}

WidgetsCommitOutcome MainWindow::FinishPublishedFacadeCommit(
    PublishedWidgetsCommit&& published) noexcept {
    if (published.owner_ != this || QThread::currentThread() != thread() ||
        published.record_ != maintained_facade_record_.get() ||
        published.generation_ != facade_preparation_generation_) {
        std::terminate();
    }
    published.owner_ = nullptr;
    published.record_ = nullptr;
    published.generation_ = 0U;
    return FinishPublishedFacadeCommitInternal();
}

void MainWindow::SetFacade(goldendict::core::DesktopFacade* facade) {
    AdvancePresentationMutationEpoch();
    if (facade_preparation_record_ != nullptr) {
        facade_preparation_record_->abandoned.store(true,
                                                    std::memory_order_release);
        ReclaimFacadeCandidate(true);
    }
    auto* const previous_facade = facade_;
    if (dictionary_browser_ != nullptr)
        dictionary_browser_->QuiesceBindingConsumer(false);
    if (full_text_search_dialog_ != nullptr)
        full_text_search_dialog_->QuiesceBindingConsumer();
    const auto restore_previous_binding_consumers = [this, previous_facade]() {
        if (full_text_search_dialog_ != nullptr && previous_facade != nullptr) {
            full_text_search_dialog_->SetBindingRegistry(
                facade_binding_registry_.get());
            full_text_search_dialog_->SetService(
                &previous_facade->GetDictionaryService());
        }
        if (dictionary_browser_ != nullptr) {
            dictionary_browser_->SetBindingRegistry(
                facade_binding_registry_.get());
            dictionary_browser_->SetFacade(previous_facade);
        }
    };
    if (facade_binding_registry_ != nullptr) {
        facade_binding_registry_->ReclaimRetired();
        if (facade == nullptr) {
            facade_binding_registry_->ClearPublished();
        } else {
            goldendict::widgets::WidgetsFacadeBindingDescriptor binding;
            binding.facade_owner =
                std::shared_ptr<goldendict::core::DesktopFacade>(
                    facade, [](goldendict::core::DesktopFacade*) {});
            binding.facade = facade;
            binding.service = &facade->GetDictionaryService();
            binding.catalog = binding.service->GetCatalog();
            for (std::size_t index = 0U; index < binding.catalog.size();
                 ++index)
                binding.catalog_index.emplace(binding.catalog[index].id, index);
            for (const auto consumer : {
                     goldendict::widgets::FacadeBindingConsumer::kSchemeHandler,
                     goldendict::widgets::FacadeBindingConsumer::
                         kDictionaryBrowser,
                     goldendict::widgets::FacadeBindingConsumer::
                         kFullTextDialog,
                     goldendict::widgets::FacadeBindingConsumer::kArticlePages,
                     goldendict::widgets::FacadeBindingConsumer::kArticleViews,
                     goldendict::widgets::FacadeBindingConsumer::
                         kRenderedMatches,
                     goldendict::widgets::FacadeBindingConsumer::kSuggestions,
                 }) {
                if (!binding.RegisterConsumer(consumer))
                    std::terminate();
            }
            const auto slot =
                facade_binding_registry_->Prepare(std::move(binding));
            if (!slot.has_value()) {
                restore_previous_binding_consumers();
                return;
            }
            if (!facade_binding_registry_->Publish(*slot)) {
                facade_binding_registry_->Abandon(*slot);
                restore_previous_binding_consumers();
                return;
            }
        }
        facade_binding_registry_->ReclaimRetired();
        if (facade_binding_registry_->NeedsReclaim() &&
            !facade_binding_reclaimer_->isActive())
            facade_binding_reclaimer_->start();
    }
    ++facade_binding_generation_;
    if (facade_binding_generation_ == 0U)
        facade_binding_generation_ = 1U;
    pending_pdf_request_.reset();
    pending_print_request_.reset();
    print_in_progress_ = false;
    const QSignalBlocker tab_signal_blocker(article_tabs_);
    InvalidateRenderedTextMatchPlan();
    if (rendered_text_match_plan_controller_ != nullptr)
        rendered_text_match_plan_controller_->SetFacade(facade);
    StopSuggestionWorker();
    ++suggestion_generation_;
    suggestions_.clear();
    RefreshSuggestions();
    completion_timer_->stop();
    for (auto& [id, request] : requests_) {
        static_cast<void>(id);
        request->Cancel();
    }
    requests_.clear();
    lookup_results_.clear();
    pending_article_search_handoffs_.clear();
    rendered_page_text_transports_.clear();
    article_navigation_generations_.clear();
    RefreshResultsNavigation();
    pending_article_scroll_restorations_.clear();
    for (int index = 0; index < article_tabs_->count(); ++index) {
        const auto tab_id = TabIdAt(index);
        auto* view = qobject_cast<ArticleView*>(article_tabs_->widget(index));
        if (tab_id != 0U && view != nullptr) {
            pending_article_scroll_restorations_[tab_id] =
                PendingArticleScrollRestoration{view->page()->scrollPosition(),
                                                nullptr, nullptr, 0U};
        }
    }
    while (article_tabs_->count() > 0) {
        QWidget* widget = article_tabs_->widget(0);
        article_tabs_->removeTab(0);
        delete widget;
    }
    article_view_ = nullptr;
    article_page_ = nullptr;
    if (full_text_search_dialog_ != nullptr)
        full_text_search_dialog_->SetService(nullptr);
    facade_ = facade;
    if (full_text_search_dialog_ != nullptr && facade_ != nullptr) {
        full_text_search_dialog_->SetBindingRegistry(
            facade_binding_registry_.get());
        full_text_search_dialog_->SetService(&facade_->GetDictionaryService());
    }
    full_text_search_action_->setEnabled(facade_ != nullptr);
    ReconcileMruTabIds(true);
    RefreshDictionaryBar();
    scheme_handler_->SetFacade(facade);
    if (dictionary_browser_ != nullptr) {
        dictionary_browser_->SetBindingRegistry(facade_binding_registry_.get());
        dictionary_browser_->SetFacade(facade);
    }
    const auto count = facade == nullptr
                           ? std::size_t{0}
                           : facade->GetDictionaryService().GetCatalog().size();
    status_->setText(
        tr("%1 dictionary loaded").arg(static_cast<qulonglong>(count)));
    suggestion_worker_owner_ = std::make_unique<SuggestionWorker>(
        [this](goldendict::core::ArticleTabId tab_id, std::uint64_t generation,
               goldendict::core::SuggestionResponse response) {
            QMetaObject::invokeMethod(
                this, [this, tab_id, generation,
                       response = std::move(response)]() mutable {
                    FinishSuggestionLookup(tab_id, generation,
                                           std::move(response));
                });
        });
    suggestion_worker_ = suggestion_worker_owner_.get();
    if (facade_ != nullptr)
        RebuildArticleTabs();
    UpdateFileActions();
}

void MainWindow::SetPreferences(
    const goldendict::core::ApplicationPreferences& preferences) {
    AdvancePresentationMutationEpoch();
#if defined(Q_OS_LINUX)
    if (preferences_.help_language != preferences.help_language &&
        help_window_ != nullptr) {
        delete help_window_.data();
        help_window_ = nullptr;
    }
#endif
    preferences_ = preferences;
    if (!preferences_.mru_tab_order)
        FinishMruTraversal();
    article_tabs_->setTabBarAutoHide(preferences_.hide_single_tab);
    for (int index = 0; index < article_tabs_->count(); ++index) {
        auto* view = qobject_cast<ArticleView*>(article_tabs_->widget(index));
        if (view != nullptr) {
            view->SetClickPreferences(preferences_.double_click_translates,
                                      preferences_.select_word_by_single_click);
            auto* page = qobject_cast<ArticlePage*>(view->page());
            if (page != nullptr) {
                page->SetOpenNewTabsInBackground(
                    preferences_.open_new_tabs_in_background);
            }
        }
    }
    for (int index = 0; index < article_tabs_->count(); ++index)
        RefreshDictionaryContext(TabIdAt(index));
}

void MainWindow::SetPreferencesApplyCallback(
    PreferencesApplyCallback apply_callback) {
    preferences_apply_callback_ = std::move(apply_callback);
}

void MainWindow::SetNetworkCacheDirectory(const QString& directory) {
    network_cache_directory_ = directory;
}

bool MainWindow::RestoreMainWindowGeometry(const std::string& geometry) {
    if (geometry.empty())
        return false;
    const QRect default_geometry = this->geometry();
    const Qt::WindowStates default_state = windowState();
    const QByteArray encoded(geometry.data(),
                             static_cast<qsizetype>(geometry.size()));
    if (restoreGeometry(encoded))
        return true;
    setWindowState(default_state);
    setGeometry(default_geometry);
    return false;
}

std::string MainWindow::CaptureMainWindowGeometry() const {
    const QByteArray geometry = saveGeometry();
    return {geometry.constData(), static_cast<std::size_t>(geometry.size())};
}

bool MainWindow::RestoreMainWindowState(const std::string& state) {
    if (state.empty() ||
        state.size() > static_cast<std::size_t>(kMaximumMainWindowStateBytes)) {
        return false;
    }
    const QScopedValueRollback restoring(restoring_main_window_state_, true);
    const QByteArray previous = saveState(kMainWindowStateVersion);
    const QByteArray encoded(state.data(),
                             static_cast<qsizetype>(state.size()));
    const auto place_missing_results_pane = [this]() {
        auto* results =
            findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
        auto* favorites =
            findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
        if (results == nullptr || favorites == nullptr)
            return;
        removeDockWidget(results);
        addDockWidget(Qt::RightDockWidgetArea, results);
        splitDockWidget(results, favorites, Qt::Vertical);
        results->show();
    };
    const auto place_missing_search_pane = [this]() {
        auto* search =
            findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
        if (search == nullptr)
            return;
        removeDockWidget(search);
        addDockWidget(Qt::LeftDockWidgetArea, search);
        search->show();
    };
    const auto place_missing_dictionary_bar = [this]() {
        if (dictionary_bar_ == nullptr)
            return;
        removeToolBar(dictionary_bar_);
        addToolBar(Qt::TopToolBarArea, dictionary_bar_);
        dictionary_bar_->show();
    };
    if (restoreState(encoded, kMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    if (restoreState(encoded, kPreviousMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    auto* nav_toolbar = findChild<QToolBar*>(QStringLiteral("navToolbar"));
    if (nav_toolbar != nullptr)
        addToolBar(Qt::TopToolBarArea, nav_toolbar);
    if (restoreState(encoded, kOlderMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    if (nav_toolbar != nullptr)
        addToolBar(Qt::TopToolBarArea, nav_toolbar);
    if (restoreState(encoded, kOldestMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    if (nav_toolbar != nullptr)
        addToolBar(Qt::TopToolBarArea, nav_toolbar);
    if (restoreState(encoded, kEarlierMainWindowStateVersion) &&
        HasUsableMainWindowLayout()) {
        return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    place_missing_dictionary_bar();
    place_missing_search_pane();
    place_missing_results_pane();
    auto* history =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    if (history != nullptr && favorites != nullptr) {
        if (nav_toolbar != nullptr)
            addToolBar(Qt::TopToolBarArea, nav_toolbar);
        history->setObjectName(QString::fromLatin1(kPreviousHistoryDockName));
        favorites->setObjectName(
            QString::fromLatin1(kPreviousFavoritesDockName));
        const bool restored_previous =
            restoreState(encoded, kEarliestMainWindowStateVersion);
        history->setObjectName(QString::fromLatin1(kHistoryPaneName));
        favorites->setObjectName(QString::fromLatin1(kFavoritesPaneName));
        const bool previous_usable = HasUsableMainWindowLayout();
        if (restored_previous && previous_usable)
            return true;
    }
    restoreState(previous, kMainWindowStateVersion);
    return false;
}

std::string MainWindow::CaptureMainWindowState() const {
    const QByteArray state = saveState(kMainWindowStateVersion);
    return {state.constData(), static_cast<std::size_t>(state.size())};
}

void MainWindow::RefreshResultsNavigation() {
    if (results_list_ == nullptr)
        return;
    const QSignalBlocker blocker(results_list_);
    results_list_->clear();
    if (article_tabs_ == nullptr)
        return;
    const auto id = TabIdAt(article_tabs_->currentIndex());
    const auto found = lookup_results_.find(id);
    if (found == lookup_results_.end())
        return;
    for (const auto& dictionary : found->second.rows) {
        auto* item = new QListWidgetItem(
            QString::fromStdString(dictionary.display_name), results_list_);
        item->setData(Qt::UserRole,
                      QString::fromStdString(dictionary.dictionary_id));
        item->setData(Qt::UserRole + 1, dictionary.first_result_index);
        item->setToolTip(item->text());
    }
    if (results_list_->count() > 0)
        results_list_->setCurrentRow(0);
}

void MainWindow::RefreshSuggestions() {
    if (suggestions_list_ == nullptr)
        return;
    const QSignalBlocker blocker(suggestions_list_);
    suggestions_list_->clear();
    if (article_tabs_ == nullptr)
        return;
    const auto found =
        suggestions_.find(TabIdAt(article_tabs_->currentIndex()));
    if (found == suggestions_.end())
        return;
    for (const auto& suggestion : found->second.rows) {
        auto* item = new QListWidgetItem(
            QString::fromStdString(suggestion.headword), suggestions_list_);
        item->setToolTip(item->text());
        item->setData(Qt::UserRole,
                      QString::fromStdString(suggestion.dictionary.id));
        item->setTextAlignment(Qt::AlignLeft);
    }
    if (suggestions_list_->count() > 0)
        suggestions_list_->scrollToTop();
}

void MainWindow::StartSuggestionLookup() {
    if (suggestions_list_ == nullptr || article_tabs_ == nullptr)
        return;
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    if (tab_id == 0U)
        return;
    const QString text = query_->text().trimmed();
    const std::uint64_t generation = ++suggestion_generation_;
    auto& presentation = suggestions_[tab_id];
    presentation.query = text;
    presentation.group_id = selected_group_id_;
    presentation.generation = generation;
    presentation.rows.clear();
    RefreshSuggestions();
    if (text.isEmpty() || facade_ == nullptr || suggestion_worker_ == nullptr) {
        if (text.isEmpty()) {
            if (suggestion_worker_ != nullptr)
                suggestion_worker_->Cancel();
            suggestions_.erase(tab_id);
        }
        return;
    }
    goldendict::core::SuggestionQuery query;
    query.text = text.toStdString();
    query.group_id = selected_group_id_;
    ApplyDictionaryFilter(&query);
    suggestion_worker_->Submit(&facade_->GetDictionaryService(),
                               std::move(query), tab_id, generation);
}

void MainWindow::FinishSuggestionLookup(
    goldendict::core::ArticleTabId tab_id, std::uint64_t generation,
    goldendict::core::SuggestionResponse response) {
    const auto found = suggestions_.find(tab_id);
    if (found == suggestions_.end() || found->second.generation != generation) {
        return;
    }
    found->second.rows.clear();
    if (response.errors.empty() || response.partial)
        found->second.rows = std::move(response.suggestions);
    const bool active = TabIdAt(article_tabs_->currentIndex()) == tab_id;
    if (active) {
        RefreshSuggestions();
        if (!response.errors.empty() && !response.partial) {
            status_->setText(
                preferences_.limit_input_phrase_length &&
                        response.errors.front().message ==
                            "Input phrase exceeds the configured symbol limit"
                    ? QStringLiteral(
                          "Input phrase exceeds the configured %1-symbol limit")
                          .arg(preferences_.input_phrase_length_limit)
                    : QStringLiteral("Suggestion lookup failed"));
        }
    }
}

void MainWindow::ActivateSuggestion() {
    if (suggestions_list_ == nullptr ||
        suggestions_list_->currentItem() == nullptr)
        return;
    const QString word = suggestions_list_->currentItem()->text();
    {
        const QSignalBlocker blocker(query_);
        query_->setText(word);
    }
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    suggestions_.erase(tab_id);
    ++suggestion_generation_;
    if (suggestion_worker_ != nullptr)
        suggestion_worker_->Cancel();
    RefreshSuggestions();
    StartLookup();
    if (article_view_ != nullptr)
        article_view_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::StopSuggestionWorker() {
    if (suggestion_worker_ != nullptr) {
        suggestion_worker_->Stop();
        suggestion_worker_owner_.reset();
        suggestion_worker_ = nullptr;
    }
}

void MainWindow::NavigateToSelectedResult() {
    if (article_view_ == nullptr || results_list_ == nullptr ||
        results_list_->currentRow() < 0) {
        return;
    }
    NavigateToArticleResult(
        article_view_,
        results_list_->currentItem()->data(Qt::UserRole + 1).toInt());
}

void MainWindow::NavigateToArticleResult(ArticleView* view, int result_index) {
    if (view == nullptr || result_index < 0)
        return;
    view->page()->runJavaScript(
        QStringLiteral(
            "const entries=document.querySelectorAll('.gd-dictionary-result');"
            "if(entries[%1]) entries[%1].scrollIntoView(true);")
            .arg(result_index));
    view->setFocus(Qt::OtherFocusReason);
}

void MainWindow::RefreshDictionaryContext(
    goldendict::core::ArticleTabId tab_id) {
    auto* view = ArticleViewForTab(tab_id);
    if (view == nullptr)
        return;
    const auto found = lookup_results_.find(tab_id);
    QList<ArticleDictionaryContextEntry> entries;
    bool overflow = false;
    std::uint64_t generation = 0U;
    if (found != lookup_results_.end()) {
        generation = found->second.generation;
        const auto limit = static_cast<std::size_t>(
            preferences_.maximum_dictionary_references);
        overflow = found->second.rows.size() > limit;
        const auto count = std::min(found->second.rows.size(), limit);
        entries.reserve(static_cast<qsizetype>(count));
        for (std::size_t index = 0; index < count; ++index) {
            const auto& row = found->second.rows[index];
            entries.push_back({QString::fromStdString(row.dictionary_id),
                               QString::fromStdString(row.display_name),
                               row.first_result_index});
        }
    }
    view->SetDictionaryContextEntries(std::move(entries), overflow, generation);
}

void MainWindow::StoreLookupResults(
    goldendict::core::ArticleTabId tab_id,
    const goldendict::core::LookupResponse& response) {
    auto& results = lookup_results_[tab_id].rows;
    results.clear();
    results.reserve(response.entries.size());
    for (std::size_t index = 0; index < response.entries.size(); ++index) {
        const auto& dictionary = response.entries[index].dictionary;
        const bool represented =
            std::any_of(results.begin(), results.end(), [&](const auto& row) {
                return row.dictionary_id == dictionary.id;
            });
        if (!represented) {
            results.push_back(
                {dictionary.id,
                 dictionary.name.empty() ? dictionary.id : dictionary.name,
                 static_cast<int>(index)});
        }
    }
    RefreshDictionaryContext(tab_id);
}

void MainWindow::ShowDictionaryResultsPane(
    goldendict::core::ArticleTabId tab_id, ArticleView* view,
    std::uint64_t generation) {
    const auto found = lookup_results_.find(tab_id);
    if (found == lookup_results_.end() ||
        found->second.generation != generation ||
        found->second.rows.size() <=
            static_cast<std::size_t>(
                preferences_.maximum_dictionary_references) ||
        ArticleViewForTab(tab_id) != view || facade_ == nullptr) {
        return;
    }
    if (TabIdAt(article_tabs_->currentIndex()) != tab_id) {
        if (!facade_->ActivateArticleTab(tab_id))
            return;
        SyncArticleTabs();
        emit ArticleTabSessionMutated();
    } else {
        RefreshResultsNavigation();
    }
    auto* results_dock =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    if (results_dock != nullptr) {
        results_dock->show();
        results_dock->raise();
    }
}

void MainWindow::ApplyDefaultPaneLayout() {
    auto* history =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    auto* favorites =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    auto* results =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    auto* search =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    if (history == nullptr || favorites == nullptr || results == nullptr ||
        search == nullptr)
        return;
    removeDockWidget(history);
    removeDockWidget(favorites);
    removeDockWidget(results);
    removeDockWidget(search);
    addDockWidget(Qt::LeftDockWidgetArea, search);
    addDockWidget(Qt::RightDockWidgetArea, results);
    addDockWidget(Qt::RightDockWidgetArea, favorites);
    addDockWidget(Qt::RightDockWidgetArea, history);
    splitDockWidget(results, favorites, Qt::Vertical);
    splitDockWidget(favorites, history, Qt::Vertical);
    results->show();
    favorites->show();
    history->show();
    search->show();
}

bool MainWindow::HasUsableMainWindowLayout() const {
    const auto* history =
        findChild<QDockWidget*>(QString::fromLatin1(kHistoryPaneName));
    const auto* favorites =
        findChild<QDockWidget*>(QString::fromLatin1(kFavoritesPaneName));
    const auto* results =
        findChild<QDockWidget*>(QString::fromLatin1(kResultsPaneName));
    const auto* search =
        findChild<QDockWidget*>(QString::fromLatin1(kSearchPaneName));
    const auto* article_toolbar =
        findChild<QToolBar*>(QStringLiteral("articleToolbar"));
    const auto* nav_toolbar =
        findChild<QToolBar*>(QStringLiteral("navToolbar"));
    if (history == nullptr || favorites == nullptr || results == nullptr ||
        search == nullptr || article_toolbar == nullptr ||
        nav_toolbar == nullptr || centralWidget() == nullptr ||
        article_tabs_ == nullptr || article_tabs_->isHidden()) {
        return false;
    }
    const auto intersects_screen = [](const QWidget* widget) {
        if (!widget->isWindow())
            return true;
        const QRect geometry = widget->frameGeometry();
        for (const QScreen* screen : QApplication::screens()) {
            const QRect visible =
                geometry.intersected(screen->availableGeometry());
            if (visible.width() >= 32 && visible.height() >= 32)
                return true;
        }
        return false;
    };
    return intersects_screen(history) && intersects_screen(favorites) &&
           intersects_screen(results) && intersects_screen(search) &&
           intersects_screen(article_toolbar) && intersects_screen(nav_toolbar);
}

void MainWindow::ShowDictionaryBrowser() {
    if (dictionary_browser_ == nullptr) {
        dictionary_browser_ = new DictionaryBrowser(this);
        dictionary_browser_->SetBindingRegistry(facade_binding_registry_.get());
        dictionary_browser_->SetFacade(facade_);
        connect(dictionary_browser_, &DictionaryBrowser::HeadwordSelected, this,
                [this](const QString& word) {
                    query_->setText(word);
                    StartLookup();
                });
#if defined(Q_OS_LINUX)
        connect(dictionary_browser_, &DictionaryBrowser::HelpRequested, this,
                [this]() {
                    ShowHelp(goldendict::app::HelpIntent::kDictionaryHeadwords);
                });
#endif
    }
    dictionary_browser_->show();
    dictionary_browser_->raise();
    dictionary_browser_->activateWindow();
}

void MainWindow::SetHistoryWords(const QStringList& words) {
    history_items_.clear();
    history_items_.reserve(static_cast<std::size_t>(words.size()));
    for (const auto& word : words) {
        history_items_.push_back({word, 0U});
    }
    RefreshHistoryList();
    UpdateHistoryActions();
}

void MainWindow::SetHistoryItems(const std::vector<HistoryViewItem>& items) {
    history_items_ = items;
    RefreshHistoryList();
    UpdateHistoryActions();
}

void MainWindow::RefreshHistoryList() {
    history_list_->clear();
    const QString filter = history_filter_->text().trimmed();
    for (const auto& entry : history_items_) {
        if (filter.isEmpty() ||
            entry.word.contains(filter, Qt::CaseInsensitive)) {
            auto* item = new QListWidgetItem(entry.word, history_list_);
            item->setData(Qt::UserRole,
                          QVariant::fromValue<quint32>(entry.group_id));
        }
    }
}

void MainWindow::UpdateHistoryActions() {
    const bool idle = !history_command_busy_;
    const bool has_history = !history_items_.empty();
    import_history_action_->setEnabled(idle);
    export_history_action_->setEnabled(idle && has_history);
    clear_history_action_->setEnabled(idle && has_history);
    import_history_button_->setEnabled(import_history_action_->isEnabled());
    export_history_button_->setEnabled(export_history_action_->isEnabled());
    clear_history_button_->setEnabled(clear_history_action_->isEnabled());
}

void MainWindow::ExportHistory() {
    if (history_command_busy_) {
        return;
    }
    const QString path =
        history_export_path_provider_
            ? history_export_path_provider_()
            : QFileDialog::getSaveFileName(
                  this, QStringLiteral("Export history to file"), QString(),
                  QStringLiteral("Text files (*.txt);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    history_command_busy_ = true;
    UpdateHistoryActions();
    const QString error = history_export_callback_
                              ? history_export_callback_(path)
                              : QStringLiteral("History export is unavailable");
    history_command_busy_ = false;
    UpdateHistoryActions();
    status_->setText(error.isEmpty() ? QStringLiteral("History export complete")
                                     : QStringLiteral("History export failed"));
}

void MainWindow::ImportHistory() {
    if (history_command_busy_) {
        return;
    }
    const QString path =
        history_import_path_provider_
            ? history_import_path_provider_()
            : QFileDialog::getOpenFileName(
                  this, QStringLiteral("Import history from file"), QString(),
                  QStringLiteral("Text files (*.txt);;All files (*.*)"));
    if (!path.isEmpty()) {
        history_command_busy_ = true;
        UpdateHistoryActions();
        emit ImportHistoryRequested(path, selected_group_id_);
        history_command_busy_ = false;
        UpdateHistoryActions();
    }
}

void MainWindow::ClearHistory() {
    if (history_command_busy_ || history_items_.empty()) {
        return;
    }
    history_command_busy_ = true;
    UpdateHistoryActions();
    emit ClearHistoryRequested();
    history_command_busy_ = false;
    UpdateHistoryActions();
}

void MainWindow::CreateFavoriteFolder() {
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("New favorite folder"),
                              QStringLiteral("Folder name:"), QLineEdit::Normal,
                              QString(), &accepted)
            .trimmed();
    if (accepted && !name.isEmpty()) {
        emit AddFavoriteFolderRequested(name, SelectedFavoriteFolderPath());
    }
}

void MainWindow::RenameFavorite() {
    const auto* item = favorites_tree_->currentItem();
    if (item == nullptr) {
        return;
    }
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("Rename favorite"),
                              QStringLiteral("Name:"), QLineEdit::Normal,
                              item->text(0), &accepted)
            .trimmed();
    if (accepted && !name.isEmpty() && name != item->text(0)) {
        emit RenameFavoriteRequested(
            item->data(0, Qt::UserRole + 1).value<QList<int>>(), name);
    }
}

void MainWindow::ImportFavorites() {
    if (favorites_command_busy_) {
        return;
    }
    const QString path =
        favorites_import_path_provider_
            ? favorites_import_path_provider_()
            : QFileDialog::getOpenFileName(
                  this, QStringLiteral("Import favorites from file"), QString(),
                  QStringLiteral(
                      "GoldenDict favorites (*.xml);;All files (*.*)"));
    if (!path.isEmpty()) {
        favorites_command_busy_ = true;
        UpdateFavoritesActions();
        emit ImportFavoritesRequested(path);
        favorites_command_busy_ = false;
        UpdateFavoritesActions();
    }
}

void MainWindow::ExportFavorites() {
    if (favorites_command_busy_ || favorites_tree_->topLevelItemCount() == 0) {
        return;
    }
    const QString path =
        favorites_export_path_provider_
            ? favorites_export_path_provider_()
            : QFileDialog::getSaveFileName(
                  this, QStringLiteral("Export favorites to file"), QString(),
                  QStringLiteral(
                      "GoldenDict favorites (*.xml);;All files (*.*)"));
    if (!path.isEmpty()) {
        favorites_command_busy_ = true;
        UpdateFavoritesActions();
        emit ExportFavoritesRequested(path);
        favorites_command_busy_ = false;
        UpdateFavoritesActions();
    }
}

void MainWindow::UpdateFavoritesActions() {
    const bool idle = !favorites_command_busy_;
    const bool selected = favorites_tree_->currentItem() != nullptr;
    const bool nested =
        selected && favorites_tree_->currentItem()->parent() != nullptr;
    add_favorite_action_->setEnabled(idle &&
                                     !query_->text().trimmed().isEmpty());
    add_favorite_folder_action_->setEnabled(idle);
    rename_favorite_action_->setEnabled(idle && selected);
    move_favorite_up_action_->setEnabled(idle && selected);
    move_favorite_down_action_->setEnabled(idle && selected);
    move_favorite_to_root_action_->setEnabled(idle && nested);
    import_favorites_action_->setEnabled(idle);
    export_favorites_action_->setEnabled(
        idle && favorites_tree_->topLevelItemCount() > 0);
    remove_favorite_action_->setEnabled(idle && selected);
}

QList<int> MainWindow::SelectedFavoriteFolderPath() const {
    const auto* item = favorites_tree_->currentItem();
    if (item == nullptr) {
        return {};
    }
    if (!item->data(0, Qt::UserRole).toBool()) {
        item = item->parent();
    }
    return item == nullptr
               ? QList<int>{}
               : item->data(0, Qt::UserRole + 1).value<QList<int>>();
}

bool MainWindow::ConfirmFavoriteRemoval() {
    if (!preferences_.confirm_favorites_deletion) {
        return true;
    }
    if (favorite_removal_confirmation_) {
        return favorite_removal_confirmation_();
    }
    return QMessageBox::warning(
               this, QStringLiteral("GoldenDict"),
               QStringLiteral("All selected items will be deleted. Continue?"),
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) == QMessageBox::Yes;
}

QList<QList<int>> MainWindow::ExpandedFavoriteFolderPaths() const {
    QList<QList<int>> paths;
    const auto collect = [&](const auto& self,
                             const QTreeWidgetItem* item) -> void {
        if (item->data(0, Qt::UserRole).toBool() && item->isExpanded()) {
            paths.push_back(
                item->data(0, Qt::UserRole + 1).value<QList<int>>());
        }
        for (int index = 0; index < item->childCount(); ++index) {
            self(self, item->child(index));
        }
    };
    for (int index = 0; index < favorites_tree_->topLevelItemCount(); ++index) {
        collect(collect, favorites_tree_->topLevelItem(index));
    }
    return paths;
}

void MainWindow::SetFavoriteItems(const std::vector<FavoriteViewItem>& items,
                                  const QList<int>& current_path) {
    const bool restore_focus = favorites_tree_->hasFocus();
    QTreeWidgetItem* restored_current = nullptr;
    const auto append = [&](const auto& self, QTreeWidgetItem* parent,
                            const FavoriteViewItem& item) -> void {
        auto* tree_item = parent == nullptr
                              ? new QTreeWidgetItem(favorites_tree_)
                              : new QTreeWidgetItem(parent);
        tree_item->setText(0, item.text);
        tree_item->setData(0, Qt::UserRole, item.folder);
        tree_item->setData(0, Qt::UserRole + 1, QVariant::fromValue(item.path));
        if (item.path == current_path) {
            restored_current = tree_item;
        }
        for (const auto& child : item.children) {
            self(self, tree_item, child);
        }
        tree_item->setExpanded(item.expanded);
    };
    favorites_tree_->clear();
    for (const auto& item : items) {
        append(append, nullptr, item);
    }
    if (restored_current != nullptr) {
        favorites_tree_->setCurrentItem(restored_current);
        restored_current->setSelected(true);
        favorites_tree_->scrollToItem(restored_current);
    }
    if (restore_focus) {
        favorites_tree_->setFocus();
    }
    UpdateFavoritesActions();
}

void MainWindow::RunWebEngineSmokeCheck(std::function<void(bool)> completion) {
    connect(
        article_view_, &ArticleView::loadFinished, this,
        [this, completion = std::move(completion)](bool loaded) mutable {
            if (!loaded) {
                completion(false);
                return;
            }
            article_view_->page()->toPlainText(
                [completion = std::move(completion)](const QString& text) {
                    completion(
                        text.contains(QStringLiteral("WebEngine smoke ready")));
                });
        },
        Qt::SingleShotConnection);
    article_view_->setHtml(
        QStringLiteral("<!doctype html><html><body><p>WebEngine smoke ready</p>"
                       "</body></html>"));
}

void MainWindow::RunSourceDirectoriesSmokeCheck(
    std::function<void(bool)> completion) {
    const std::vector<std::string> paths = {"/one", "/two", "/three"};
    const std::vector<goldendict::core::SoundDirectoryConfiguration> sounds = {
        {"/sound-one", "One"}, {"/sound-two", "Two"}};
    SourceDirectoriesDialog dialog(paths, sounds, this);
    auto* path_list =
        dialog.findChild<QListWidget*>(QStringLiteral("dictionaryPathList"));
    auto* path_up =
        dialog.findChild<QPushButton*>(QStringLiteral("moveDictionaryPathUp"));
    auto* path_remove =
        dialog.findChild<QPushButton*>(QStringLiteral("removeDictionaryPath"));
    auto* sound_list =
        dialog.findChild<QTreeWidget*>(QStringLiteral("soundDirectoryList"));
    auto* sound_up =
        dialog.findChild<QPushButton*>(QStringLiteral("moveSoundDirectoryUp"));
    bool passed = path_list != nullptr && path_up != nullptr &&
                  path_remove != nullptr && sound_list != nullptr &&
                  sound_up != nullptr;
    if (passed) {
        path_list->setCurrentRow(2);
        path_up->click();
        path_list->setCurrentRow(0);
        path_remove->click();
        sound_list->setCurrentItem(sound_list->topLevelItem(1));
        sound_up->click();
        sound_list->topLevelItem(0)->setText(1, QStringLiteral("Edited"));
        passed =
            dialog.DictionaryPaths() ==
                (std::vector<std::string>{"/three", "/two"}) &&
            dialog.SoundDirectories() ==
                (std::vector<goldendict::core::SoundDirectoryConfiguration>{
                    {"/sound-two", "Edited"}, {"/sound-one", "One"}});
    }
    SourceDirectoriesDialog cancelled(paths, sounds, this);
    cancelled.reject();
    passed = passed && cancelled.result() == QDialog::Rejected &&
             cancelled.DictionaryPaths() == paths &&
             cancelled.SoundDirectories() == sounds;
    const std::vector<goldendict::core::MediaWikiSourceConfiguration> wikis = {
        {"wiki.one", "One", true, "https://one.example/w"},
        {"wiki.two", "Two", false, "https://two.example/w"}};
    const std::vector<goldendict::core::WebsiteSourceConfiguration> websites = {
        {"web.one", "Website", true, "https://website.example/?q=%GDWORD%"}};
    const std::vector<goldendict::core::ForvoSourceConfiguration> forvo = {
        {"forvo.one",
         "Forvo",
         false,
         "https://apifree.forvo.com",
         {"en", "ru"}}};
    const std::vector<goldendict::core::DictServerSourceConfiguration> dicts = {
        {"dict.one", "DICT", true, "dict.example", 2628U, "*", "prefix"}};
    const std::vector<goldendict::core::ExternalProgramSourceConfiguration>
        programs = {{"program.one",
                     "Plain",
                     false,
                     goldendict::core::ExternalProgramOutputKind::kPlainText,
                     "/bin/echo",
                     {},
                     ""},
                    {"program.two",
                     "Prefix",
                     true,
                     goldendict::core::ExternalProgramOutputKind::kPrefixMatch,
                     "/usr/bin/printf",
                     {"%GDWORD%"},
                     "/tmp"}};
    bool reject_once = true;
    bool callback_received = false;
    const SourceDirectoriesDialog::ForvoCredentialMap forvo_credentials = {
        {"forvo.one", "session-secret"}};
    SourceDirectoriesDialog online(
        paths, sounds, wikis, websites, forvo, forvo_credentials, dicts,
        programs,
        [&](const auto&, const auto&, const auto& edited_wikis,
            const auto& edited_websites, const auto& edited_forvo,
            const auto& edited_forvo_credentials, const auto& edited_dicts,
            const auto& edited_programs) {
            callback_received =
                edited_wikis.front().id == "wiki.two" &&
                edited_wikis.front().enabled && edited_websites == websites &&
                edited_forvo.front().language_codes ==
                    (std::vector<std::string>{"ru", "en"}) &&
                edited_forvo_credentials ==
                    (SourceDirectoriesDialog::ForvoCredentialMap{
                        {"forvo.one", "replacement-secret"}}) &&
                edited_dicts == dicts &&
                edited_programs.front().id == "program.two" &&
                edited_programs.front().enabled &&
                edited_programs.front().output_kind ==
                    goldendict::core::ExternalProgramOutputKind::kHtml &&
                edited_programs.front().executable == "/usr/bin/printf" &&
                edited_programs.front().working_directory.empty() &&
                edited_programs.front().argument_templates ==
                    (std::vector<std::string>{"", "%GDWORD%"}) &&
                edited_programs[1].id == "program.one" &&
                edited_programs[1].argument_templates.empty();
            if (reject_once) {
                reject_once = false;
                return QStringLiteral("forced apply failure");
            }
            return QString{};
        },
        this);
    auto* wiki_list =
        online.findChild<QTreeWidget*>(QStringLiteral("mediaWikiList"));
    auto* wiki_up =
        online.findChild<QPushButton*>(QStringLiteral("mediaWikiUp"));
    auto* forvo_list =
        online.findChild<QTreeWidget*>(QStringLiteral("forvoList"));
    auto* forvo_credential =
        online.findChild<QLineEdit*>(QStringLiteral("forvoSessionCredential"));
    auto* buttons = online.findChild<QDialogButtonBox*>();
    auto* program_list =
        online.findChild<QTreeWidget*>(QStringLiteral("externalProgramList"));
    auto* program_up =
        online.findChild<QPushButton*>(QStringLiteral("externalProgramUp"));
    auto* result_kind = online.findChild<QComboBox*>(
        QStringLiteral("externalProgramResultKind"));
    auto* executable = online.findChild<QLineEdit*>(
        QStringLiteral("externalProgramExecutable"));
    auto* working_directory = online.findChild<QLineEdit*>(
        QStringLiteral("externalProgramWorkingDirectory"));
    auto* clear_working = online.findChild<QPushButton*>(
        QStringLiteral("externalProgramClearWorkingDirectory"));
    auto* arguments = online.findChild<QListWidget*>(
        QStringLiteral("externalProgramArgumentList"));
    auto* argument_add =
        online.findChild<QPushButton*>(QStringLiteral("externalArgumentAdd"));
    auto* argument_up =
        online.findChild<QPushButton*>(QStringLiteral("externalArgumentUp"));
    passed = passed && wiki_list != nullptr && wiki_up != nullptr &&
             forvo_list != nullptr && forvo_credential != nullptr &&
             forvo_credential->echoMode() == QLineEdit::Password &&
             forvo_credential->text() == QStringLiteral("session-secret") &&
             buttons != nullptr && program_list != nullptr &&
             program_up != nullptr && result_kind != nullptr &&
             executable != nullptr && working_directory != nullptr &&
             clear_working != nullptr && arguments != nullptr &&
             argument_add != nullptr && argument_up != nullptr;
    if (passed) {
        auto* website_list =
            online.findChild<QTreeWidget*>(QStringLiteral("websiteList"));
        website_list->topLevelItem(0)->setText(2, QStringLiteral("invalid"));
        buttons->button(QDialogButtonBox::Apply)->click();
        auto* error =
            online.findChild<QLabel*>(QStringLiteral("sourceValidationError"));
        passed = !callback_received && online.result() != QDialog::Accepted &&
                 error != nullptr && !error->isHidden();
        website_list->topLevelItem(0)->setText(
            2, QStringLiteral("https://website.example/?q=%GDWORD%"));
        wiki_list->setCurrentItem(wiki_list->topLevelItem(1));
        wiki_up->click();
        wiki_list->topLevelItem(0)->setCheckState(0, Qt::Checked);
        forvo_list->topLevelItem(0)->setText(3, QStringLiteral("ru,en"));
        forvo_credential->setText(QStringLiteral("replacement-secret"));
        program_list->setCurrentItem(program_list->topLevelItem(1));
        program_up->click();
        result_kind->setCurrentIndex(static_cast<int>(
            goldendict::core::ExternalProgramOutputKind::kHtml));
        clear_working->click();
        argument_add->click();
        arguments->currentItem()->setText(QString{});
        argument_up->click();
        executable->setText(QStringLiteral("relative"));
        buttons->button(QDialogButtonBox::Apply)->click();
        passed = passed && !callback_received &&
                 online.result() != QDialog::Accepted && error != nullptr &&
                 !error->isHidden();
        executable->setText(QStringLiteral("/usr/bin/printf"));
        buttons->button(QDialogButtonBox::Apply)->click();
        passed = passed && callback_received &&
                 online.result() != QDialog::Accepted && error != nullptr &&
                 !error->isHidden();
        buttons->button(QDialogButtonBox::Apply)->click();
        passed = passed && online.result() == QDialog::Accepted;
    }
    bool received_empty = false;
    SourceDirectoriesDialog empty_online(
        paths, sounds, {}, {}, {}, {}, {}, {},
        [&](const auto&, const auto&, const auto& empty_wikis,
            const auto& empty_websites, const auto& empty_forvo,
            const auto& empty_forvo_credentials, const auto& empty_dicts,
            const auto& empty_programs) {
            received_empty = empty_wikis.empty() && empty_websites.empty() &&
                             empty_forvo.empty() &&
                             empty_forvo_credentials.empty() &&
                             empty_dicts.empty() && empty_programs.empty();
            return QString{};
        },
        this);
    empty_online.findChild<QDialogButtonBox*>()
        ->button(QDialogButtonBox::Apply)
        ->click();
    passed =
        passed && received_empty && empty_online.result() == QDialog::Accepted;
    completion(passed);
}

void MainWindow::EditSourceDirectories() {
    if (source_configuration_busy_)
        return;
    source_configuration_busy_ = true;
    dictionaries_action_->setEnabled(false);
    try {
        SourceDirectoriesDialog dialog(
            dictionary_paths_, sound_directories_, mediawiki_sources_,
            website_sources_, forvo_sources_, forvo_credentials_,
            dict_server_sources_, external_program_sources_,
            source_apply_callback_, this);
#if defined(Q_OS_LINUX)
        connect(&dialog, &SourceDirectoriesDialog::HelpRequested, this,
                [this]() {
                    ShowHelp(goldendict::app::HelpIntent::kManageDictionaries);
                });
#endif
        if (source_dialog_executor_)
            source_dialog_executor_(dialog);
        else
            dialog.exec();
    } catch (...) {
        source_configuration_busy_ = false;
        dictionaries_action_->setEnabled(true);
        throw;
    }
    source_configuration_busy_ = false;
    dictionaries_action_->setEnabled(true);
}

void MainWindow::EditPreferences() {
    if (preferences_busy_)
        return;
    preferences_busy_ = true;
    preferences_action_->setEnabled(false);
    try {
        PreferencesDialog dialog(
            preferences_,
            preferences_apply_callback_
                ? preferences_apply_callback_
                : [](const auto&) {
                      return QCoreApplication::translate(
                          "MainWindow",
                          "Preferences cannot be applied in this context");
                  },
            network_cache_directory_,
            this);
#if defined(Q_OS_LINUX)
        connect(&dialog, &PreferencesDialog::HelpRequested, this, [this]() {
            ShowHelp(goldendict::app::HelpIntent::kPreferences);
        });
#endif
        if (preferences_dialog_executor_)
            preferences_dialog_executor_(dialog);
        else
            dialog.exec();
    } catch (...) {
        preferences_busy_ = false;
        preferences_action_->setEnabled(true);
        throw;
    }
    preferences_busy_ = false;
    preferences_action_->setEnabled(true);
}

void MainWindow::StartLookup() {
    StartLookupInTab(goldendict::core::TabOpenPolicy::kCurrentTab,
                     goldendict::core::TabActivationPolicy::kActivate);
}

void MainWindow::ActivateFromSingleInstanceLookup() {
    if (!isVisible()) {
        show();
    }
    if (isMinimized()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    raise();
    activateWindow();
}

void MainWindow::SubmitInitialLookup(const QString& word) {
    if (facade_ == nullptr || word.isEmpty()) {
        return;
    }
    query_->setText(word);
    StartLookup();
}

void MainWindow::StartLookupInTab(
    goldendict::core::TabOpenPolicy open_policy,
    goldendict::core::TabActivationPolicy activation,
    const QString& internal_url, std::optional<std::uint32_t> group_id) {
    if (facade_ == nullptr || query_->text().trimmed().isEmpty()) {
        return;
    }
    const QString word = query_->text().trimmed();
    goldendict::core::TabNavigationState navigation;
    navigation.kind = internal_url.isEmpty()
                          ? goldendict::core::TabNavigationKind::kLookup
                          : goldendict::core::TabNavigationKind::kInternalLink;
    navigation.query = word.toStdString();
    navigation.group_id = group_id.value_or(selected_group_id_);
    navigation.title = navigation.query;
    navigation.internal_url = internal_url.toStdString();
    const auto tab_result = facade_->OpenArticleTab(
        navigation, open_policy, activation, NewTabPlacementPolicy());
    if (!tab_result) {
        status_->setText(
            tab_result.error == goldendict::core::TabOperationError::
                                    kInvalidNavigation &&
                    preferences_.limit_input_phrase_length
                ? QStringLiteral(
                      "Input phrase exceeds the configured %1-symbol limit")
                      .arg(preferences_.input_phrase_length_limit)
                : QStringLiteral("Unable to update article state"));
        return;
    }
    if (group_id.has_value())
        SelectGroup(*group_id);
    SyncArticleTabs();
    emit ArticleTabSessionMutated();
    StartNavigationLookup(tab_result.tab_id, navigation, true);
}

void MainWindow::StartNavigationLookup(
    goldendict::core::ArticleTabId tab_id,
    const goldendict::core::TabNavigationState& navigation,
    bool record_history) {
    if (facade_ == nullptr || navigation.query.empty())
        return;
    DeferPendingArticleScrollRestoration(tab_id, ArticleViewForTab(tab_id));
    InvalidateRenderedTextMatchPlan(tab_id);
    pending_article_search_handoffs_.erase(tab_id);
    rendered_page_text_transports_.erase(tab_id);
    suggestions_.erase(tab_id);
    ++suggestion_generation_;
    if (suggestion_worker_ != nullptr)
        suggestion_worker_->Cancel();
    if (TabIdAt(article_tabs_->currentIndex()) == tab_id)
        RefreshSuggestions();
    if (auto existing = requests_.find(tab_id); existing != requests_.end()) {
        existing->second->Cancel();
        requests_.erase(existing);
    }
    auto& lookup_presentation = lookup_results_[tab_id];
    ++lookup_presentation.generation;
    lookup_presentation.rows.clear();
    RefreshDictionaryContext(tab_id);
    if (TabIdAt(article_tabs_->currentIndex()) == tab_id)
        RefreshResultsNavigation();
    goldendict::core::LookupQuery query;
    query.text = navigation.query;
    query.group_id = navigation.group_id;
    ApplyNavigationDictionaryFilter(&query, navigation);
    if (record_history) {
        emit LookupSubmitted(QString::fromStdString(navigation.query),
                             navigation.group_id);
    }
    requests_[tab_id] =
        facade_->GetDictionaryService().StartLookup(std::move(query));
    if (TabIdAt(article_tabs_->currentIndex()) == tab_id) {
        status_->setText(QStringLiteral("Looking up..."));
        lookup_button_->setEnabled(false);
    }
    completion_timer_->start();
}

void MainWindow::FinishLookup() {
    std::vector<goldendict::core::ArticleTabId> finished;
    for (const auto& [id, request] : requests_) {
        if (request->IsFinished())
            finished.push_back(id);
    }
    for (const auto id : finished) {
        auto request = std::move(requests_.at(id));
        requests_.erase(id);
        auto* view = ArticleViewForTab(id);
        if (view == nullptr) {
            pending_article_search_handoffs_.erase(id);
            continue;
        }
        const bool active = TabIdAt(article_tabs_->currentIndex()) == id;
        QString navigation_title;
        const auto tabs_state = facade_->GetArticleTabsState();
        const auto tab_state =
            std::find_if(tabs_state.tabs.begin(), tabs_state.tabs.end(),
                         [id](const auto& tab) { return tab.id == id; });
        if (tab_state != tabs_state.tabs.end()) {
            navigation_title =
                QString::fromStdString(tab_state->navigation.title);
        }
        try {
            const auto response = request->Await();
            if (!response.entries.empty()) {
                StoreLookupResults(id, response);
                const auto article = facade_->ComposeLookupPage(response);
                const std::uint64_t presentation_generation =
                    lookup_results_[id].generation;
                std::optional<PendingArticleSearchHandoff> search_handoff;
                if (auto pending = pending_article_search_handoffs_.find(id);
                    pending != pending_article_search_handoffs_.end()) {
                    if (pending->second.lookup_generation ==
                            presentation_generation &&
                        pending->second.view == view) {
                        search_handoff = pending->second;
                    }
                    pending_article_search_handoffs_.erase(pending);
                }
                connect(
                    view, &ArticleView::loadFinished, this,
                    [this, id, view, presentation_generation,
                     search_handoff = std::move(search_handoff)](bool success) {
                        const auto current = lookup_results_.find(id);
                        if (success && current != lookup_results_.end() &&
                            current->second.generation ==
                                presentation_generation &&
                            ArticleViewForTab(id) == view) {
                            RefreshDictionaryContext(id);
                            if (search_handoff.has_value() &&
                                search_handoff->view == view) {
                                const auto navigation =
                                    article_navigation_generations_.find(id);
                                if (navigation !=
                                    article_navigation_generations_.end()) {
                                    ExtractRenderedPageText(
                                        id, view,
                                        search_handoff
                                            ->accepted_query_generation,
                                        presentation_generation,
                                        search_handoff->search_generation,
                                        navigation->second);
                                }
                                DispatchArticleSearch(
                                    id, view, search_handoff->query,
                                    search_handoff->search_generation, false);
                            }
                        }
                    },
                    Qt::SingleShotConnection);
                if (article.sanitized_html.has_value()) {
                    PublishArticleHtml(
                        id, view,
                        QString::fromUtf8(article.sanitized_html->data(),
                                          static_cast<qsizetype>(
                                              article.sanitized_html->size())));
                } else {
                    PublishArticleHtml(
                        id, view,
                        QStringLiteral("<!doctype "
                                       "html><html><body><h1>%1</"
                                       "h1><p>%2</p></body></html>")
                            .arg(EscapeHtml(navigation_title),
                                 EscapeHtml(QString::fromStdString(
                                     article.plain_text))));
                }
                if (active)
                    status_->setText(tr("%1 result(s)")
                                         .arg(static_cast<qulonglong>(
                                             response.entries.size())));
            } else if (!response.errors.empty()) {
                pending_article_search_handoffs_.erase(id);
                lookup_results_[id].rows.clear();
                PublishArticleHtml(
                    id, view,
                    QStringLiteral("<!doctype html><html><body><h1>Lookup "
                                   "failed</h1><p>%1</p></body></html>")
                        .arg(EscapeHtml(QString::fromStdString(
                            response.errors.front().message))));
                if (active)
                    status_->setText(QStringLiteral("Lookup failed"));
            } else {
                pending_article_search_handoffs_.erase(id);
                lookup_results_[id].rows.clear();
                PublishArticleHtml(
                    id, view,
                    QStringLiteral(
                        "<!doctype html><html><body><h1>%1</h1><p>No "
                        "result found.</p></body></html>")
                        .arg(EscapeHtml(navigation_title)));
                if (active)
                    status_->setText(QStringLiteral("No result"));
            }
        } catch (const std::exception& error) {
            pending_article_search_handoffs_.erase(id);
            lookup_results_[id].rows.clear();
            PublishArticleHtml(
                id, view,
                QStringLiteral("<!doctype html><html><body><h1>Lookup "
                               "failed</h1><p>%1</p></body></html>")
                    .arg(EscapeHtml(QString::fromLocal8Bit(error.what()))));
            if (active)
                status_->setText(QStringLiteral("Lookup failed"));
        }
        RefreshDictionaryContext(id);
        if (active)
            RefreshResultsNavigation();
    }
    if (requests_.empty())
        completion_timer_->stop();
    const auto active_id = TabIdAt(article_tabs_->currentIndex());
    lookup_button_->setEnabled(requests_.find(active_id) == requests_.end());
}

void MainWindow::FindInArticle(bool backwards) {
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    auto* view = ArticleViewForTab(tab_id);
    if (tab_id == 0U || view == nullptr)
        return;
    InvalidateRenderedTextMatchPlan(tab_id);
    const QString text = article_search_->text();
    auto& presentation = article_search_presentations_[tab_id];
    presentation.accepted_query_generation = 0U;
    rendered_page_text_transports_.erase(tab_id);
    presentation.query = text;
    const std::uint64_t generation = ++presentation.generation;
    if (text.isEmpty()) {
        view->findText(QString());
        presentation.status.clear();
        article_search_status_->clear();
        return;
    }
    DispatchArticleSearch(tab_id, view, text, generation, backwards);
}

void MainWindow::DispatchArticleSearch(goldendict::core::ArticleTabId tab_id,
                                       ArticleView* view, const QString& text,
                                       std::uint64_t generation,
                                       bool backwards) {
    const auto navigation = article_navigation_generations_.find(tab_id);
    if (view == nullptr || view->page() == nullptr || text.isEmpty() ||
        ArticleViewForTab(tab_id) != view ||
        navigation == article_navigation_generations_.end()) {
        return;
    }
    QWebEnginePage::FindFlags flags;
    if (backwards)
        flags |= QWebEnginePage::FindBackward;
    const QPointer<MainWindow> guarded_window(this);
    const QPointer<ArticleView> guarded_view(view);
    const QPointer<QWebEnginePage> guarded_page(view->page());
    const std::uint64_t navigation_generation = navigation->second;
    view->findText(
        text, flags,
        [guarded_window, tab_id, guarded_view, guarded_page, text, generation,
         navigation_generation](const QWebEngineFindTextResult& result) {
            if (guarded_window.isNull())
                return;
            guarded_window->FinishArticleSearch(
                tab_id, guarded_view, guarded_page, navigation_generation, text,
                generation, result.activeMatch(), result.numberOfMatches());
        });
}

void MainWindow::FinishArticleSearch(goldendict::core::ArticleTabId tab_id,
                                     const QPointer<ArticleView>& view,
                                     const QPointer<QWebEnginePage>& page,
                                     std::uint64_t navigation_generation,
                                     const QString& text,
                                     std::uint64_t search_generation,
                                     int active_match, int number_of_matches) {
    const auto found = article_search_presentations_.find(tab_id);
    const auto navigation = article_navigation_generations_.find(tab_id);
    if (view.isNull() || page.isNull() ||
        found == article_search_presentations_.end() ||
        found->second.generation != search_generation ||
        found->second.query != text ||
        navigation == article_navigation_generations_.end() ||
        navigation->second != navigation_generation ||
        ArticleViewForTab(tab_id) != view.data() ||
        view->page() != page.data()) {
        return;
    }
    found->second.status =
        number_of_matches == 0
            ? QStringLiteral("No matches")
            : tr("%1 of %2").arg(active_match).arg(number_of_matches);
    if (TabIdAt(article_tabs_->currentIndex()) == tab_id)
        article_search_status_->setText(found->second.status);
}

void MainWindow::ExtractRenderedPageText(
    goldendict::core::ArticleTabId tab_id, ArticleView* view,
    std::uint64_t accepted_query_generation, std::uint64_t lookup_generation,
    std::uint64_t search_generation, std::uint64_t navigation_generation) {
    if (view == nullptr || accepted_query_generation == 0U ||
        ArticleViewForTab(tab_id) != view || view->page() == nullptr) {
        return;
    }
    QPointer<ArticleView> guarded_view(view);
    QPointer<QWebEnginePage> guarded_page(view->page());
    QPointer<MainWindow> guarded_window(this);
    view->page()->toPlainText([guarded_window, tab_id, guarded_view,
                               guarded_page, accepted_query_generation,
                               lookup_generation, search_generation,
                               navigation_generation](const QString& text) {
        if (guarded_window.isNull())
            return;
        const auto lookup = guarded_window->lookup_results_.find(tab_id);
        const auto search =
            guarded_window->article_search_presentations_.find(tab_id);
        const auto navigation =
            guarded_window->article_navigation_generations_.find(tab_id);
        if (guarded_view.isNull() || guarded_page.isNull() ||
            lookup == guarded_window->lookup_results_.end() ||
            lookup->second.generation != lookup_generation ||
            search == guarded_window->article_search_presentations_.end() ||
            search->second.generation != search_generation ||
            search->second.accepted_query_generation !=
                accepted_query_generation ||
            navigation ==
                guarded_window->article_navigation_generations_.end() ||
            navigation->second != navigation_generation ||
            guarded_window->ArticleViewForTab(tab_id) != guarded_view.data() ||
            guarded_view->page() != guarded_page.data()) {
            return;
        }
        guarded_window->rendered_page_text_transports_[tab_id] = {
            text,
            accepted_query_generation,
            lookup_generation,
            search_generation,
            navigation_generation,
            guarded_view,
            guarded_page};
        guarded_window->SubmitRenderedTextMatchPlan(tab_id);
    });
}

void MainWindow::SubmitRenderedTextMatchPlan(
    goldendict::core::ArticleTabId tab_id) {
    if (facade_ == nullptr || rendered_text_match_plan_controller_ == nullptr)
        return;
    const auto transport = rendered_page_text_transports_.find(tab_id);
    const auto search = article_search_presentations_.find(tab_id);
    if (transport == rendered_page_text_transports_.end() ||
        search == article_search_presentations_.end() ||
        search->second.accepted_query_generation == 0U ||
        search->second.accepted_query_generation !=
            transport->second.accepted_query_generation) {
        return;
    }

    const QByteArray rendered_text = transport->second.text.toUtf8();
    const QByteArray query_text = search->second.query.toUtf8();
    goldendict::core::RenderedTextMatchPlanRequest request;
    request.rendered_text.assign(
        rendered_text.constData(),
        static_cast<std::size_t>(rendered_text.size()));
    request.query_text.assign(query_text.constData(),
                              static_cast<std::size_t>(query_text.size()));
    request.mode = search->second.mode;
    request.match_case = search->second.match_case;
    request.ignore_diacritics = search->second.ignore_diacritics;
    request.ignore_word_order = search->second.ignore_word_order;
    request.maximum_word_distance = search->second.maximum_word_distance;

    const std::uint64_t generation = ++rendered_text_match_plan_generation_;
    pending_rendered_text_match_plan_ = RenderedTextMatchPlanIdentity{
        generation,
        transport->second.accepted_query_generation,
        transport->second.lookup_generation,
        transport->second.search_generation,
        transport->second.navigation_generation,
        tab_id,
        request,
        transport->second.view,
        transport->second.page};
    if (const auto current = rendered_text_match_plans_.find(tab_id);
        current != rendered_text_match_plans_.end() &&
        !current->second.application_token.isEmpty() &&
        !current->second.identity.view.isNull()) {
        current->second.identity.view->ClearFullTextHighlights(
            current->second.application_token, true);
        current->second.identity.view->ClearFullTextNavigation(
            current->second.application_token, true);
    }
    rendered_text_match_plans_.erase(tab_id);
    rendered_text_match_plan_controller_->Submit(std::move(request),
                                                 generation);
}

void MainWindow::FinishRenderedTextMatchPlan(
    std::uint64_t generation,
    goldendict::core::RenderedTextMatchPlanResult result) {
    if (!pending_rendered_text_match_plan_.has_value() || facade_ == nullptr ||
        pending_rendered_text_match_plan_->work_generation != generation) {
        return;
    }
    const auto identity = *pending_rendered_text_match_plan_;
    pending_rendered_text_match_plan_.reset();
    const auto lookup = lookup_results_.find(identity.tab_id);
    const auto search = article_search_presentations_.find(identity.tab_id);
    const auto navigation =
        article_navigation_generations_.find(identity.tab_id);
    const auto transport = rendered_page_text_transports_.find(identity.tab_id);
    const QByteArray current_query =
        search == article_search_presentations_.end()
            ? QByteArray{}
            : search->second.query.toUtf8();
    if (identity.view.isNull() || identity.page.isNull() ||
        lookup == lookup_results_.end() ||
        lookup->second.generation != identity.lookup_generation ||
        search == article_search_presentations_.end() ||
        search->second.generation != identity.search_generation ||
        search->second.accepted_query_generation !=
            identity.accepted_query_generation ||
        std::string(current_query.constData(),
                    static_cast<std::size_t>(current_query.size())) !=
            identity.request.query_text ||
        search->second.mode != identity.request.mode ||
        search->second.match_case != identity.request.match_case ||
        search->second.ignore_word_order !=
            identity.request.ignore_word_order ||
        search->second.maximum_word_distance !=
            identity.request.maximum_word_distance ||
        search->second.ignore_diacritics !=
            identity.request.ignore_diacritics ||
        navigation == article_navigation_generations_.end() ||
        navigation->second != identity.navigation_generation ||
        transport == rendered_page_text_transports_.end() ||
        transport->second.accepted_query_generation !=
            identity.accepted_query_generation ||
        transport->second.lookup_generation != identity.lookup_generation ||
        transport->second.search_generation != identity.search_generation ||
        transport->second.navigation_generation !=
            identity.navigation_generation ||
        transport->second.view != identity.view ||
        transport->second.page != identity.page ||
        ArticleViewForTab(identity.tab_id) != identity.view.data() ||
        identity.view->page() != identity.page.data()) {
        return;
    }
    const QByteArray current_rendered_text = transport->second.text.toUtf8();
    if (std::string(current_rendered_text.constData(),
                    static_cast<std::size_t>(current_rendered_text.size())) !=
        identity.request.rendered_text) {
        return;
    }
    if (result.error != goldendict::core::RenderedTextMatchPlanError::kNone) {
        rendered_text_match_plans_[identity.tab_id] =
            RenderedTextMatchPlanState{
                identity, std::move(result), QString(), false, 0, -1};
        return;
    }
    const QString application_token =
        QStringLiteral("goldendict-full-text-%1").arg(generation);
    std::vector<ArticleHighlightRange> ranges;
    ranges.reserve(result.ranges.size());
    for (const auto& range : result.ranges) {
        ranges.push_back(
            {range.byte_offset, range.byte_length,
             QString::fromUtf8(range.literal.data(),
                               static_cast<qsizetype>(range.literal.size()))});
    }
    rendered_text_match_plans_[identity.tab_id] = RenderedTextMatchPlanState{
        identity, std::move(result), application_token, false, 0, -1};
    const QString rendered_text = transport->second.text;
    identity.view->ApplyFullTextHighlights(
        application_token, rendered_text, ranges, identity.request.match_case,
        [guarded_window = QPointer<MainWindow>(this), tab_id = identity.tab_id,
         guarded_view = identity.view, guarded_page = identity.page,
         application_token](ArticleHighlightResult applied) {
            if (guarded_window.isNull())
                return;
            const auto found =
                guarded_window->rendered_text_match_plans_.find(tab_id);
            if (found == guarded_window->rendered_text_match_plans_.end() ||
                found->second.application_token != application_token ||
                applied.token != application_token || guarded_view.isNull() ||
                guarded_page.isNull() ||
                guarded_window->ArticleViewForTab(tab_id) !=
                    guarded_view.data() ||
                guarded_view->page() != guarded_page.data()) {
                if (!guarded_view.isNull())
                    guarded_view->ClearFullTextHighlights(application_token);
                return;
            }
            const int expected_ordered =
                static_cast<int>(found->second.result.ranges.size());
            if (!applied.applied || applied.ordered_count != expected_ordered ||
                applied.current_position != (expected_ordered == 0 ? -1 : 0)) {
                guarded_view->ClearFullTextHighlights(application_token);
                guarded_window->rendered_text_match_plans_.erase(found);
                return;
            }
            found->second.applied = true;
            found->second.occurrence_count = applied.occurrence_count;
            found->second.current_position = applied.current_position;
            if (applied.ordered_count > 0) {
                guarded_view->PublishFullTextNavigationSnapshot(
                    {true, applied.token, applied.current_position,
                     applied.ordered_count, applied.current_position > 0,
                     applied.current_position + 1 < applied.ordered_count});
            }
        });
}

void MainWindow::InvalidateRenderedTextMatchPlan(
    std::optional<goldendict::core::ArticleTabId> tab_id) {
    ++rendered_text_match_plan_generation_;
    pending_rendered_text_match_plan_.reset();
    if (rendered_text_match_plan_controller_ != nullptr)
        rendered_text_match_plan_controller_->Cancel();
    if (tab_id.has_value()) {
        const auto found = rendered_text_match_plans_.find(*tab_id);
        if (found != rendered_text_match_plans_.end() &&
            !found->second.application_token.isEmpty() &&
            !found->second.identity.view.isNull()) {
            found->second.identity.view->ClearFullTextHighlights(
                found->second.application_token, true);
            found->second.identity.view->ClearFullTextNavigation(
                found->second.application_token, true);
        }
        rendered_text_match_plans_.erase(*tab_id);
    } else {
        for (const auto& [id, state] : rendered_text_match_plans_) {
            static_cast<void>(id);
            if (!state.application_token.isEmpty() &&
                !state.identity.view.isNull()) {
                state.identity.view->ClearFullTextHighlights(
                    state.application_token, true);
                state.identity.view->ClearFullTextNavigation(
                    state.application_token, true);
            }
        }
        rendered_text_match_plans_.clear();
    }
}

void MainWindow::NavigateFullTextHighlight(
    goldendict::core::ArticleTabId tab_id,
    ArticleHighlightNavigationDirection direction) {
    const auto found = rendered_text_match_plans_.find(tab_id);
    if (found == rendered_text_match_plans_.end() || !found->second.applied ||
        found->second.application_token.isEmpty() ||
        found->second.current_position < 0 ||
        found->second.result.ranges.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        found->second.identity.view.isNull() ||
        found->second.identity.page.isNull()) {
        return;
    }
    const RenderedTextMatchPlanIdentity identity = found->second.identity;
    const QString token = found->second.application_token;
    const int old_position = found->second.current_position;
    const int ordered_count =
        static_cast<int>(found->second.result.ranges.size());
    const int expected_position =
        direction == ArticleHighlightNavigationDirection::kPrevious
            ? (old_position > 0 ? old_position - 1 : old_position)
            : (old_position + 1 < ordered_count ? old_position + 1
                                                : old_position);
    identity.view->NavigateFullTextHighlight(
        token, direction,
        [guarded_window = QPointer<MainWindow>(this), identity, token,
         old_position, ordered_count,
         expected_position](ArticleHighlightNavigationSnapshot snapshot) {
            if (guarded_window.isNull())
                return;
            const auto current =
                guarded_window->rendered_text_match_plans_.find(
                    identity.tab_id);
            if (current == guarded_window->rendered_text_match_plans_.end() ||
                !snapshot.accepted || snapshot.token != token ||
                snapshot.ordered_count != ordered_count ||
                snapshot.current_position != expected_position ||
                current->second.application_token != token ||
                !current->second.applied ||
                current->second.current_position != old_position ||
                static_cast<int>(current->second.result.ranges.size()) !=
                    ordered_count ||
                current->second.identity.work_generation !=
                    identity.work_generation ||
                current->second.identity.accepted_query_generation !=
                    identity.accepted_query_generation ||
                current->second.identity.lookup_generation !=
                    identity.lookup_generation ||
                current->second.identity.search_generation !=
                    identity.search_generation ||
                current->second.identity.navigation_generation !=
                    identity.navigation_generation ||
                current->second.identity.tab_id != identity.tab_id ||
                current->second.identity.request.ignore_diacritics !=
                    identity.request.ignore_diacritics ||
                current->second.identity.request.rendered_text !=
                    identity.request.rendered_text ||
                current->second.identity.request.query_text !=
                    identity.request.query_text ||
                current->second.identity.request.mode !=
                    identity.request.mode ||
                current->second.identity.request.match_case !=
                    identity.request.match_case ||
                current->second.identity.request.ignore_word_order !=
                    identity.request.ignore_word_order ||
                current->second.identity.request.maximum_word_distance !=
                    identity.request.maximum_word_distance ||
                current->second.identity.request.timeout !=
                    identity.request.timeout ||
                current->second.identity.view != identity.view ||
                current->second.identity.page != identity.page ||
                identity.view.isNull() || identity.page.isNull() ||
                guarded_window->ArticleViewForTab(identity.tab_id) !=
                    identity.view.data() ||
                identity.view->page() != identity.page.data()) {
                return;
            }
            current->second.current_position = snapshot.current_position;
            identity.view->PublishFullTextNavigationSnapshot(snapshot);
        });
}

void MainWindow::RefreshArticleSearch() {
    if (article_tabs_ == nullptr || article_search_ == nullptr ||
        article_search_status_ == nullptr) {
        return;
    }
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    const auto found = article_search_presentations_.find(tab_id);
    const QSignalBlocker blocker(article_search_);
    article_search_->setText(found == article_search_presentations_.end()
                                 ? QString()
                                 : found->second.query);
    article_search_status_->setText(found == article_search_presentations_.end()
                                        ? QString()
                                        : found->second.status);
    search_in_page_action_->setEnabled(tab_id != 0U &&
                                       article_view_ != nullptr);
}

void MainWindow::SaveArticleAsPdf() {
    const QString file_name = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Article as PDF"), QString(),
        QStringLiteral("PDF files (*.pdf)"));
    if (file_name.isEmpty()) {
        return;
    }
    const QString path =
        file_name.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
            ? file_name
            : file_name + QStringLiteral(".pdf");
    StartPdfExport(path);
}

void MainWindow::StartPdfExport(const QString& path) {
    if (article_view_ == nullptr)
        return;
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    const auto navigation = article_navigation_generations_.find(tab_id);
    if (tab_id == 0U || article_view_->page() == nullptr ||
        ArticleViewForTab(tab_id) != article_view_ ||
        navigation == article_navigation_generations_.end()) {
        return;
    }
    ++pdf_request_generation_;
    if (pdf_request_generation_ == 0U)
        pdf_request_generation_ = 1U;
    pending_pdf_request_ = ArticleOutputRequest{pdf_request_generation_,
                                                facade_binding_generation_,
                                                navigation->second,
                                                tab_id,
                                                article_view_,
                                                article_view_->page(),
                                                path};
    status_->setText(QStringLiteral("Saving PDF..."));
    const QPointer<MainWindow> guarded_window(this);
    const auto completion =
        [guarded_window,
         generation = pdf_request_generation_](const QByteArray& pdf_data) {
            if (!guarded_window.isNull())
                guarded_window->FinishPdfExport(generation, pdf_data);
        };
    if (pdf_dispatcher_)
        pdf_dispatcher_(article_view_, completion);
    else
        article_view_->printToPdf(completion);
}

void MainWindow::FinishPdfExport(std::uint64_t request_generation,
                                 const QByteArray& pdf_data) {
    if (!pending_pdf_request_.has_value() ||
        pending_pdf_request_->request_generation != request_generation) {
        return;
    }
    const auto request = *pending_pdf_request_;
    pending_pdf_request_.reset();
    const auto navigation =
        article_navigation_generations_.find(request.tab_id);
    const bool owned =
        request.facade_binding_generation == facade_binding_generation_ &&
        !request.view.isNull() && !request.page.isNull() &&
        navigation != article_navigation_generations_.end() &&
        navigation->second == request.navigation_generation &&
        ArticleViewForTab(request.tab_id) == request.view.data() &&
        request.view->page() == request.page.data();
    if (!owned)
        return;
    const bool saved =
        !pdf_data.isEmpty() &&
        (pdf_writer_ ? pdf_writer_(request.path, pdf_data)
                     : [&request, &pdf_data]() {
                           QSaveFile file(request.path);
                           return file.open(QIODevice::WriteOnly) &&
                                  file.write(pdf_data) == pdf_data.size() &&
                                  file.commit();
                       }());
    if (TabIdAt(article_tabs_->currentIndex()) == request.tab_id) {
        status_->setText(saved ? QStringLiteral("PDF saved")
                               : QStringLiteral("PDF save failed"));
    }
}

void MainWindow::InvalidateArticleOutputOwnership(
    goldendict::core::ArticleTabId tab_id, ArticleView* view) {
    if (pending_pdf_request_.has_value() &&
        pending_pdf_request_->tab_id == tab_id &&
        pending_pdf_request_->view.data() == view) {
        pending_pdf_request_.reset();
    }
    if (pending_print_request_.has_value() &&
        pending_print_request_->tab_id == tab_id &&
        pending_print_request_->view.data() == view) {
        pending_print_request_.reset();
        print_in_progress_ = false;
        UpdateFileActions();
    }
}

void MainWindow::PrintArticle() {
    if (article_view_ == nullptr || print_in_progress_)
        return;
    if (printer_ == nullptr)
        printer_ = std::make_unique<QPrinter>(QPrinter::HighResolution);
    const bool available =
        printer_available_ ? printer_available_() : printer_->isValid();
    if (!available) {
        status_->setText(QStringLiteral("No printer is available"));
        return;
    }
    const QPointer<ArticleView> captured_view(article_view_);
    const bool accepted =
        print_dialog_executor_
            ? print_dialog_executor_(printer_.get())
            : [this]() {
                  QPrintDialog dialog(printer_.get(), this);
                  dialog.setWindowTitle(QStringLiteral("Print Article"));
                  return dialog.exec() == QDialog::Accepted;
              }();
    if (!accepted)
        return;
    if (!captured_view.isNull())
        StartPrinterRender(captured_view.data(), printer_.get());
}

void MainWindow::PreviewArticle() {
    if (article_view_ == nullptr || print_in_progress_)
        return;
    if (printer_ == nullptr)
        printer_ = std::make_unique<QPrinter>(QPrinter::HighResolution);
    const bool available =
        printer_available_ ? printer_available_() : printer_->isValid();
    if (!available) {
        status_->setText(QStringLiteral("No printer is available"));
        return;
    }
    const QPointer<ArticleView> captured_view(article_view_);
    const auto paint = [this, captured_view]() {
        if (!captured_view.isNull())
            StartPrinterRender(captured_view.data(), printer_.get());
    };
    if (print_preview_executor_) {
        print_preview_executor_(printer_.get(), paint);
        return;
    }
    QPrintPreviewDialog dialog(printer_.get(), this);
    connect(&dialog, &QPrintPreviewDialog::paintRequested, this,
            [paint](QPrinter*) { paint(); });
    dialog.exec();
}

void MainWindow::StartPrinterRender(ArticleView* view, QPrinter* printer) {
    if (view == nullptr || printer == nullptr || print_in_progress_)
        return;
    print_in_progress_ = true;
    const auto tab_id = TabIdAt(article_tabs_->indexOf(view));
    const auto navigation = article_navigation_generations_.find(tab_id);
    if (tab_id == 0U || view->page() == nullptr ||
        navigation == article_navigation_generations_.end()) {
        print_in_progress_ = false;
        return;
    }
    pending_print_request_ = ArticleOutputRequest{0U,
                                                  facade_binding_generation_,
                                                  navigation->second,
                                                  tab_id,
                                                  view,
                                                  view->page(),
                                                  {}};
    UpdateFileActions();
    status_->setText(QStringLiteral("Printing..."));
    if (print_dispatcher_) {
        print_dispatcher_(view, printer);
    } else {
        view->print(printer);
    }
}

void MainWindow::FinishPrinterRender(ArticleView* view, bool success) {
    if (!pending_print_request_.has_value() ||
        pending_print_request_->view.data() != view) {
        return;
    }
    const auto request = *pending_print_request_;
    const auto navigation =
        article_navigation_generations_.find(request.tab_id);
    const bool owned =
        request.facade_binding_generation == facade_binding_generation_ &&
        !request.view.isNull() && !request.page.isNull() &&
        navigation != article_navigation_generations_.end() &&
        navigation->second == request.navigation_generation &&
        ArticleViewForTab(request.tab_id) == request.view.data() &&
        request.view->page() == request.page.data();
    if (!owned)
        return;
    pending_print_request_.reset();
    print_in_progress_ = false;
    UpdateFileActions();
    if (TabIdAt(article_tabs_->currentIndex()) == request.tab_id) {
        status_->setText(success ? QStringLiteral("Article printed")
                                 : QStringLiteral("Printing failed"));
    }
}

void MainWindow::SaveArticle() {
    if (article_view_ == nullptr || save_in_progress_)
        return;
    const auto tab_id = TabIdAt(article_tabs_->currentIndex());
    auto* const view = article_view_;
    const auto navigation = article_navigation_generations_.find(tab_id);
    if (tab_id == 0U || view->page() == nullptr ||
        ArticleViewForTab(tab_id) != view ||
        navigation == article_navigation_generations_.end()) {
        return;
    }
    const QString file_name =
        save_article_path_provider_
            ? save_article_path_provider_()
            : QFileDialog::getSaveFileName(
                  this, QStringLiteral("Save Article as HTML"), QString(),
                  QStringLiteral("HTML files (*.html *.htm)"));
    if (file_name.isEmpty()) {
        return;
    }
    const QString path = QFileInfo(file_name).suffix().isEmpty()
                             ? file_name + QStringLiteral(".html")
                             : file_name;
    const QPointer<MainWindow> guarded_window(this);
    const QPointer<ArticleView> guarded_view(view);
    const QPointer<QWebEnginePage> guarded_page(view->page());
    const std::uint64_t navigation_generation = navigation->second;
    save_in_progress_ = true;
    UpdateFileActions();
    view->page()->toHtml([guarded_window, tab_id, guarded_view, guarded_page,
                          navigation_generation, path](const QString& html) {
        if (guarded_window.isNull())
            return;
        guarded_window->FinishArticleSave(tab_id, guarded_view, guarded_page,
                                          navigation_generation, path, html);
    });
}

void MainWindow::FinishArticleSave(goldendict::core::ArticleTabId tab_id,
                                   const QPointer<ArticleView>& view,
                                   const QPointer<QWebEnginePage>& page,
                                   std::uint64_t navigation_generation,
                                   const QString& path, const QString& html) {
    const auto navigation = article_navigation_generations_.find(tab_id);
    const bool owned = !view.isNull() && !page.isNull() &&
                       navigation != article_navigation_generations_.end() &&
                       navigation->second == navigation_generation &&
                       ArticleViewForTab(tab_id) == view.data() &&
                       view->page() == page.data();
    save_in_progress_ = false;
    UpdateFileActions();
    if (!owned) {
        status_->setText(QStringLiteral("HTML save canceled"));
        return;
    }
    const bool saved = article_save_writer_
                           ? article_save_writer_(path, html)
                           : [&path, &html]() {
                                 QSaveFile file(path);
                                 return file.open(QIODevice::WriteOnly) &&
                                        file.write(html.toUtf8()) != -1 &&
                                        file.commit();
                             }();
    status_->setText(saved ? QStringLiteral("HTML saved")
                           : QStringLiteral("HTML save failed"));
}

void MainWindow::UpdateFileActions() {
    const bool has_article = facade_ != nullptr && article_view_ != nullptr;
    new_tab_action_->setEnabled(facade_ != nullptr);
    save_article_action_->setEnabled(has_article && !save_in_progress_);
    print_action_->setEnabled(has_article && !print_in_progress_);
    print_preview_action_->setEnabled(has_article && !print_in_progress_);
    quit_action_->setEnabled(true);
}

void MainWindow::UpdateNavigationActions() {
    bool can_back = false;
    bool can_forward = false;
    if (facade_ != nullptr) {
        const auto state = facade_->GetArticleTabsState();
        const auto found = std::find_if(
            state.tabs.begin(), state.tabs.end(),
            [&](const auto& tab) { return tab.id == state.active_tab_id; });
        if (found != state.tabs.end()) {
            can_back = found->can_go_back;
            can_forward = found->can_go_forward;
        }
    }
    back_action_->setEnabled(can_back);
    forward_action_->setEnabled(can_forward);
}

void MainWindow::ZoomArticle(double delta) {
    constexpr double kMinimumZoom = 0.25;
    constexpr double kMaximumZoom = 5.0;
    article_view_->setZoomFactor(std::clamp(article_view_->zoomFactor() + delta,
                                            kMinimumZoom, kMaximumZoom));
}

void MainWindow::ShowMessage(const QString& title, const QString& message) {
    article_view_->setHtml(
        QStringLiteral("<!doctype html><html><head><meta charset=\"utf-8\">"
                       "</head><body><h1>%1</h1><p>%2</p></body></html>")
            .arg(EscapeHtml(title), EscapeHtml(message)));
}
