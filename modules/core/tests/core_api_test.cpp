// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <utility>

#include "goldendict/core/desktop_facade.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core {

class NeverCancelled final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return false; }
};

class EmptyDictionaryService final : public DictionaryService {
   public:
    std::vector<DictionaryIdentity> GetCatalog() const override { return {}; }

    LookupResponse Lookup(
        const LookupQuery& query,
        const CancellationToken* cancellation) const override {
        if (cancellation != nullptr &&
            cancellation->IsCancellationRequested()) {
            LookupResponse response;
            LookupError error;
            error.code = LookupErrorCode::kCancelled;
            error.message = "Lookup cancelled";
            response.errors.push_back(std::move(error));
            return response;
        }
        LookupResponse response;
        LookupError error;
        error.code = LookupErrorCode::kDictionaryUnavailable;
        error.message = "No dictionaries for " + query.text;
        response.errors.push_back(std::move(error));
        return response;
    }

    SuggestionResponse Suggest(
        const SuggestionQuery& query,
        const CancellationToken* cancellation) const override {
        static_cast<void>(query);
        static_cast<void>(cancellation);
        return {};
    }

    HeadwordEnumerationPage EnumerateHeadwords(
        const HeadwordEnumerationQuery& query,
        const CancellationToken* cancellation) const override {
        static_cast<void>(query);
        static_cast<void>(cancellation);
        return {};
    }

    std::unique_ptr<LookupRequest> StartLookup(
        LookupQuery query) const override {
        static_cast<void>(query);
        return {};
    }

    std::vector<std::byte> GetResource(
        const ResourceReference& resource,
        const CancellationToken* cancellation) const override {
        static_cast<void>(resource);
        static_cast<void>(cancellation);
        return {};
    }
};

class EmptyDesktopFacade final : public DesktopFacade {
   public:
    DictionaryService& GetDictionaryService() noexcept override {
        return service_;
    }

    const DictionaryService& GetDictionaryService() const noexcept override {
        return service_;
    }

    ArticleContent ComposeLookupPage(
        const LookupResponse& response) const override {
        static_cast<void>(response);
        return {};
    }

    std::optional<ArticleUrl> ResolveArticleUrl(
        const std::string& url) const override {
        static_cast<void>(url);
        return std::nullopt;
    }

    ArticleTabsState GetArticleTabsState() const override { return {}; }

    ArticleTabSession ExportArticleTabSession() const override { return {}; }

    TabOperationResult RestoreArticleTabSession(
        const ArticleTabSession& session) override {
        static_cast<void>(session);
        return {};
    }

    TabOperationResult OpenArticleTab(
        const TabNavigationState& navigation, TabOpenPolicy open_policy,
        TabActivationPolicy activation_policy,
        TabPlacementPolicy placement_policy) override {
        static_cast<void>(navigation);
        static_cast<void>(open_policy);
        static_cast<void>(activation_policy);
        static_cast<void>(placement_policy);
        return {};
    }

    TabOperationResult ActivateArticleTab(ArticleTabId tab_id) override {
        return {TabOperationError::kNone, tab_id};
    }

    TabOperationResult CloseArticleTab(ArticleTabId tab_id) override {
        return {TabOperationError::kNone, tab_id};
    }

    TabOperationResult CloseOtherArticleTabs(ArticleTabId tab_id) override {
        return {TabOperationError::kNone, tab_id};
    }

    TabOperationResult GoBackInArticleTab(ArticleTabId tab_id) override {
        return {TabOperationError::kNone, tab_id};
    }

    TabOperationResult GoForwardInArticleTab(ArticleTabId tab_id) override {
        return {TabOperationError::kNone, tab_id};
    }

   private:
    EmptyDictionaryService service_;
};

class CoreApiTest : public QObject {
    Q_OBJECT

   private slots:
    void LookupQueryHasBoundedDefaults();
    void SuggestionQueryHasBoundedDefaults();
    void HeadlessServiceDoesNotRequireAGuiApplication();
};

void CoreApiTest::LookupQueryHasBoundedDefaults() {
    const LookupQuery query;

    QCOMPARE(query.match_mode, MatchMode::kExact);
    QCOMPARE(query.result_limit, std::size_t{20});
    QCOMPARE(query.timeout, std::chrono::seconds(5));
}

void CoreApiTest::SuggestionQueryHasBoundedDefaults() {
    const SuggestionQuery query;

    QCOMPARE(query.result_limit, std::size_t{20});
    QCOMPARE(query.timeout, std::chrono::seconds(5));
}

void CoreApiTest::HeadlessServiceDoesNotRequireAGuiApplication() {
    const EmptyDictionaryService service;
    const NeverCancelled cancellation;
    LookupQuery query;
    query.text = "example";

    const LookupResponse response = service.Lookup(query, &cancellation);

    QVERIFY(response.entries.empty());
    QCOMPARE(response.errors.size(), std::size_t{1});
    QCOMPARE(response.errors.front().code,
             LookupErrorCode::kDictionaryUnavailable);
    QVERIFY(!response.partial);
}

}  // namespace goldendict::core

using goldendict::core::CoreApiTest;

QTEST_APPLESS_MAIN(CoreApiTest)

#include "core_api_test.moc"
