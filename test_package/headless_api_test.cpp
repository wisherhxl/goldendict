// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdlib>
#include <iostream>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace {

class EmptyDictionaryService final
    : public goldendict::core::DictionaryService {
   public:
    std::vector<goldendict::core::DictionaryIdentity> GetCatalog()
        const override {
        return {};
    }

    goldendict::core::LookupResponse Lookup(
        const goldendict::core::LookupQuery& query,
        const goldendict::core::CancellationToken* cancellation)
        const override {
        static_cast<void>(query);
        static_cast<void>(cancellation);
        return {};
    }

    std::unique_ptr<goldendict::core::LookupRequest> StartLookup(
        goldendict::core::LookupQuery query) const override {
        static_cast<void>(query);
        return {};
    }

    std::vector<std::byte> GetResource(
        const goldendict::core::ResourceReference& resource,
        const goldendict::core::CancellationToken* cancellation)
        const override {
        static_cast<void>(resource);
        static_cast<void>(cancellation);
        return {};
    }
};

}  // namespace

int main() {
    const EmptyDictionaryService service;
    const goldendict::core::LookupQuery query;

    if (!service.GetCatalog().empty() || query.result_limit == 0) {
        return EXIT_FAILURE;
    }
    const auto packaged_service = goldendict::core::CreateDictionaryService({});
    if (!packaged_service->GetCatalog().empty()) {
        return EXIT_FAILURE;
    }

    std::cout
        << "headless_api_test: goldendict::core API linked successfully\n";
    return EXIT_SUCCESS;
}
