// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QStandardItemModel>
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
    result.excerpt_byte_offset = ordinal * 100U;
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
    QCOMPARE(actual.excerpt_byte_offset, expected.excerpt_byte_offset);
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
    void PreservesOrderDuplicatesDisplayTooltipsAndMetadata();
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

void FullTextResponseModelTest::
    PreservesOrderDuplicatesDisplayTooltipsAndMetadata() {
    goldendict::core::FullTextResponse response;
    response.results = {MakeResult("first", "Straße 日本語 😀", 1U),
                        MakeResult("second", "duplicate", 2U),
                        MakeResult("third", "duplicate", 3U),
                        MakeResult("empty", "empty name", 4U)};
    response.results[0].dictionary.name = "Wörterbuch 日本語 😀";
    response.results[1].dictionary.name = "First dictionary";
    response.results[2].dictionary.name = "Second dictionary";
    response.results[3].dictionary.name.clear();
    const auto expected = response.results;
    FullTextResponseModel model(response);

    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.data(model.index(0, 0)).toString(),
             QString::fromUtf8("Straße 日本語 😀"));
    QCOMPARE(model.data(model.index(1, 0)).toString(),
             QStringLiteral("duplicate"));
    QCOMPARE(model.data(model.index(2, 0)).toString(),
             QStringLiteral("duplicate"));
    QCOMPARE(model.data(model.index(3, 0)).toString(),
             QStringLiteral("empty name"));
    for (int row = 0; row < model.rowCount(); ++row) {
        QCOMPARE(model.data(model.index(row, 0), Qt::EditRole),
                 model.data(model.index(row, 0), Qt::DisplayRole));
    }
    QCOMPARE(model.data(model.index(0, 0), Qt::EditRole).toString(),
             QString::fromUtf8("Straße 日本語 😀"));
    QCOMPARE(model.data(model.index(1, 0), Qt::EditRole).toString(),
             QStringLiteral("duplicate"));
    QCOMPARE(model.data(model.index(2, 0), Qt::EditRole).toString(),
             QStringLiteral("duplicate"));
    QCOMPARE(model.data(model.index(3, 0), Qt::EditRole).toString(),
             QStringLiteral("empty name"));
    QCOMPARE(model.data(model.index(0, 0), Qt::ToolTipRole).toString(),
             QString::fromUtf8("Wörterbuch 日本語 😀"));
    QCOMPARE(model.data(model.index(1, 0), Qt::ToolTipRole).toString(),
             QStringLiteral("First dictionary"));
    QCOMPARE(model.data(model.index(2, 0), Qt::ToolTipRole).toString(),
             QStringLiteral("Second dictionary"));
    QVERIFY(!model.data(model.index(3, 0), Qt::ToolTipRole).isValid());
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
    replacement.results[0].dictionary.name = "New dictionary";
    model.Reset(replacement);

    QCOMPARE(reset_spy.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.ResultAt(model.index(0, 0))->dictionary.id,
             std::string("new"));
    QCOMPARE(model.data(model.index(0, 0), Qt::EditRole).toString(),
             QStringLiteral("new row"));
    QCOMPARE(model.data(model.index(0, 0), Qt::ToolTipRole).toString(),
             QStringLiteral("New dictionary"));
}

void FullTextResponseModelTest::OwnsCopiedAndMovedResponseLifetimes() {
    goldendict::core::FullTextResponse copied_source;
    copied_source.results.push_back(MakeResult("copy", "copy source", 1U));
    copied_source.results[0].dictionary.name = "Copied dictionary";
    FullTextResponseModel copied(copied_source);
    copied_source.results[0].headword = "mutated";
    copied_source.results[0].dictionary.name = "Mutated dictionary";
    copied_source.results.clear();
    QCOMPARE(copied.data(copied.index(0, 0)).toString(),
             QStringLiteral("copy source"));
    QCOMPARE(copied.data(copied.index(0, 0), Qt::EditRole).toString(),
             QStringLiteral("copy source"));
    QCOMPARE(copied.data(copied.index(0, 0), Qt::ToolTipRole).toString(),
             QStringLiteral("Copied dictionary"));

    FullTextResponseModel moved([] {
        goldendict::core::FullTextResponse temporary;
        temporary.results.push_back(MakeResult("move", "move source", 2U));
        temporary.results[0].dictionary.name = "Moved dictionary";
        return temporary;
    }());
    QCOMPARE(moved.data(moved.index(0, 0)).toString(),
             QStringLiteral("move source"));
    QCOMPARE(moved.data(moved.index(0, 0), Qt::EditRole).toString(),
             QStringLiteral("move source"));
    QCOMPARE(moved.data(moved.index(0, 0), Qt::ToolTipRole).toString(),
             QStringLiteral("Moved dictionary"));

    moved.Reset([] {
        goldendict::core::FullTextResponse temporary;
        temporary.results.push_back(MakeResult("reset", "reset move", 3U));
        temporary.results[0].dictionary.name = "Reset dictionary";
        return temporary;
    }());
    QCOMPARE(moved.data(moved.index(0, 0)).toString(),
             QStringLiteral("reset move"));
    QCOMPARE(moved.data(moved.index(0, 0), Qt::EditRole).toString(),
             QStringLiteral("reset move"));
    QCOMPARE(moved.data(moved.index(0, 0), Qt::ToolTipRole).toString(),
             QStringLiteral("Reset dictionary"));
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
        QCOMPARE(first.data(first.index(row, 0), Qt::EditRole),
                 second.data(second.index(row, 0), Qt::EditRole));
        QCOMPARE(first.data(first.index(row, 0), Qt::ToolTipRole),
                 second.data(second.index(row, 0), Qt::ToolTipRole));
        CompareResult(*first.ResultAt(first.index(row, 0)),
                      *second.ResultAt(second.index(row, 0)));
    }
}

void FullTextResponseModelTest::HandlesInvalidIndexesAndRoles() {
    goldendict::core::FullTextResponse response;
    response.results.push_back(MakeResult("one", "headword", 1U));
    FullTextResponseModel model(response);
    FullTextResponseModel other(response);
    QStandardItemModel foreign(1, 2);

    QVERIFY(!model.data(QModelIndex()).isValid());
    QVERIFY(!model.data(QModelIndex(), Qt::EditRole).isValid());
    QVERIFY(!model.data(other.index(0, 0), Qt::EditRole).isValid());
    QVERIFY(!model.data(model.index(1, 0), Qt::EditRole).isValid());
    QVERIFY(!model.data(model.index(0, 1), Qt::EditRole).isValid());
    QVERIFY(!model.data(foreign.index(0, 1), Qt::EditRole).isValid());
    QVERIFY(!model.data(other.index(0, 0), Qt::ToolTipRole).isValid());
    QVERIFY(!model.data(model.index(1, 0), Qt::ToolTipRole).isValid());
    QVERIFY(!model.data(model.index(0, 1), Qt::ToolTipRole).isValid());
    QVERIFY(!model.data(foreign.index(0, 1), Qt::ToolTipRole).isValid());
    QVERIFY(!model.data(model.index(0, 0), Qt::UserRole).isValid());
    QVERIFY(model.ResultAt(QModelIndex()) == nullptr);
    QVERIFY(model.ResultAt(other.index(0, 0)) == nullptr);
    QVERIFY(!model.index(1, 0).isValid());
    QCOMPARE(model.rowCount(model.index(0, 0)), 0);
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::FullTextResponseModelTest)

#include "full_text_response_model_test.moc"
