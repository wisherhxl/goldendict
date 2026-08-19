// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_FORMATS_AARD_AARD_DICTIONARY_H_
#define GOLDENDICT_CORE_SRC_FORMATS_AARD_AARD_DICTIONARY_H_
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "../../dictionary/dictionary_backend.h"
#include "../../dictionary/full_text_index_lifecycle.h"
#include "../../dictionary/full_text_index_snapshot.h"
#include "aard_reader.h"

namespace goldendict::core::formats::aard {
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

    const std::shared_ptr<dictionary::FullTextIndexFormatWorkPort>&
    full_text_work_port() const noexcept {
        return full_text_work_port_;
    }

    const std::shared_ptr<dictionary::FullTextIndexSnapshotHolder>&
    full_text_snapshot_holder() const noexcept {
        return full_text_snapshot_holder_;
    }

    std::optional<dictionary::ResolvedFullTextDocument> ResolveFullTextDocument(
        std::string_view document_id) const override {
        const auto snapshot = full_text_snapshot_holder_->Acquire();
        return snapshot != nullptr ? snapshot->ResolveDocument(document_id)
                                   : std::nullopt;
    }

    bool IsFullTextIndexAvailable() const noexcept override {
        return full_text_snapshot_holder_->Acquire() != nullptr;
    }

    std::optional<dictionary::FullTextIndexState> full_text_index_state()
        const noexcept {
        const auto snapshot = full_text_snapshot_holder_->Acquire();
        return snapshot != nullptr ? std::optional(snapshot->state())
                                   : std::nullopt;
    }

   private:
    dictionary::Identity identity_;
    std::shared_ptr<const Reader> reader_;
    std::shared_ptr<dictionary::FullTextIndexSnapshotHolder>
        full_text_snapshot_holder_;
    std::shared_ptr<dictionary::FullTextIndexFormatWorkPort>
        full_text_work_port_;
    std::optional<FullTextError> full_text_error_;
};
}  // namespace goldendict::core::formats::aard
#endif
