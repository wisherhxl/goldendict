// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QTranslator>
#include <QtTest>

#include "dictionary_status_presentation.h"

namespace goldendict::app {
namespace {

goldendict::core::DictionaryIdentity Identity(std::size_t article_count,
                                              std::size_t headword_count) {
    goldendict::core::DictionaryIdentity identity;
    identity.article_count = article_count;
    identity.headword_count = headword_count;
    return identity;
}

class StatusTranslator final : public QTranslator {
   public:
    QString translate(const char* context, const char* source_text, const char*,
                      int) const override {
        if (QString::fromLatin1(context) == QStringLiteral("MainWindow") &&
            QString::fromLatin1(source_text) ==
                QStringLiteral("%1 dictionaries, %2 articles, %3 words")) {
            return QStringLiteral("translated %1/%2/%3");
        }
        return {};
    }
};

}  // namespace

class DictionaryStatusPresentationTest final : public QObject {
    Q_OBJECT

   private slots:
    void FormatsEmptyCatalog();
    void AggregatesMultipleDictionaries();
    void UsesLegacyMainWindowTranslationContext();
};

void DictionaryStatusPresentationTest::FormatsEmptyCatalog() {
    QCOMPARE(FormatDictionaryCatalogStatus({}),
             QStringLiteral("0 dictionaries, 0 articles, 0 words"));
}

void DictionaryStatusPresentationTest::AggregatesMultipleDictionaries() {
    const std::vector<goldendict::core::DictionaryIdentity> catalog = {
        Identity(3U, 5U), Identity(7U, 11U), Identity(13U, 17U)};
    QCOMPARE(FormatDictionaryCatalogStatus(catalog),
             QStringLiteral("3 dictionaries, 23 articles, 33 words"));
}

void DictionaryStatusPresentationTest::
    UsesLegacyMainWindowTranslationContext() {
    StatusTranslator translator;
    QVERIFY(QCoreApplication::installTranslator(&translator));
    const std::vector<goldendict::core::DictionaryIdentity> catalog = {
        Identity(19U, 23U), Identity(29U, 31U)};
    QCOMPARE(FormatDictionaryCatalogStatus(catalog),
             QStringLiteral("translated 2/48/54"));
    QVERIFY(QCoreApplication::removeTranslator(&translator));
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::DictionaryStatusPresentationTest)

#include "dictionary_status_presentation_test.moc"
