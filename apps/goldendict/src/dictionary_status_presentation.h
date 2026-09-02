// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APP_DICTIONARY_STATUS_PRESENTATION_H_
#define GOLDENDICT_APP_DICTIONARY_STATUS_PRESENTATION_H_

#include <vector>

#include <QString>

#include "goldendict/core/dictionary_service.h"

namespace goldendict::app {

QString FormatDictionaryCatalogStatus(
    const std::vector<goldendict::core::DictionaryIdentity>& catalog);

}  // namespace goldendict::app

#endif  // GOLDENDICT_APP_DICTIONARY_STATUS_PRESENTATION_H_
