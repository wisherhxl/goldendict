// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QtTest>

#include <cstddef>
#include <string>

#include "full_text_response_model.h"

namespace goldendict::app {
namespace {

goldendict::core::FullTextResult MakeResult(const std::string& id,
                                            const std::string& headword,
                                            std::size_t ordinal) {
    goldendict::core::FullTextResult result;
    result.dictionary.id = id;
    result.dictionary.name = "Dictionary " + id;
    result.dictionary.edition = "edition-" + std::to_string(ordinal);
    result.dictionary.source = "/source/" + id;
    result.dictionary.description = "description-" + id;
    result.dictionary.article_count = 100U + ordinal;
    result.dictionary.headword_count = 200U + ordinal;
    result.dictionary.source_language = "en";
    result.dictionary.target_language = "ja";
    result.dictionary.supports_headword_enumeration = ordinal % 2U == 0U;
    result.dictionary.supports_full_text_search = true;
    result.headword = headword;
    result.document_id = "document-" + std::to_string(ordinal);
    result.match.requested_headword = "requested-" + std::to_string(ordinal);
    result.match.normalized_headword = "normalized-" + std::to_string(ordinal);
    result.match.mode = goldendict::core::MatchMode::kPrefix;
    result.match.score = 0.5 + static_cast<double>(ordinal);
    result.excerpt = "excerpt-" + std::to_string(ordinal);
    result.matches = {{ordinal, 3U, "one"}, {ordinal + 10U, 7U, "second"}};
    return result;
}

void CompareResult(const goldendict::core::FullTextResult& actual,
                   const goldendict::core::FullTextResult& expected) {
    QCOMPARE(actual.dictionary.id, expected.dictionary.id);
    QCOMPARE(actual.dictionary.name, expected.dictionary.name);
    QCOMPARE(actual.dictionary.edition, expected.dictionary.edition);
    QCOMPARE(actual.dictionary.source, expected.dictionary.source);
    QCOMPARE(actual.dictionary.description, expected.dictionary.description);
    QCOMPARE(actual.dictionary.article_count,
             expected.dictionary.article_count);
    QCOMPARE(actual.dictionary.headword_count,
             expected.dictionary.headword_count);
    QCOMPARE(actual.dictionary.source_language,
             expected.dictionary.source_language);
    QCOMPARE(actual.dictionary.target_language,
             expected.dictionary.target_language);
    QCOMPARE(actual.dictionary.supports_headword_enumeration,
             expected.dictionary.supports_headword_enumeration);
    QCOMPARE(actual.dictionary.supports_full_text_search,
             expected.dictionary.supports_full_text_search);
    QCOMPARE(actual.headword, expected.headword);
    QCOMPARE(actual.document_id, expected.document_id);
    QCOMPARE(actual.match.requested_headword,
             expected.match.requested_headword);
    QCOMPARE(actual.match.normalized_headword,
             expected.match.normalized_headword);
    QCOMPARE(static_cast<int>(actual.match.mode),
             static_cast<int>(expected.match.mode));
    QCOMPARE(actual.match.score, expected.match.score);
    QCOMPARE(actual.excerpt, expected.excerpt);
    QCOMPARE(actual.matches.size(), expected.matches.size());
    for (std::size_t i = 0; i < expected.matches.size(); ++i) {
        QCOMPARE(actual.matches[i].byte_offset,
                 expected.matches[i].byte_offset);
        QCOMPARE(actual.matches[i].byte_length,
                 expected.matches[i].byte_length);
        QCOMPARE(actual.matches[i].text, expected.matches[i].text);
    }
}

}  // namespace

class FullTextResponseModelTest final : public QObject {
    Q_OBJECT

   private slots:
    void ProjectsEmptyErrorAndPartialResponses();
    void PreservesOrderDuplicatesDisplayAndMetadata();
    void ResetReplacesSnapshotAtomically();
    void OwnsCopiedAndMovedResponseLifetimes();
    void RepeatedProjectionIsDeterministic();
    void HandlesInvalidIndexesAndRoles();
};

void FullTextResponseModelTest::ProjectsEmptyErrorAndPartialResponses() {
    FullTextResponseModel model;
    QCOMPARE(model.rowCount(), 0);

    goldendict::core::FullTextResponse error;
    error.errors.push_back({goldendict::core::FullTextErrorCode::kInternal,
                            "dictionary", "contained error"});
    model.Reset(error);
    QCOMPARE(model.rowCount(), 0);

    goldendict::core::FullTextResponse partial;
    partial.partial = true;
    model.Reset(partial);
    QCOMPARE(model.rowCount(), 0);

    partial.results.push_back(MakeResult("partial", "retained", 1U));
    model.Reset(partial);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0)).toString(),
             QStringLiteral("retained"));
}

void FullTextResponseModelTest::PreservesOrderDuplicatesDisplayAndMetadata() {
    goldendict::core::FullTextResponse response;
    response.results = {MakeResult("first", "Straße 日本語 😀", 1U),
                        MakeResult("second", "duplicate", 2U),
                        MakeResult("third", "DUPLICATE", 3U)};
    const auto expected = response.results;
    FullTextResponseModel model(response);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0, 0)).toString(),
             QString::fromUtf8("Straße 日本語 😀"));
    QCOMPARE(model.data(model.index(1, 0)).toString(),
             QStringLiteral("duplicate"));
    QCOMPARE(model.data(model.index(2, 0)).toString(),
             QStringLiteral("DUPLICATE"));
    for (int row = 0; row < model.rowCount(); ++row) {
        const auto* actual = model.ResultAt(model.index(row, 0));
        QVERIFY(actual != nullptr);
        CompareResult(*actual, expected[static_cast<std::size_t>(row)]);
    }
}

void FullTextResponseModelTest::ResetReplacesSnapshotAtomically() {
    goldendict::core::FullTextResponse initial;
    initial.results = {MakeResult("old-1", "old one", 1U),
                       MakeResult("old-2", "old two", 2U)};
    FullTextResponseModel model(initial);
    QSignalSpy reset_spy(&model, &QAbstractItemModel::modelReset);

    goldendict::core::FullTextResponse replacement;
    replacement.results = {MakeResult("new", "new row", 3U)};
    model.Reset(replacement);

    QCOMPARE(reset_spy.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.ResultAt(model.index(0, 0))->dictionary.id,
             std::string("new"));
}

void FullTextResponseModelTest::OwnsCopiedAndMovedResponseLifetimes() {
    goldendict::core::FullTextResponse copied_source;
    copied_source.results.push_back(MakeResult("copy", "copy source", 1U));
    FullTextResponseModel copied(copied_source);
    copied_source.results[0].headword = "mutated";
    copied_source.results.clear();
    QCOMPARE(copied.data(copied.index(0, 0)).toString(),
             QStringLiteral("copy source"));

    FullTextResponseModel moved([] {
        goldendict::core::FullTextResponse temporary;
        temporary.results.push_back(MakeResult("move", "move source", 2U));
        return temporary;
    }());
    QCOMPARE(moved.data(moved.index(0, 0)).toString(),
             QStringLiteral("move source"));

    moved.Reset([] {
        goldendict::core::FullTextResponse temporary;
        temporary.results.push_back(MakeResult("reset", "reset move", 3U));
        return temporary;
    }());
    QCOMPARE(moved.data(moved.index(0, 0)).toString(),
             QStringLiteral("reset move"));
}

void FullTextResponseModelTest::RepeatedProjectionIsDeterministic() {
    goldendict::core::FullTextResponse response;
    response.results = {MakeResult("one", "same", 1U),
                        MakeResult("two", "same", 2U)};
    FullTextResponseModel first(response);
    FullTextResponseModel second(response);
    second.Reset(response);

    QCOMPARE(first.rowCount(), second.rowCount());
    for (int row = 0; row < first.rowCount(); ++row) {
        QCOMPARE(first.data(first.index(row, 0)),
                 second.data(second.index(row, 0)));
        CompareResult(*first.ResultAt(first.index(row, 0)),
                      *second.ResultAt(second.index(row, 0)));
    }
}

void FullTextResponseModelTest::HandlesInvalidIndexesAndRoles() {
    goldendict::core::FullTextResponse response;
    response.results.push_back(MakeResult("one", "headword", 1U));
    FullTextResponseModel model(response);
    FullTextResponseModel other(response);

    QVERIFY(!model.data(QModelIndex()).isValid());
    QVERIFY(!model.data(model.index(0, 0), Qt::UserRole).isValid());
    QVERIFY(model.ResultAt(QModelIndex()) == nullptr);
    QVERIFY(model.ResultAt(other.index(0, 0)) == nullptr);
    QVERIFY(!model.index(1, 0).isValid());
    QCOMPARE(model.rowCount(model.index(0, 0)), 0);
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::FullTextResponseModelTest)

#include "full_text_response_model_test.moc"
