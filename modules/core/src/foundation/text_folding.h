// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_FOLDING_H_
#define GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_FOLDING_H_

#include <stdexcept>
#include <string>
#include <string_view>

namespace goldendict::core::foundation {

class TextFoldingError final : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

std::string FoldForLookup(std::string_view text);

}  // namespace goldendict::core::foundation

#endif  // GOLDENDICT_CORE_SRC_FOUNDATION_TEXT_FOLDING_H_
