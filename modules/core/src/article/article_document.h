// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_ARTICLE_DOCUMENT_H_
#define GOLDENDICT_CORE_ARTICLE_DOCUMENT_H_

#include <string>
#include <string_view>

namespace goldendict::core::article {

std::string NewDocument();
void FinishDocument(std::string* document);
std::string_view ExtractDocumentBody(std::string_view document);

}  // namespace goldendict::core::article

#endif  // GOLDENDICT_CORE_ARTICLE_DOCUMENT_H_
