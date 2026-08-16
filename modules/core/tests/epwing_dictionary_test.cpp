// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/epwing/epwing_dictionary.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include <thread>
#include "support/epwing_fixture.h"

namespace goldendict::core::formats::epwing {
namespace {
class FullTextCancelledToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class SlowFullTextToken final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return false;
    }
};
}  // namespace

class EpwingDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesBackendContract();
    void BuildsPhysicalArticleFullTextIndex();
    void TracksCompleteSourceSnapshotAndContainsFailures();
};

void EpwingDictionaryTest::ExposesBackendContract() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto dictionary =
        Dictionary::Open("epwing-fixture", test::WriteEpwingFixture(root));
    QCOMPARE(dictionary.identity().name, "Fixture EPWING");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"example", "second"}));
    QVERIFY(dictionary.identity().description.find("Fixture copyright") !=
            std::string::npos);
    QCOMPARE(dictionary.LookupExact("example").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("sec").front(), "second");
    QVERIFY(dictionary.GetResource("FIXTURE/GAIJI/pixel.png").has_value());
}

void EpwingDictionaryTest::BuildsPhysicalArticleFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto catalog = test::WriteEpwingOwnershipFixture(root);
    const auto index = root / "ownership.gdfts";
    const Dictionary dictionary = Dictionary::Open("epwing-id", catalog, index);
    QCOMPARE(dictionary.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));

    FullTextQuery query;
    query.text = "article 0";
    const auto owner = dictionary.SearchFullText(query);
    QCOMPARE(owner.results.size(), std::size_t{1});
    QCOMPARE(owner.results.front().headword, std::string("owner"));
    QCOMPARE(owner.results.front().document_id,
             std::string("epwing-index:0:0:0:5:0"));
    query.text = "same bytes";
    const auto equal = dictionary.SearchFullText(query);
    QCOMPARE(equal.results.size(), std::size_t{2});
    QVERIFY(equal.results[0].document_id != equal.results[1].document_id);
    query.text = "article 10";
    const auto multi_digit = dictionary.SearchFullText(query);
    QCOMPARE(multi_digit.results.size(), std::size_t{1});
    QCOMPARE(multi_digit.results.front().document_id,
             std::string("epwing-index:10:9:0:5:640"));
    for (const std::string excluded :
         {"alias", "resource-a", "a.bin", "Ownership Book", "text/html"}) {
        query.text = excluded;
        QVERIFY2(dictionary.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    QCOMPARE(
        Dictionary::Open("epwing-id", catalog, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kReused));
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    QCOMPARE(
        Dictionary::Open("epwing-id", catalog, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kRebuiltCorrupt));
    const Dictionary disabled = Dictionary::Open("epwing-id", catalog);
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void EpwingDictionaryTest::TracksCompleteSourceSnapshotAndContainsFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto catalog = test::WriteEpwingFixture(root);
    const auto index = root / "fixture.gdfts";
    QCOMPARE(
        Dictionary::Open("epwing-id", catalog, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kCreated));
    QCOMPARE(
        Dictionary::Open("epwing-id", catalog, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kReused));
    test::EpwingWrite(root / "FIXTURE" / "GAIJI" / "pixel.png", "changed");
    QCOMPARE(
        Dictionary::Open("epwing-id", catalog, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    test::EpwingWrite(root / "FIXTURE" / "GAIJI" / "added.bin", "added");
    QCOMPARE(
        Dictionary::Open("epwing-id", catalog, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kRebuiltStale));
    QVERIFY(std::filesystem::remove(root / "FIXTURE" / "GAIJI" / "added.bin"));
    QCOMPARE(
        Dictionary::Open("epwing-id", catalog, index).full_text_index_state(),
        std::optional(dictionary::FullTextIndexState::kRebuiltStale));

    FullTextQuery query;
    query.text = "definition";
    const Dictionary dictionary = Dictionary::Open("epwing-id", catalog, index);
    FullTextCancelledToken cancelled;
    QCOMPARE(dictionary.SearchFullText(query, &cancelled).errors.front().code,
             FullTextErrorCode::kCancelled);
    query.timeout = std::chrono::milliseconds(1);
    SlowFullTextToken slow;
    QCOMPARE(dictionary.SearchFullText(query, &slow).errors.front().code,
             FullTextErrorCode::kDeadlineExceeded);
    const auto blocked = root / "blocked.gdfts";
    QVERIFY(std::filesystem::create_directory(blocked));
    const Dictionary storage = Dictionary::Open("epwing-id", catalog, blocked);
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);
}
}  // namespace goldendict::core::formats::epwing

using goldendict::core::formats::epwing::EpwingDictionaryTest;
QTEST_APPLESS_MAIN(EpwingDictionaryTest)
#include "epwing_dictionary_test.moc"
