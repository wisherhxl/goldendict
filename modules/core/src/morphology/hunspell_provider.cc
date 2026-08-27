// SPDX-License-Identifier: GPL-3.0-or-later

#include "hunspell_provider.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hunspell/hunspell.hxx>

#include "../foundation/text_encoding.h"
#include "../foundation/utf8.h"
#include "hunspell_content.h"

namespace goldendict::core::morphology::hunspell {
namespace {

inline constexpr std::size_t kMaximumExactQueryBytes = 4096U;

std::mutex& EngineMutex() {
    static std::mutex mutex;
    return mutex;
}

dictionary::Error TranslateContentError(const ContentError& error) {
    const auto code = error.code() == ContentErrorCode::kMissingFile
                          ? dictionary::ErrorCode::kUnavailable
                          : dictionary::ErrorCode::kInvalidData;
    return dictionary::Error(code, error.what());
}

class Provider final : public dictionary::Backend {
   public:
    explicit Provider(Content content)
        : encoding_(std::move(content.encoding)),
          engine_(content.files.affix_file.string().c_str(),
                  content.files.dictionary_file.string().c_str()) {
        identity_.id = content.files.dictionary_id;
        identity_.name = content.files.dictionary_id;
        identity_.source = content.files.affix_file.string();
        identity_.headword_count = content.dictionary_entry_count;
    }

    const dictionary::Identity& identity() const noexcept override {
        return identity_;
    }

    std::vector<dictionary::Article> LookupExact(
        std::string_view headword,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        if (options.result_limit == 0U)
            return {};
        if (headword.empty() || headword.size() > kMaximumExactQueryBytes ||
            headword.find('\0') != std::string_view::npos ||
            !foundation::IsValidUtf8(headword)) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    "Invalid Hunspell exact-lookup query");
        }
        if (std::any_of(headword.begin(), headword.end(), [](char character) {
                return character == ' ' || character == '\t' ||
                       character == '\r' || character == '\n';
            })) {
            return {};
        }

        std::string encoded;
        try {
            encoded = foundation::EncodeFromUtf8(headword, encoding_,
                                                 kMaximumExactQueryBytes * 4U);
        } catch (const foundation::TextEncodingError& error) {
            throw dictionary::Error(dictionary::ErrorCode::kInvalidData,
                                    error.what());
        }

        bool accepted = false;
        {
            std::lock_guard<std::mutex> lock(EngineMutex());
            accepted = engine_.spell(encoded) != 0;
        }
        dictionary::CheckRequest(options);
        if (!accepted)
            return {};
        return {{std::string(headword), "text/plain", {}}};
    }

    std::vector<dictionary::Article> LookupPrefix(
        std::string_view /*prefix*/,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        return {};
    }

    std::vector<std::string> SuggestPrefix(
        std::string_view /*prefix*/,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        return {};
    }

    std::optional<dictionary::Resource> GetResource(
        std::string_view /*resource_id*/,
        const dictionary::RequestOptions& options) const override {
        dictionary::CheckRequest(options);
        return std::nullopt;
    }

   private:
    dictionary::Identity identity_;
    std::string encoding_;
    mutable Hunspell engine_;
};

}  // namespace

std::unique_ptr<dictionary::Backend> OpenProvider(const DataFiles& files) {
    try {
        return std::make_unique<Provider>(LoadContent(files));
    } catch (const ContentError& error) {
        throw TranslateContentError(error);
    } catch (const std::exception& error) {
        throw dictionary::Error(
            dictionary::ErrorCode::kInvalidData,
            std::string("Cannot initialize Hunspell: ") + error.what());
    }
}

}  // namespace goldendict::core::morphology::hunspell
