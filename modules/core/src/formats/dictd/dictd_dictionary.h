// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_DICTIONARY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_DICTIONARY_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../dictionary/dictionary_backend.h"
#include "dictd_reader.h"

namespace goldendict::core::formats::dictd {

class Dictionary final : public dictionary::Backend {
   public:
    static Dictionary Open(std::string id,
                           const std::filesystem::path& index_path);

    const dictionary::Identity& identity() const noexcept override {
        return identity_;
    }

    std::vector<dictionary::Article> LookupExact(
        std::string_view headword,
        const dictionary::RequestOptions& options = {}) const override;
    std::vector<dictionary::Article> LookupPrefix(
        std::string_view prefix,
        const dictionary::RequestOptions& options = {}) const override;
    std::vector<std::string> SuggestPrefix(
        std::string_view prefix,
        const dictionary::RequestOptions& options = {}) const override;
    std::optional<dictionary::Resource> GetResource(
        std::string_view resource_id,
        const dictionary::RequestOptions& options = {}) const override;

   private:
    dictionary::Identity identity_;
    Reader reader_;
};

}  // namespace goldendict::core::formats::dictd

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DICTD_DICTD_DICTIONARY_H_
