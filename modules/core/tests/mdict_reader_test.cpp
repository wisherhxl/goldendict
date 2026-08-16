// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "../src/formats/mdict/mdict_reader.h"
#include "support/mdict_fixture.h"

namespace goldendict::core::formats::mdict {

class MdictReaderTest : public QObject {
    Q_OBJECT
   private slots:
    void ReadsStylesRedirectsAndMddResources();
    void DecodesUtf16KeysAndRecords();
    void ExposesImmutableTerminalOwnershipView();
    void CheckpointsAndCancelsOwnershipTraversal();
    void RejectsCorruptionAndUnsupportedEncryption();
};

void MdictReaderTest::ReadsStylesRedirectsAndMddResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Reader reader = Reader::Open(test::WriteMdictFixture(
        std::filesystem::path(directory.path().toStdString())));

    QCOMPARE(reader.metadata().name, "Fixture MDict");
    QCOMPARE(reader.metadata().description, "Fixture description");
    const auto exact = reader.LookupExact("EXAMPLE");
    QCOMPARE(exact.size(), std::size_t{1});
    QVERIFY(exact.front().data.find("<b>definition</b>") != std::string::npos);
    QCOMPARE(reader.LookupExact("alias").front().data, exact.front().data);
    QCOMPARE(reader.LookupPrefix("exa").size(), std::size_t{1});
    QCOMPARE(reader.SuggestPrefix("exa").front(), "example");
    QVERIFY(reader.LookupPrefix("exa", 0U).empty());
    QVERIFY(reader.SuggestPrefix("exa", 0U).empty());
    QVERIFY(reader.Resource("pixel.png") != nullptr);
    QCOMPARE(*reader.Resource("/pixel.png"), "mdict-png");
    QVERIFY(reader.Resource("../pixel.png") == nullptr);
}

void MdictReaderTest::ExposesImmutableTerminalOwnershipView() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto mdx =
        test::WriteMdictContainer(root / "ownership.mdx", "Ownership Fixture",
                                  {{"", "ignored"},
                                   {"first alias", "@@@LINK=terminal"},
                                   {"terminal", "<b>terminal article</b>"},
                                   {"chain", "@@@LINK=first alias"},
                                   {"missing", "@@@LINK=absent"},
                                   {"cycle a", "@@@LINK=cycle b"},
                                   {"cycle b", "@@@LINK=cycle a"},
                                   {"Caf\xC3\xA9", "folded winner"},
                                   {"cafe", "folded collision"},
                                   {"folded alias", "@@@LINK=CAFE"},
                                   {"same one", "equal bytes"},
                                   {"same two", "equal bytes"},
                                   {"empty", ""},
                                   {"duplicate alias", "@@@LINK=terminal"},
                                   {"duplicate alias", "@@@LINK=terminal"}});
    const auto mdd = test::WriteMdictContainer(
        root / "ownership.mdd", "Resources", {{"\\hidden.txt", "secret"}});
    const Reader reader = Reader::Open({mdx, {mdd}});
    const auto view = reader.ReadIngestionView();

    QCOMPARE(view.records.size(), std::size_t{14});
    QCOMPARE(view.articles.size(), std::size_t{5});
    QCOMPARE(view.source_snapshot.size(), std::size_t{2});

    const auto& first = view.articles[0];
    QCOMPARE(first.headword, "first alias");
    QCOMPARE(first.first_record_ordinal, std::size_t{0});
    QCOMPARE(first.article_ordinal, std::size_t{0});
    QCOMPARE(first.terminal.terminal_key_ordinal, std::size_t{1});
    QVERIFY(first.terminal.record_offset >= std::size_t{10});
    QCOMPARE(first.terminal.record_size,
             std::string("<b>terminal article</b>").size());
    QVERIFY(first.aliases ==
            (std::vector<std::string>{"terminal", "chain", "duplicate alias",
                                      "duplicate alias"}));

    QCOMPARE(view.records[0].outcome, ResolutionOutcome::kTerminal);
    QCOMPARE(view.records[2].outcome, ResolutionOutcome::kTerminal);
    QCOMPARE(view.records[3].outcome, ResolutionOutcome::kMissingTarget);
    QVERIFY(!view.records[3].terminal.has_value());
    QCOMPARE(view.records[4].outcome, ResolutionOutcome::kCycle);
    QCOMPARE(view.records[5].outcome, ResolutionOutcome::kCycle);
    QCOMPARE(view.records[3].record_ordinal, std::size_t{3});
    QCOMPARE(view.records[5].record_ordinal, std::size_t{5});

    QCOMPARE(view.articles[1].headword, "Caf\xC3\xA9");
    QCOMPARE(view.articles[2].headword, "cafe");
    QVERIFY(view.articles[1].aliases ==
            (std::vector<std::string>{"folded alias"}));
    QCOMPARE(view.articles[3].html, view.articles[4].html);
    QVERIFY(!(view.articles[3].terminal == view.articles[4].terminal));
    QCOMPARE(view.articles[3].article_ordinal, std::size_t{3});
    QCOMPARE(view.articles[4].article_ordinal, std::size_t{4});

    QCOMPARE(view.records[11].outcome, ResolutionOutcome::kTerminal);
    QVERIFY(view.records[11].terminal.has_value());
    QVERIFY(std::none_of(view.articles.begin(), view.articles.end(),
                         [](const IngestionArticle& article) {
                             return article.headword == "empty" ||
                                    article.html.find("secret") !=
                                        std::string::npos;
                         }));
    const std::string future_id =
        "mdict-index:" + std::to_string(view.articles[4].first_record_ordinal) +
        ":" + std::to_string(view.articles[4].article_ordinal) + ":" +
        std::to_string(view.articles[4].terminal.terminal_key_ordinal) + ":" +
        std::to_string(view.articles[4].terminal.record_offset) + ":" +
        std::to_string(view.articles[4].terminal.record_size);
    QVERIFY(future_id.find("mdict-index:10:4:10:") == 0U);
}

void MdictReaderTest::CheckpointsAndCancelsOwnershipTraversal() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteMdictContainer(root / "checkpoint.mdx", "Checkpoint Fixture",
                                  {{"terminal", "article"},
                                   {"one", "@@@LINK=terminal"},
                                   {"two", "@@@LINK=one"}});
    const Reader reader = Reader::Open({path, {}});
    std::size_t checkpoints = 0U;
    const auto view =
        reader.ReadIngestionView([&checkpoints]() { ++checkpoints; });
    QCOMPARE(view.records.size(), std::size_t{3});
    QVERIFY(checkpoints >= std::size_t{6});

    checkpoints = 0U;
    QVERIFY_EXCEPTION_THROWN(reader.ReadIngestionView([&checkpoints]() {
        if (++checkpoints == 3U)
            throw std::runtime_error("cancelled");
    }),
                             std::runtime_error);
    QCOMPARE(checkpoints, std::size_t{3});
    QCOMPARE(reader.ReadIngestionView().articles.size(), std::size_t{1});
}

void MdictReaderTest::DecodesUtf16KeysAndRecords() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path =
        test::WriteMdictContainer(root / "utf16.mdx", "UTF16 Fixture",
                                  {{"cafe", "<b>drink</b>"}}, false, true);

    const Reader reader = Reader::Open({path, {}});

    QCOMPARE(reader.LookupExact("CAFE").front().data, "<b>drink</b>");
}

void MdictReaderTest::RejectsCorruptionAndUnsupportedEncryption() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto corrupt = root / "corrupt.mdx";
    std::ofstream(corrupt, std::ios::binary) << "not-mdict";
    QVERIFY_EXCEPTION_THROWN(Reader::Open({corrupt, {}}), Error);

    const auto encrypted = test::WriteMdictContainer(
        root / "encrypted.mdx", "Encrypted Fixture", {{"word", "value"}}, true);
    try {
        (void)Reader::Open({encrypted, {}});
        QFAIL("encrypted MDict fixture was accepted");
    } catch (const Error& error) {
        QCOMPARE(error.code(), ErrorCode::kUnsupported);
    }
}

}  // namespace goldendict::core::formats::mdict

using goldendict::core::formats::mdict::MdictReaderTest;
QTEST_APPLESS_MAIN(MdictReaderTest)
#include "mdict_reader_test.moc"
