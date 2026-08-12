// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/desktop_facade.h"

#include <memory>

#include "../article/article_composer.h"
#include "../article/internal_url.h"
#include "../dictionary/dictionary_backend.h"
#include "goldendict/core/application.h"

namespace goldendict::core {
namespace {

class DesktopFacadeImpl final : public DesktopFacade {
   public:
    explicit DesktopFacadeImpl(const CoreConfiguration& configuration)
        : service_(CreateDictionaryService(configuration)) {}

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

   private:
    std::unique_ptr<DictionaryService> service_;
};

}  // namespace

DesktopFacade::~DesktopFacade() = default;

std::unique_ptr<DesktopFacade> CreateDesktopFacade(
    const CoreConfiguration& configuration) {
    return std::make_unique<DesktopFacadeImpl>(configuration);
}

}  // namespace goldendict::core
