// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_ARTICLE_DECODER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_ARTICLE_DECODER_H_

#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::formats::stardict {

using ArticleDecodeCheckpoint = std::function<void()>;

class ArticleDecodeError final : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

std::string DecodeArticleFields(std::string_view record,
                                std::string_view same_type_sequence,
                                std::string_view target_language,
                                const ArticleDecodeCheckpoint& checkpoint = {});

std::string DecodeArticle(std::string_view headword, std::string_view record,
                          std::string_view same_type_sequence,
                          std::string_view source_language,
                          std::string_view target_language,
                          const ArticleDecodeCheckpoint& checkpoint = {});

}  // namespace goldendict::core::formats::stardict

#endif  // GOLDENDICT_CORE_SRC_FORMATS_STARDICT_STARDICT_ARTICLE_DECODER_H_
