// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_dictionary_projection.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace goldendict::app {
namespace {

bool Contains(const std::vector<std::string>& ids, const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

}  // namespace

goldendict::core::FullTextQuery ProjectFullTextDictionaries(
    goldendict::core::FullTextQuery query,
    const std::vector<goldendict::core::DictionaryIdentity>& catalog,
    const goldendict::core::DictionaryGroupConfiguration* selected_group,
    bool dictionary_bar_visible,
    const std::vector<std::string>& participating_dictionary_ids) {
    query.dictionary_ids.clear();
    query.dictionary_filter_active = true;

    std::map<std::string, const goldendict::core::DictionaryIdentity*>
        identities;
    for (const auto& identity : catalog) {
        identities.emplace(identity.id, &identity);
    }

    const auto retain = [&](const std::string& id) {
        const auto identity = identities.find(id);
        return identity != identities.end() &&
               identity->second->supports_full_text_search &&
               (!dictionary_bar_visible ||
                Contains(participating_dictionary_ids, id));
    };

    if (selected_group == nullptr) {
        for (const auto& identity : catalog) {
            if (retain(identity.id)) {
                query.dictionary_ids.push_back(identity.id);
            }
        }
        return query;
    }

    for (const auto& id : selected_group->dictionary_ids) {
        if (!Contains(selected_group->muted_dictionary_ids, id) && retain(id)) {
            query.dictionary_ids.push_back(id);
        }
    }
    return query;
}

}  // namespace goldendict::app
