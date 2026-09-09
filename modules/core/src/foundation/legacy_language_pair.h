// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_CORE_SRC_FOUNDATION_LEGACY_LANGUAGE_PAIR_H_
#define GOLDENDICT_CORE_SRC_FOUNDATION_LEGACY_LANGUAGE_PAIR_H_

#include <string>
#include <utility>

namespace goldendict::core::foundation {

// Frozen LangCoder name-token semantics. Callers own filename/title precedence.
std::pair<std::string, std::string> InferLegacyLanguagePair(std::string name);

}  // namespace goldendict::core::foundation

#endif
