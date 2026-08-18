// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_SDICT_SDICT_DICTIONARY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_SDICT_SDICT_DICTIONARY_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../dictionary/dictionary_backend.h"
#include "sdict_reader.h"

namespace goldendict::core::formats::sdict {

class Dictionary final : public dictionary::Backend,
                         public dictionary::FullTextBackend {
   public:
    static Dictionary Open(std::string id,
                           const std::filesystem::path& dictionary_path,
                           const std::optional<std::filesystem::path>&
                               full_text_index_path = std::nullopt);

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
    dictionary::HeadwordPage EnumerateHeadwords(
        std::size_t offset,
        const dictionary::RequestOptions& options = {}) const override;
    std::optional<dictionary::Resource> GetResource(
        std::string_view resource_id,
        const dictionary::RequestOptions& options = {}) const override;
    FullTextResponse SearchFullText(
        const FullTextQuery& query,
        const CancellationToken* cancellation = nullptr) const override;

    std::optional<dictionary::ResolvedFullTextDocument> ResolveFullTextDocument(
        std::string_view document_id) const override {
        return full_text_index_.has_value()
                   ? full_text_index_->ResolveDocument(document_id)
                   : std::nullopt;
    }

    bool IsFullTextIndexAvailable() const noexcept override {
        return full_text_index_.has_value();
    }

    std::optional<dictionary::FullTextIndexState> full_text_index_state()
        const noexcept {
        return full_text_index_.has_value()
                   ? std::optional(full_text_index_->state())
                   : std::nullopt;
    }

   private:
    dictionary::Identity identity_;
    Reader reader_;
    std::optional<dictionary::FullTextIndex> full_text_index_;
    std::optional<FullTextError> full_text_error_;
};

}  // namespace goldendict::core::formats::sdict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_SDICT_SDICT_DICTIONARY_H_
