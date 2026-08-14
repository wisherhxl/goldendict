// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_DESKTOP_FACADE_H_
#define GOLDENDICT_CORE_DESKTOP_FACADE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/dictionary_service.h"
#include "goldendict/core/headword_export.h"

namespace goldendict::core {

enum class ArticleUrlKind {
    kLookup,
    kResource,
};

struct ArticleUrl {
    ArticleUrlKind kind = ArticleUrlKind::kLookup;
    std::string lookup_text;
    ResourceReference resource;
};

inline constexpr std::size_t kMaximumArticleTabs = 32U;
inline constexpr std::size_t kMaximumTabNavigationEntries = 100U;

using ArticleTabId = std::uint64_t;

enum class TabNavigationKind {
    kEmpty,
    kLookup,
    kInternalLink,
};

struct TabNavigationState {
    TabNavigationKind kind = TabNavigationKind::kEmpty;
    std::string query;
    std::uint32_t group_id = 0U;
    std::string title;
    std::string internal_url;
    std::string source_dictionary_id;
    std::string source_article_id;
    std::string target_article_id;
    std::string target_anchor;

    bool operator==(const TabNavigationState& other) const noexcept {
        return kind == other.kind && query == other.query &&
               group_id == other.group_id && title == other.title &&
               internal_url == other.internal_url &&
               source_dictionary_id == other.source_dictionary_id &&
               source_article_id == other.source_article_id &&
               target_article_id == other.target_article_id &&
               target_anchor == other.target_anchor;
    }
};

struct ArticleTabState {
    ArticleTabId id = 0U;
    TabNavigationState navigation;
    bool can_go_back = false;
    bool can_go_forward = false;
};

struct ArticleTabsState {
    std::vector<ArticleTabState> tabs;
    ArticleTabId active_tab_id = 0U;
};

struct ArticleTabSessionTab {
    ArticleTabId id = 0U;
    std::vector<TabNavigationState> history;
    std::size_t history_cursor = 0U;

    bool operator==(const ArticleTabSessionTab& other) const noexcept {
        return id == other.id && history == other.history &&
               history_cursor == other.history_cursor;
    }
};

struct ArticleTabSession {
    std::vector<ArticleTabSessionTab> tabs;
    ArticleTabId active_tab_id = 0U;

    bool operator==(const ArticleTabSession& other) const noexcept {
        return tabs == other.tabs && active_tab_id == other.active_tab_id;
    }
};

enum class TabOpenPolicy {
    kCurrentTab,
    kReuseExisting,
    kNewTab,
};

enum class TabActivationPolicy {
    kActivate,
    kKeepActive,
};

enum class TabPlacementPolicy {
    kAppend,
    kAfterActive,
};

enum class TabOperationError {
    kNone,
    kInvalidTabId,
    kInvalidNavigation,
    kTabLimitReached,
    kNavigationLimitReached,
    kNoBackEntry,
    kNoForwardEntry,
    kInvalidSession,
};

struct TabOperationResult {
    TabOperationError error = TabOperationError::kNone;
    ArticleTabId tab_id = 0U;

    explicit operator bool() const noexcept {
        return error == TabOperationError::kNone;
    }
};

class GOLDENDICT_EXPORTS DesktopFacade {
   public:
    virtual ~DesktopFacade();

    virtual DictionaryService& GetDictionaryService() noexcept = 0;
    virtual const DictionaryService& GetDictionaryService() const noexcept = 0;
    virtual std::unique_ptr<HeadwordExportOperation> StartHeadwordExport(
        HeadwordExportRequest request) const = 0;
    virtual ArticleContent ComposeLookupPage(
        const LookupResponse& response) const = 0;
    virtual std::optional<ArticleUrl> ResolveArticleUrl(
        const std::string& url) const = 0;
    virtual ArticleTabsState GetArticleTabsState() const = 0;
    virtual ArticleTabSession ExportArticleTabSession() const = 0;
    virtual TabOperationResult RestoreArticleTabSession(
        const ArticleTabSession& session) = 0;
    virtual TabOperationResult OpenArticleTab(
        const TabNavigationState& navigation, TabOpenPolicy open_policy,
        TabActivationPolicy activation_policy,
        TabPlacementPolicy placement_policy = TabPlacementPolicy::kAppend) = 0;
    virtual TabOperationResult ActivateArticleTab(ArticleTabId tab_id) = 0;
    virtual TabOperationResult CloseArticleTab(ArticleTabId tab_id) = 0;
    virtual TabOperationResult CloseOtherArticleTabs(ArticleTabId tab_id) = 0;
    virtual TabOperationResult GoBackInArticleTab(ArticleTabId tab_id) = 0;
    virtual TabOperationResult GoForwardInArticleTab(ArticleTabId tab_id) = 0;
};

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_DESKTOP_FACADE_H_
