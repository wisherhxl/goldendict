// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/formats/aard/aard_dictionary.h"
#include <QtTest>
#include <filesystem>
#include <fstream>
#include <thread>
#include "support/aard_fixture.h"

namespace goldendict::core::formats::aard {
namespace {
class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

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

class AardDictionaryTest : public QObject {
    Q_OBJECT
   private slots:
    void ExposesIdentityHtmlAndSuggestions();
    void RejectsCancellationAndHasNoResources();
    void BuildsAndRebuildsUniqueArticleFullTextIndex();
    void ContainsFullTextFailures();
    void BuildsCandidatesAndPublishesOnlyThroughCoordinator();
};

void AardDictionaryTest::ExposesIdentityHtmlAndSuggestions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "aard-id", test::WriteAardFixture(
                       std::filesystem::path(directory.path().toStdString())));
    QCOMPARE(dictionary.identity().name, "Fixture Aard");
    QVERIFY(dictionary.identity().supports_headword_enumeration);
    QCOMPARE(dictionary.EnumerateHeadwords(0U).headwords,
             (std::vector<std::string>{"alias", "example", "redirect"}));
    QCOMPARE(dictionary.identity().description, "fixture");
    QCOMPARE(dictionary.identity().source_language, "en");
    QCOMPARE(dictionary.identity().target_language, "de");
    QCOMPARE(dictionary.LookupExact("example").front().format, "text/html");
    QCOMPARE(dictionary.LookupExact("alias").size(), std::size_t{1});
    QCOMPARE(dictionary.SuggestPrefix("exa").front(), "example");
}

void AardDictionaryTest::RejectsCancellationAndHasNoResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Dictionary dictionary = Dictionary::Open(
        "aard-id", test::WriteAardFixture(
                       std::filesystem::path(directory.path().toStdString())));
    CancelledSignal signal;
    dictionary::RequestOptions options;
    options.cancellation = &signal;
    QVERIFY_EXCEPTION_THROWN(dictionary.LookupExact("example", options),
                             dictionary::Error);
    QVERIFY(!dictionary.GetResource("missing.png").has_value());
}

void AardDictionaryTest::BuildsAndRebuildsUniqueArticleFullTextIndex() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    auto path = test::WriteAardFixture(root, "fixture.aar", false, false, false,
                                       "[\"<b>visible searchable definition</b>"
                                       "<a href=\\\"w:alias\\\">label</a>\"]");
    const auto index = root / "fixture.gdfts";
    const Dictionary created = Dictionary::Open("aard-id", path, index);
    QCOMPARE(created.full_text_index_state(),
             std::optional(dictionary::FullTextIndexState::kCreated));
    const auto verify_startup_evidence =
        [&](const Dictionary& aard,
            dictionary::FullTextIndexState expected_state) {
            QCOMPARE(aard.full_text_index_state(),
                     std::optional(expected_state));
            const auto snapshot = aard.full_text_snapshot_holder()->Acquire();
            const auto canonical = [&] {
                std::ifstream input(index, std::ios::binary);
                return std::string(std::istreambuf_iterator<char>(input),
                                   std::istreambuf_iterator<char>());
            }();
            const dictionary::FullTextIndexWorkIdentity identity{7U, "aard-id"};
            const auto evidence = aard.StartupArtifactEvidence(identity);
            QVERIFY(evidence.has_value());
            QCOMPARE(evidence->identity, identity);
            QCOMPARE(evidence->source_revision,
                     aard.full_text_work_port()->FullTextIndexSourceRevision());
            QCOMPARE(evidence->snapshot, snapshot);
            QCOMPARE(aard.full_text_snapshot_holder()->Acquire(), snapshot);
            dictionary::FullTextIndexLifecycleCoordinator coordinator;
            QVERIFY(coordinator.RegisterDictionary(
                {"aard-id", "AARD", aard.identity().article_count},
                aard.full_text_work_port(), aard.full_text_snapshot_holder()));
            QVERIFY(coordinator.ApplyPolicyToRegisteredEntries({}));
            const auto requested = coordinator.Snapshot("aard-id");
            QVERIFY(requested.has_value());
            QCOMPARE(requested->state(),
                     dictionary::FullTextIndexLifecycleState::kWorkRequested);
            const auto bound =
                aard.StartupArtifactEvidence(requested->identity());
            QVERIFY(bound.has_value());
            QVERIFY(coordinator.ReconcileStartupArtifact(*bound));
            QCOMPARE(coordinator.Snapshot("aard-id")->identity(),
                     requested->identity());
            QCOMPARE(coordinator.Snapshot("aard-id")->state(),
                     dictionary::FullTextIndexLifecycleState::kCurrent);
            QVERIFY(!coordinator.ReconcileStartupArtifact(*bound));
            QCOMPARE(aard.full_text_snapshot_holder()->Acquire(), snapshot);
            std::ifstream unchanged(index, std::ios::binary);
            QCOMPARE(std::string(std::istreambuf_iterator<char>(unchanged),
                                 std::istreambuf_iterator<char>()),
                     canonical);
        };
    verify_startup_evidence(created, dictionary::FullTextIndexState::kCreated);
    FullTextQuery query;
    query.text = "searchable";
    const auto response = created.SearchFullText(query);
    QCOMPARE(response.results.size(), std::size_t{1});
    QCOMPARE(response.results.front().headword, std::string("example"));
    QCOMPARE(response.results.front().document_id,
             std::string("aard-index:0:0"));
    for (const std::string excluded : {"alias", "bword", "Fixture"}) {
        query.text = excluded;
        QVERIFY2(created.SearchFullText(query).results.empty(),
                 excluded.c_str());
    }
    query.text = "example";
    const auto redirect = created.SearchFullText(query);
    QCOMPARE(redirect.results.size(), std::size_t{1});
    QCOMPARE(redirect.results.front().document_id,
             std::string("aard-index:2:1"));
    const Dictionary reused = Dictionary::Open("aard-id", path, index);
    verify_startup_evidence(reused, dictionary::FullTextIndexState::kReused);
    path = test::WriteAardFixture(root, "fixture.aar", true, true);
    const Dictionary rebuilt_stale = Dictionary::Open("aard-id", path, index);
    verify_startup_evidence(rebuilt_stale,
                            dictionary::FullTextIndexState::kRebuiltStale);
    std::ofstream(index, std::ios::binary | std::ios::trunc) << "corrupt";
    const Dictionary rebuilt_corrupt = Dictionary::Open("aard-id", path, index);
    verify_startup_evidence(rebuilt_corrupt,
                            dictionary::FullTextIndexState::kRebuiltCorrupt);
    const Dictionary disabled = Dictionary::Open("aard-id", path);
    QVERIFY(!disabled.StartupArtifactEvidence({8U, "aard-id"}).has_value());
    QCOMPARE(disabled.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kUnsupported);
}

void AardDictionaryTest::ContainsFullTextFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteAardFixture(root);
    const Dictionary dictionary =
        Dictionary::Open("aard-id", path, root / "fixture.gdfts");
    FullTextQuery query;
    query.text = "definition";
    FullTextCancelledToken cancelled;
    QCOMPARE(dictionary.SearchFullText(query, &cancelled).errors.front().code,
             FullTextErrorCode::kCancelled);
    query.timeout = std::chrono::milliseconds(1);
    SlowFullTextToken slow;
    QCOMPARE(dictionary.SearchFullText(query, &slow).errors.front().code,
             FullTextErrorCode::kDeadlineExceeded);
    const auto blocked = root / "blocked.gdfts";
    QVERIFY(std::filesystem::create_directory(blocked));
    const Dictionary storage = Dictionary::Open("storage-id", path, blocked);
    QVERIFY(!storage.StartupArtifactEvidence({1U, "storage-id"}).has_value());
    QCOMPARE(storage.LookupExact("example").size(), std::size_t{1});
    QCOMPARE(storage.SearchFullText(query).errors.front().code,
             FullTextErrorCode::kInternal);
}

void AardDictionaryTest::BuildsCandidatesAndPublishesOnlyThroughCoordinator() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = std::filesystem::path(directory.path().toStdString());
    const auto path = test::WriteAardFixture(root);
    const auto index_path = root / "fixture.gdfts";
    Dictionary dictionary = Dictionary::Open("aard-id", path, index_path);
    const auto holder = dictionary.full_text_snapshot_holder();
    const auto original = holder->Acquire();
    QVERIFY(original != nullptr);
    const auto port = dictionary.full_text_work_port();
    QVERIFY(port->IsFullTextIndexSupported());
    QVERIFY(!Dictionary::Open("disabled", path)
                 .full_text_work_port()
                 ->IsFullTextIndexSupported());
    const auto revision = port->FullTextIndexSourceRevision();
    QVERIFY(revision.rfind("aard-source-v1:", 0U) == 0U);
    QCOMPARE(port->FullTextIndexSourceRevision(), revision);

    std::ofstream(index_path, std::ios::binary | std::ios::trunc) << "corrupt";
    dictionary::FullTextIndexWorkRequest request;
    request.identity = {1U, "aard-id"};
    request.source_revision = revision;
    request.maximum_documents = 8U;
    request.maximum_document_bytes = 1024U;
    request.maximum_corpus_bytes = 4096U;
    auto candidate = port->PerformFullTextIndexWork(request);
    QCOMPARE(candidate.status, dictionary::FullTextIndexWorkStatus::kCompleted);
    QVERIFY(candidate.replacement_snapshot != nullptr);
    QCOMPARE(holder->Acquire(), original);
    std::ifstream unchanged(index_path, std::ios::binary);
    QCOMPARE(std::string(std::istreambuf_iterator<char>(unchanged),
                         std::istreambuf_iterator<char>()),
             std::string("corrupt"));

    const auto verify_failed =
        [&](dictionary::FullTextIndexWorkRequest failed) {
            const auto result = port->PerformFullTextIndexWork(failed);
            QVERIFY(result.status !=
                    dictionary::FullTextIndexWorkStatus::kCompleted);
            QVERIFY(result.replacement_snapshot == nullptr);
            QCOMPARE(holder->Acquire(), original);
            std::ifstream input(index_path, std::ios::binary);
            QCOMPARE(std::string(std::istreambuf_iterator<char>(input),
                                 std::istreambuf_iterator<char>()),
                     std::string("corrupt"));
        };
    auto invalid = request;
    invalid.maximum_documents = 0U;
    verify_failed(invalid);
    invalid = request;
    invalid.maximum_document_bytes = 0U;
    verify_failed(invalid);
    invalid = request;
    invalid.maximum_corpus_bytes = 0U;
    verify_failed(invalid);
    invalid = request;
    invalid.maximum_documents = 1U;
    verify_failed(invalid);
    invalid = request;
    invalid.maximum_document_bytes = 1U;
    verify_failed(invalid);
    invalid = request;
    invalid.maximum_corpus_bytes = 1U;
    verify_failed(invalid);
    invalid = request;
    invalid.source_revision = "stale";
    verify_failed(invalid);
    invalid = request;
    invalid.deadline = std::chrono::steady_clock::time_point::min();
    verify_failed(invalid);
    FullTextCancelledToken cancelled;
    invalid = request;
    invalid.cancellation = &cancelled;
    verify_failed(invalid);

    dictionary::FullTextIndexLifecycleCoordinator coordinator;
    QVERIFY(
        coordinator.RegisterDictionary({"aard-id", "AARD", 2U}, port, holder));
    dictionary::FullTextIndexPolicy policy;
    QVERIFY(coordinator.SubmitRebuild({{2U, "aard-id"}, policy}));
    request.identity = {2U, "aard-id"};
    request.maximum_documents = 8U;
    QVERIFY(coordinator.ExecuteBoundedWork(request));
    QVERIFY(holder->Acquire() != original);
    QCOMPARE(coordinator.Snapshot("aard-id")->state(),
             dictionary::FullTextIndexLifecycleState::kCurrent);
    QVERIFY(dictionary::FullTextIndex::OpenOrBuild(
                index_path, dictionary::CaptureSourceSnapshot({path}), {})
                .state() == dictionary::FullTextIndexState::kReused);

    const auto published = holder->Acquire();
    const auto canonical = [&] {
        std::ifstream input(index_path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
    }();
    std::ofstream(path, std::ios::binary | std::ios::app) << "drift";
    request.source_revision = revision;
    const auto drifted = port->PerformFullTextIndexWork(request);
    QCOMPARE(drifted.status, dictionary::FullTextIndexWorkStatus::kFailed);
    QCOMPARE(holder->Acquire(), published);
    std::ifstream after_drift(index_path, std::ios::binary);
    QCOMPARE(std::string(std::istreambuf_iterator<char>(after_drift),
                         std::istreambuf_iterator<char>()),
             canonical);
}
}  // namespace
}  // namespace goldendict::core::formats::aard

using goldendict::core::formats::aard::AardDictionaryTest;
QTEST_APPLESS_MAIN(AardDictionaryTest)
#include "aard_dictionary_test.moc"
