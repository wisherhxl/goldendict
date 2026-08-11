// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/desktop_facade.h"

#include <memory>

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
