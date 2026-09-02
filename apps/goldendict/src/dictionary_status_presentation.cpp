// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_status_presentation.h"

#include <QCoreApplication>
#include <QtTypes>

namespace goldendict::app {

QString FormatDictionaryCatalogStatus(
    const std::vector<goldendict::core::DictionaryIdentity>& catalog) {
    qulonglong article_count = 0U;
    qulonglong headword_count = 0U;
    for (const auto& dictionary : catalog) {
        article_count += static_cast<qulonglong>(dictionary.article_count);
        headword_count += static_cast<qulonglong>(dictionary.headword_count);
    }

    return QCoreApplication::translate("MainWindow",
                                       "%1 dictionaries, %2 articles, %3 words")
        .arg(static_cast<qulonglong>(catalog.size()))
        .arg(article_count)
        .arg(headword_count);
}

}  // namespace goldendict::app
