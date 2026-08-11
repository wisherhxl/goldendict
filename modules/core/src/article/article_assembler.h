// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_ARTICLE_ARTICLE_ASSEMBLER_H_
#define GOLDENDICT_CORE_SRC_ARTICLE_ARTICLE_ASSEMBLER_H_

#include <string>
#include <vector>

#include "../dictionary/dictionary_backend.h"

namespace goldendict::core::article {

struct ResourceReference {
    std::string dictionary_id;
    std::string resource_id;
};

struct Document {
    std::string plain_text;
    std::string sanitized_html;
    std::vector<ResourceReference> resources;
};

Document Assemble(const dictionary::Identity& dictionary,
                  const std::vector<dictionary::Article>& articles);

}  // namespace goldendict::core::article

#endif  // GOLDENDICT_CORE_SRC_ARTICLE_ARTICLE_ASSEMBLER_H_
