// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <limits>
#include <string>

#include "goldendict/core/application.h"
#include "goldendict/core/desktop_facade.h"

namespace goldendict::core {
namespace {

TabNavigationState Lookup(std::string query, std::uint32_t group_id = 0U) {
    TabNavigationState state;
    state.kind = TabNavigationKind::kLookup;
    state.query = std::move(query);
    state.group_id = group_id;
    state.title = state.query;
    return state;
}

class ArticleTabsTest : public QObject {
    Q_OBJECT

   private slots:
    void StartsWithSingleUntitledTab();
    void CreatesActivatesClosesAndClosesOthersDeterministically();
    void PreservesNavigationAndTruncatesForwardHistory();
    void AppliesReuseAndNewTabPolicies();
    void PreservesInternalLinkState();
    void ExportsAndRestoresCompleteSession();
    void RejectsInvalidSessionsAtomically();
    void RejectsLimitsAndInvalidOperationsAtomically();
};

void ArticleTabsTest::StartsWithSingleUntitledTab() {
    auto facade = CreateDesktopFacade({});

    const auto state = facade->GetArticleTabsState();

    QCOMPARE(state.tabs.size(), std::size_t{1});
    QCOMPARE(state.active_tab_id, state.tabs.front().id);
    QCOMPARE(state.tabs.front().navigation.kind, TabNavigationKind::kEmpty);
    QCOMPARE(state.tabs.front().navigation.title, "(untitled)");
    QVERIFY(!state.tabs.front().can_go_back);
    QVERIFY(!state.tabs.front().can_go_forward);
}

void ArticleTabsTest::CreatesActivatesClosesAndClosesOthersDeterministically() {
    auto facade = CreateDesktopFacade({});
    const ArticleTabId first = facade->GetArticleTabsState().active_tab_id;
    const auto second =
        facade->OpenArticleTab(Lookup("second", 2U), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kKeepActive);
    const auto third =
        facade->OpenArticleTab(Lookup("third", 3U), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kActivate);
    QVERIFY(second);
    QVERIFY(third);
    QCOMPARE(facade->GetArticleTabsState().active_tab_id, third.tab_id);

    QVERIFY(facade->CloseArticleTab(third.tab_id));
    QCOMPARE(facade->GetArticleTabsState().active_tab_id, second.tab_id);
    QVERIFY(facade->ActivateArticleTab(first));
    QVERIFY(facade->CloseArticleTab(first));
    QCOMPARE(facade->GetArticleTabsState().active_tab_id, second.tab_id);

    const auto replacement = facade->CloseArticleTab(second.tab_id);
    QVERIFY(replacement);
    const auto singleton = facade->GetArticleTabsState();
    QCOMPARE(singleton.tabs.size(), std::size_t{1});
    QVERIFY(singleton.active_tab_id != second.tab_id);
    QCOMPARE(singleton.tabs.front().navigation.kind, TabNavigationKind::kEmpty);

    const auto kept =
        facade->OpenArticleTab(Lookup("kept"), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kKeepActive);
    const auto removed =
        facade->OpenArticleTab(Lookup("removed"), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kActivate);
    QVERIFY(kept);
    QVERIFY(removed);
    QVERIFY(facade->CloseOtherArticleTabs(kept.tab_id));
    const auto one = facade->GetArticleTabsState();
    QCOMPARE(one.tabs.size(), std::size_t{1});
    QCOMPARE(one.tabs.front().id, kept.tab_id);
    QCOMPARE(one.active_tab_id, kept.tab_id);
}

void ArticleTabsTest::PreservesNavigationAndTruncatesForwardHistory() {
    auto facade = CreateDesktopFacade({});
    const ArticleTabId tab_id = facade->GetArticleTabsState().active_tab_id;
    QVERIFY(facade->OpenArticleTab(Lookup("one", 7U),
                                   TabOpenPolicy::kCurrentTab,
                                   TabActivationPolicy::kActivate));
    QVERIFY(facade->OpenArticleTab(Lookup("two", 8U),
                                   TabOpenPolicy::kCurrentTab,
                                   TabActivationPolicy::kActivate));
    QVERIFY(facade->GoBackInArticleTab(tab_id));
    auto state = facade->GetArticleTabsState().tabs.front();
    QCOMPARE(state.navigation.query, "one");
    QCOMPARE(state.navigation.group_id, 7U);
    QVERIFY(state.can_go_back);
    QVERIFY(state.can_go_forward);

    QVERIFY(facade->OpenArticleTab(Lookup("branch", 9U),
                                   TabOpenPolicy::kCurrentTab,
                                   TabActivationPolicy::kActivate));
    state = facade->GetArticleTabsState().tabs.front();
    QCOMPARE(state.navigation.query, "branch");
    QCOMPARE(state.navigation.group_id, 9U);
    QVERIFY(!state.can_go_forward);
    QCOMPARE(facade->GoForwardInArticleTab(tab_id).error,
             TabOperationError::kNoForwardEntry);
}

void ArticleTabsTest::AppliesReuseAndNewTabPolicies() {
    auto facade = CreateDesktopFacade({});
    const auto existing =
        facade->OpenArticleTab(Lookup("same", 4U), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kKeepActive);
    auto same_navigation = Lookup("same", 4U);
    same_navigation.title = "Updated presentation title";
    const auto reused =
        facade->OpenArticleTab(same_navigation, TabOpenPolicy::kReuseExisting,
                               TabActivationPolicy::kActivate);
    QVERIFY(reused);
    QCOMPARE(reused.tab_id, existing.tab_id);
    QCOMPARE(facade->GetArticleTabsState().tabs.size(), std::size_t{2});
    QCOMPARE(facade->GetArticleTabsState().active_tab_id, existing.tab_id);

    const auto different_group = facade->OpenArticleTab(
        Lookup("same", 5U), TabOpenPolicy::kReuseExisting,
        TabActivationPolicy::kKeepActive);
    const auto duplicate =
        facade->OpenArticleTab(Lookup("same", 4U), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kKeepActive);
    QVERIFY(different_group);
    QVERIFY(duplicate);
    QVERIFY(different_group.tab_id != existing.tab_id);
    QVERIFY(duplicate.tab_id != existing.tab_id);
    QCOMPARE(facade->GetArticleTabsState().tabs.size(), std::size_t{4});
}

void ArticleTabsTest::PreservesInternalLinkState() {
    auto facade = CreateDesktopFacade({});
    TabNavigationState link;
    link.kind = TabNavigationKind::kInternalLink;
    link.query = "linked word";
    link.group_id = 42U;
    link.title = "Linked title";
    link.internal_url = "goldendict://lookup/linked%20word";
    link.source_dictionary_id = "source-dictionary";
    link.source_article_id = "source-article";
    link.target_article_id = "target-article";
    link.target_anchor = "section-2";

    QVERIFY(facade->OpenArticleTab(link, TabOpenPolicy::kCurrentTab,
                                   TabActivationPolicy::kActivate));
    QCOMPARE(facade->GetArticleTabsState().tabs.front().navigation, link);
    QVERIFY(facade->OpenArticleTab(Lookup("next"), TabOpenPolicy::kCurrentTab,
                                   TabActivationPolicy::kActivate));
    const ArticleTabId id = facade->GetArticleTabsState().active_tab_id;
    QVERIFY(facade->GoBackInArticleTab(id));
    QCOMPARE(facade->GetArticleTabsState().tabs.front().navigation, link);
}

void ArticleTabsTest::ExportsAndRestoresCompleteSession() {
    auto facade = CreateDesktopFacade({});
    TabNavigationState link;
    link.kind = TabNavigationKind::kInternalLink;
    link.query = "linked word";
    link.group_id = 42U;
    link.title = "Linked title";
    link.internal_url = "goldendict://lookup/linked%20word";
    link.source_dictionary_id = "source-dictionary";
    link.source_article_id = "source-article";
    link.target_article_id = "target-article";
    link.target_anchor = "section-2";

    ArticleTabSession expected;
    expected.active_tab_id = 42U;
    expected.tabs = {
        {7U, {Lookup("first", 3U), Lookup("second", 4U)}, 0U},
        {42U, {Lookup("other", 8U), link, Lookup("after", 9U)}, 1U}};

    QVERIFY(facade->RestoreArticleTabSession(expected));
    QCOMPARE(facade->ExportArticleTabSession(), expected);
    const auto state = facade->GetArticleTabsState();
    QCOMPARE(state.tabs.size(), std::size_t{2});
    QCOMPARE(state.tabs[0].id, ArticleTabId{7U});
    QCOMPARE(state.tabs[0].navigation.query, "first");
    QVERIFY(state.tabs[0].can_go_forward);
    QCOMPARE(state.tabs[1].navigation, link);
    QVERIFY(state.tabs[1].can_go_back);
    QVERIFY(state.tabs[1].can_go_forward);

    QVERIFY(facade->GoForwardInArticleTab(42U));
    QCOMPARE(facade->GetArticleTabsState().tabs[1].navigation.query, "after");
    QVERIFY(facade->GoBackInArticleTab(42U));
    QVERIFY(facade->OpenArticleTab(Lookup("branch", 10U),
                                   TabOpenPolicy::kCurrentTab,
                                   TabActivationPolicy::kActivate));
    QCOMPARE(facade->ExportArticleTabSession().tabs[1].history.size(),
             std::size_t{3});
    QCOMPARE(facade->GetArticleTabsState().tabs[1].navigation.query, "branch");

    const auto created =
        facade->OpenArticleTab(Lookup("collision-free"), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kKeepActive);
    QVERIFY(created);
    QCOMPARE(created.tab_id, ArticleTabId{43U});
}

void ArticleTabsTest::RejectsInvalidSessionsAtomically() {
    auto facade = CreateDesktopFacade({});
    ArticleTabSession valid;
    valid.active_tab_id = 12U;
    valid.tabs = {{12U, {Lookup("stable")}, 0U}};
    QVERIFY(facade->RestoreArticleTabSession(valid));
    const auto before = facade->ExportArticleTabSession();

    std::vector<ArticleTabSession> invalid = {
        {},
        {{{0U, {Lookup("zero")}, 0U}}, 0U},
        {{{12U, {Lookup("first")}, 0U}, {12U, {Lookup("duplicate")}, 0U}}, 12U},
        {{{12U, {}, 0U}}, 12U},
        {{{12U, {Lookup("cursor")}, 1U}}, 12U},
        {{{12U, {Lookup("missing-active")}, 0U}}, 99U},
        {{{std::numeric_limits<ArticleTabId>::max(), {Lookup("overflow")}, 0U}},
         std::numeric_limits<ArticleTabId>::max()},
    };
    auto bad_navigation = Lookup("bad");
    bad_navigation.kind = static_cast<TabNavigationKind>(99);
    invalid.push_back({{{12U, {bad_navigation}, 0U}}, 12U});
    ArticleTabSession too_many;
    too_many.active_tab_id = 1U;
    for (ArticleTabId id = 1U; id <= kMaximumArticleTabs + 1U; ++id) {
        too_many.tabs.push_back({id, {Lookup("bounded")}, 0U});
    }
    invalid.push_back(std::move(too_many));
    ArticleTabSession too_much_history = valid;
    too_much_history.tabs.front().history.assign(
        kMaximumTabNavigationEntries + 1U, Lookup("bounded"));
    invalid.push_back(std::move(too_much_history));

    for (const auto& candidate : invalid) {
        QCOMPARE(facade->RestoreArticleTabSession(candidate).error,
                 TabOperationError::kInvalidSession);
        QCOMPARE(facade->ExportArticleTabSession(), before);
    }
    const auto created =
        facade->OpenArticleTab(Lookup("next"), TabOpenPolicy::kNewTab,
                               TabActivationPolicy::kKeepActive);
    QVERIFY(created);
    QCOMPARE(created.tab_id, ArticleTabId{13U});
}

void ArticleTabsTest::RejectsLimitsAndInvalidOperationsAtomically() {
    auto facade = CreateDesktopFacade({});
    const auto before = facade->GetArticleTabsState();
    auto invalid = Lookup(std::string(kMaximumLookupTextBytes + 1U, 'x'));
    QCOMPARE(facade
                 ->OpenArticleTab(invalid, TabOpenPolicy::kCurrentTab,
                                  TabActivationPolicy::kActivate)
                 .error,
             TabOperationError::kInvalidNavigation);
    QCOMPARE(facade->GetArticleTabsState().tabs.size(), before.tabs.size());
    invalid = Lookup("invalid");
    invalid.kind = static_cast<TabNavigationKind>(99);
    QCOMPARE(facade
                 ->OpenArticleTab(invalid, TabOpenPolicy::kCurrentTab,
                                  TabActivationPolicy::kActivate)
                 .error,
             TabOperationError::kInvalidNavigation);
    invalid = Lookup(std::string("embedded\0nul", 12U));
    QCOMPARE(facade
                 ->OpenArticleTab(invalid, TabOpenPolicy::kCurrentTab,
                                  TabActivationPolicy::kActivate)
                 .error,
             TabOperationError::kInvalidNavigation);
    invalid = {};
    invalid.group_id = 7U;
    QCOMPARE(facade
                 ->OpenArticleTab(invalid, TabOpenPolicy::kCurrentTab,
                                  TabActivationPolicy::kActivate)
                 .error,
             TabOperationError::kInvalidNavigation);

    constexpr ArticleTabId kMissing = 999999U;
    QCOMPARE(facade->ActivateArticleTab(kMissing).error,
             TabOperationError::kInvalidTabId);
    QCOMPARE(facade->CloseArticleTab(kMissing).error,
             TabOperationError::kInvalidTabId);
    QCOMPARE(facade->CloseOtherArticleTabs(kMissing).error,
             TabOperationError::kInvalidTabId);
    QCOMPARE(facade->GoBackInArticleTab(kMissing).error,
             TabOperationError::kInvalidTabId);

    for (std::size_t index = 1U; index < kMaximumArticleTabs; ++index) {
        QVERIFY(facade->OpenArticleTab(Lookup("tab-" + std::to_string(index)),
                                       TabOpenPolicy::kNewTab,
                                       TabActivationPolicy::kKeepActive));
    }
    QCOMPARE(facade
                 ->OpenArticleTab(Lookup("overflow"), TabOpenPolicy::kNewTab,
                                  TabActivationPolicy::kKeepActive)
                 .error,
             TabOperationError::kTabLimitReached);
    QCOMPARE(facade->GetArticleTabsState().tabs.size(), kMaximumArticleTabs);

    auto bounded = CreateDesktopFacade({});
    const ArticleTabId id = bounded->GetArticleTabsState().active_tab_id;
    for (std::size_t index = 1U; index < kMaximumTabNavigationEntries;
         ++index) {
        QVERIFY(bounded->OpenArticleTab(
            Lookup("entry-" + std::to_string(index)),
            TabOpenPolicy::kCurrentTab, TabActivationPolicy::kActivate));
    }
    QCOMPARE(
        bounded
            ->OpenArticleTab(Lookup("overflow"), TabOpenPolicy::kCurrentTab,
                             TabActivationPolicy::kActivate)
            .error,
        TabOperationError::kNavigationLimitReached);
    QCOMPARE(bounded->GetArticleTabsState().tabs.front().navigation.query,
             "entry-99");
    QVERIFY(bounded->GoBackInArticleTab(id));
    QVERIFY(bounded->OpenArticleTab(Lookup("bounded-branch"),
                                    TabOpenPolicy::kCurrentTab,
                                    TabActivationPolicy::kActivate));
}

}  // namespace
}  // namespace goldendict::core

QTEST_APPLESS_MAIN(goldendict::core::ArticleTabsTest)

#include "article_tabs_test.moc"
