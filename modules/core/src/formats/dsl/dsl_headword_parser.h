// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_HEADWORD_PARSER_H_
#define GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_HEADWORD_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

namespace goldendict::core::formats::dsl::headword {

struct Expansion {
    std::string primary;
    std::string article_tilde;
    std::vector<std::string> records;
};

// Converts one main line and its alternate lines into index-ready headwords.
Expansion Parse(const std::vector<std::string>& source_lines);

std::string ReplaceTildes(std::string value, std::string_view primary);

}  // namespace goldendict::core::formats::dsl::headword

#endif  // GOLDENDICT_CORE_SRC_FORMATS_DSL_DSL_HEADWORD_PARSER_H_
