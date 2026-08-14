// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/desktop_facade.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "../article/article_composer.h"
#include "../article/internal_url.h"
#include "../dictionary/dictionary_backend.h"
#include "article_tab_session.h"
#include "goldendict/core/application.h"

namespace goldendict::core {
namespace {

constexpr char kUntitledTabTitle[] = "(untitled)";

bool HasSameNavigationIdentity(const TabNavigationState& left,
                               const TabNavigationState& right) {
    return left.kind == right.kind && left.query == right.query &&
           left.group_id == right.group_id &&
           left.internal_url == right.internal_url &&
           left.source_dictionary_id == right.source_dictionary_id &&
           left.source_article_id == right.source_article_id &&
           left.target_article_id == right.target_article_id &&
           left.target_anchor == right.target_anchor;
}

TabNavigationState EmptyNavigation() {
    TabNavigationState state;
    state.title = kUntitledTabTitle;
    return state;
}

struct TabRecord {
    ArticleTabId id = 0U;
    std::vector<TabNavigationState> history;
    std::size_t history_index = 0U;
};

class DesktopFacadeImpl final : public DesktopFacade {
   public:
    DesktopFacadeImpl(
        const CoreConfiguration& configuration,
        std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources)
        : service_(CreateDictionaryService(configuration,
                                           std::move(runtime_sources))) {
        tabs_.push_back(CreateTabRecord(EmptyNavigation()));
        active_tab_id_ = tabs_.front().id;
    }

    DictionaryService& GetDictionaryService() noexcept override {
        return *service_;
    }

    const DictionaryService& GetDictionaryService() const noexcept override {
        return *service_;
    }

    ArticleContent ComposeLookupPage(
        const LookupResponse& response) const override {
        return article::ComposeLookupPage(response);
    }

    std::optional<ArticleUrl> ResolveArticleUrl(
        const std::string& url) const override {
        const auto parsed = article::ParseInternalUrl(url);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        ArticleUrl result;
        if (parsed->kind == article::InternalUrlKind::kLookup) {
            result.kind = ArticleUrlKind::kLookup;
            result.lookup_text = parsed->target;
        } else {
            result.kind = ArticleUrlKind::kResource;
            result.resource.dictionary_id = parsed->dictionary_id;
            result.resource.resource_id = parsed->target;
            result.resource.media_type =
                dictionary::MediaTypeForResourceId(parsed->target);
        }
        return result;
    }

    ArticleTabsState GetArticleTabsState() const override {
        ArticleTabsState state;
        state.active_tab_id = active_tab_id_;
        state.tabs.reserve(tabs_.size());
        for (const auto& tab : tabs_) {
            state.tabs.push_back({tab.id, tab.history[tab.history_index],
                                  tab.history_index > 0U,
                                  tab.history_index + 1U < tab.history.size()});
        }
        return state;
    }

    ArticleTabSession ExportArticleTabSession() const override {
        ArticleTabSession session;
        session.active_tab_id = active_tab_id_;
        session.tabs.reserve(tabs_.size());
        for (const auto& tab : tabs_) {
            session.tabs.push_back({tab.id, tab.history, tab.history_index});
        }
        return session;
    }

    TabOperationResult RestoreArticleTabSession(
        const ArticleTabSession& session) override {
        ArticleTabId next_tab_id = 0U;
        if (!application::ValidateArticleTabSession(session, &next_tab_id)) {
            return {TabOperationError::kInvalidSession, 0U};
        }
        std::vector<TabRecord> restored;
        restored.reserve(session.tabs.size());
        for (const auto& tab : session.tabs) {
            restored.push_back({tab.id, tab.history, tab.history_cursor});
        }
        tabs_ = std::move(restored);
        active_tab_id_ = session.active_tab_id;
        next_tab_id_ = next_tab_id;
        return {TabOperationError::kNone, active_tab_id_};
    }

    TabOperationResult OpenArticleTab(
        const TabNavigationState& navigation, TabOpenPolicy open_policy,
        TabActivationPolicy activation_policy,
        TabPlacementPolicy placement_policy) override {
        if (!application::IsValidTabNavigation(navigation)) {
            return {TabOperationError::kInvalidNavigation, 0U};
        }
        if (open_policy == TabOpenPolicy::kReuseExisting) {
            const auto found = std::find_if(
                tabs_.begin(), tabs_.end(), [&](const TabRecord& tab) {
                    return HasSameNavigationIdentity(
                        tab.history[tab.history_index], navigation);
                });
            if (found != tabs_.end()) {
                if (activation_policy == TabActivationPolicy::kActivate) {
                    active_tab_id_ = found->id;
                }
                return {TabOperationError::kNone, found->id};
            }
            open_policy = TabOpenPolicy::kNewTab;
        }
        if (open_policy == TabOpenPolicy::kNewTab) {
            if (tabs_.size() >= kMaximumArticleTabs) {
                return {TabOperationError::kTabLimitReached, 0U};
            }
            auto insertion = tabs_.end();
            if (placement_policy == TabPlacementPolicy::kAfterActive) {
                insertion = std::find_if(tabs_.begin(), tabs_.end(),
                                         [this](const TabRecord& tab) {
                                             return tab.id == active_tab_id_;
                                         });
                ++insertion;
            }
            const auto created =
                tabs_.insert(insertion, CreateTabRecord(navigation));
            const ArticleTabId id = created->id;
            if (activation_policy == TabActivationPolicy::kActivate) {
                active_tab_id_ = id;
            }
            return {TabOperationError::kNone, id};
        }

        auto* tab = FindTab(active_tab_id_);
        if (tab->history.size() >= kMaximumTabNavigationEntries &&
            tab->history_index + 1U == tab->history.size()) {
            return {TabOperationError::kNavigationLimitReached, tab->id};
        }
        tab->history.erase(tab->history.begin() + static_cast<std::ptrdiff_t>(
                                                      tab->history_index + 1U),
                           tab->history.end());
        tab->history.push_back(navigation);
        ++tab->history_index;
        return {TabOperationError::kNone, tab->id};
    }

    TabOperationResult ActivateArticleTab(ArticleTabId tab_id) override {
        if (FindTab(tab_id) == nullptr) {
            return {TabOperationError::kInvalidTabId, tab_id};
        }
        active_tab_id_ = tab_id;
        return {TabOperationError::kNone, tab_id};
    }

    TabOperationResult CloseArticleTab(ArticleTabId tab_id) override {
        const auto found = FindTabIterator(tab_id);
        if (found == tabs_.end()) {
            return {TabOperationError::kInvalidTabId, tab_id};
        }
        const auto index = static_cast<std::size_t>(found - tabs_.begin());
        const bool was_active = tab_id == active_tab_id_;
        tabs_.erase(found);
        if (tabs_.empty()) {
            tabs_.push_back(CreateTabRecord(EmptyNavigation()));
            active_tab_id_ = tabs_.front().id;
        } else if (was_active) {
            const std::size_t fallback = std::min(index, tabs_.size() - 1U);
            active_tab_id_ = tabs_[fallback].id;
        }
        return {TabOperationError::kNone, active_tab_id_};
    }

    TabOperationResult CloseOtherArticleTabs(ArticleTabId tab_id) override {
        const auto* selected = FindTab(tab_id);
        if (selected == nullptr) {
            return {TabOperationError::kInvalidTabId, tab_id};
        }
        TabRecord retained = *selected;
        tabs_.clear();
        tabs_.push_back(std::move(retained));
        active_tab_id_ = tab_id;
        return {TabOperationError::kNone, tab_id};
    }

    TabOperationResult GoBackInArticleTab(ArticleTabId tab_id) override {
        auto* tab = FindTab(tab_id);
        if (tab == nullptr) {
            return {TabOperationError::kInvalidTabId, tab_id};
        }
        if (tab->history_index == 0U) {
            return {TabOperationError::kNoBackEntry, tab_id};
        }
        --tab->history_index;
        return {TabOperationError::kNone, tab_id};
    }

    TabOperationResult GoForwardInArticleTab(ArticleTabId tab_id) override {
        auto* tab = FindTab(tab_id);
        if (tab == nullptr) {
            return {TabOperationError::kInvalidTabId, tab_id};
        }
        if (tab->history_index + 1U >= tab->history.size()) {
            return {TabOperationError::kNoForwardEntry, tab_id};
        }
        ++tab->history_index;
        return {TabOperationError::kNone, tab_id};
    }

   private:
    TabRecord CreateTabRecord(const TabNavigationState& navigation) {
        return {next_tab_id_++, {navigation}, 0U};
    }

    std::vector<TabRecord>::iterator FindTabIterator(ArticleTabId tab_id) {
        return std::find_if(
            tabs_.begin(), tabs_.end(),
            [tab_id](const TabRecord& tab) { return tab.id == tab_id; });
    }

    TabRecord* FindTab(ArticleTabId tab_id) {
        const auto found = FindTabIterator(tab_id);
        return found == tabs_.end() ? nullptr : &*found;
    }

    const TabRecord* FindTab(ArticleTabId tab_id) const {
        const auto found = std::find_if(
            tabs_.begin(), tabs_.end(),
            [tab_id](const TabRecord& tab) { return tab.id == tab_id; });
        return found == tabs_.end() ? nullptr : &*found;
    }

    std::unique_ptr<DictionaryService> service_;
    std::vector<TabRecord> tabs_;
    ArticleTabId active_tab_id_ = 0U;
    ArticleTabId next_tab_id_ = 1U;
};

}  // namespace

DesktopFacade::~DesktopFacade() = default;

std::unique_ptr<DesktopFacade> CreateDesktopFacade(
    const CoreConfiguration& configuration) {
    return CreateDesktopFacade(configuration, {});
}

std::unique_ptr<DesktopFacade> CreateDesktopFacade(
    const CoreConfiguration& configuration,
    std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources) {
    return std::make_unique<DesktopFacadeImpl>(configuration,
                                               std::move(runtime_sources));
}

}  // namespace goldendict::core
