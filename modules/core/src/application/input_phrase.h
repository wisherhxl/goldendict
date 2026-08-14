// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_APPLICATION_INPUT_PHRASE_H_
#define GOLDENDICT_CORE_SRC_APPLICATION_INPUT_PHRASE_H_

#include <string_view>

#include "../foundation/utf8.h"
#include "goldendict/core/application.h"

namespace goldendict::core::application {

inline bool IsInputPhraseAccepted(
    std::string_view phrase,
    const ApplicationPreferences& preferences) noexcept {
    return !preferences.limit_input_phrase_length ||
           foundation::Utf8CodePointCount(phrase) <=
               preferences.input_phrase_length_limit;
}

}  // namespace goldendict::core::application

#endif  // GOLDENDICT_CORE_SRC_APPLICATION_INPUT_PHRASE_H_
