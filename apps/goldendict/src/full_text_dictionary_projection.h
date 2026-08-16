// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APP_FULL_TEXT_DICTIONARY_PROJECTION_H_
#define GOLDENDICT_APP_FULL_TEXT_DICTIONARY_PROJECTION_H_

#include <string>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::app {

goldendict::core::FullTextQuery ProjectFullTextDictionaries(
    goldendict::core::FullTextQuery query,
    const std::vector<goldendict::core::DictionaryIdentity>& catalog,
    const goldendict::core::DictionaryGroupConfiguration* selected_group,
    bool dictionary_bar_visible,
    const std::vector<std::string>& participating_dictionary_ids);

}  // namespace goldendict::app

#endif  // GOLDENDICT_APP_FULL_TEXT_DICTIONARY_PROJECTION_H_
