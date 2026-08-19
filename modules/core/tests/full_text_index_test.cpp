// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include "../src/dictionary/full_text_index.h"
#include "../src/dictionary/full_text_index_lifecycle.h"
#include "../src/dictionary/full_text_index_snapshot.h"
#include "../src/dictionary/full_text_matcher.h"
#include "../src/foundation/utf8.h"

namespace goldendict::core::dictionary {
namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("goldendict-full-text-" +
                 std::to_string(QRandomGenerator::global()->generate64()));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    const std::filesystem::path& path() const { return path_; }

   private:
    std::filesystem::path path_;
};

FullTextDocument Document(std::string id, std::string headword,
                          std::string text) {
    FullTextDocument document;
    document.dictionary.id = std::move(id);
    document.dictionary.name = document.dictionary.id;
    document.headword = std::move(headword);
    document.document_id = document.headword + "-article";
    document.plain_text = std::move(text);
    return document;
}

std::shared_ptr<const FullTextIndex> Snapshot(
    const TemporaryDirectory& directory, const std::string& name) {
    return std::make_shared<const FullTextIndex>(FullTextIndex::OpenOrBuild(
        directory.path() / (name + ".gdfts"), {},
        {Document(name, name, name + " snapshot content")}));
}

FullTextIndexRegistrationMetadata Registration(std::string dictionary_id,
                                               std::string format_type = "AARD",
                                               std::size_t article_count = 0U) {
    return {std::move(dictionary_id), std::move(format_type), article_count};
}

bool Register(FullTextIndexLifecycleCoordinator& coordinator,
              FullTextIndexRegistrationMetadata metadata,
              std::shared_ptr<FullTextIndexFormatWorkPort> port,
              std::shared_ptr<FullTextIndexSnapshotHolder> holder =
                  std::make_shared<FullTextIndexSnapshotHolder>()) {
    return coordinator.RegisterDictionary(std::move(metadata), std::move(port),
                                          std::move(holder));
}

class Cancelled final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class FakeFormatWorkPort final : public FullTextIndexFormatWorkPort {
   public:
    enum class Behavior { kComplete, kFail, kThrowStandard, kThrowUnknown };

    bool supported = true;
    std::string source_revision = "revision-1";
    Behavior behavior = Behavior::kComplete;
    std::optional<FullTextIndexWorkRequest> request;
    bool cancellation_observed = false;
    std::size_t invocation_count = 0U;
    mutable std::size_t support_probe_count = 0U;
    mutable std::size_t revision_probe_count = 0U;
    bool throw_revision = false;
    std::shared_ptr<const FullTextIndex> replacement_snapshot;

    bool IsFullTextIndexSupported() const noexcept override {
        ++support_probe_count;
        return supported;
    }

    std::string FullTextIndexSourceRevision() const override {
        ++revision_probe_count;
        if (throw_revision)
            throw std::runtime_error("escaped revision failure");
        return source_revision;
    }

   private:
    FullTextIndexWorkResult DoPerformFullTextIndexWork(
        const FullTextIndexWorkRequest& work_request) override {
        ++invocation_count;
        request = work_request;
        cancellation_observed =
            work_request.cancellation != nullptr &&
            work_request.cancellation->IsCancellationRequested();
        switch (behavior) {
            case Behavior::kComplete:
                return {cancellation_observed
                            ? FullTextIndexWorkStatus::kCancelled
                            : FullTextIndexWorkStatus::kCompleted,
                        {},
                        replacement_snapshot};
            case Behavior::kFail:
                return {FullTextIndexWorkStatus::kFailed, "adapter failure",
                        nullptr};
            case Behavior::kThrowStandard:
                throw std::runtime_error("escaped adapter failure");
            case Behavior::kThrowUnknown:
                throw 7;
        }
        return {};
    }
};

class BlockingFormatWorkPort final : public FullTextIndexFormatWorkPort {
   public:
    enum class Completion { kResult, kThrowStandard, kThrowUnknown };

    bool IsFullTextIndexSupported() const noexcept override { return true; }

    std::string FullTextIndexSourceRevision() const override {
        return source_revision;
    }

    void WaitUntilStarted() {
        std::unique_lock lock(mutex_);
        started_condition_.wait(lock, [this] { return started_; });
    }

    void Finish(FullTextIndexWorkResult result) {
        std::lock_guard lock(mutex_);
        result_ = std::move(result);
        finish_ = true;
        finish_condition_.notify_all();
    }

    void FinishByThrowing(Completion completion) {
        std::lock_guard lock(mutex_);
        completion_ = completion;
        finish_ = true;
        finish_condition_.notify_all();
    }

    bool cancellation_observed() const {
        std::lock_guard lock(mutex_);
        return cancellation_observed_;
    }

    std::string source_revision = "blocking-revision";

   private:
    FullTextIndexWorkResult DoPerformFullTextIndexWork(
        const FullTextIndexWorkRequest& request) override {
        std::unique_lock lock(mutex_);
        request_ = request;
        started_ = true;
        started_condition_.notify_all();
        finish_condition_.wait(lock, [this] { return finish_; });
        cancellation_observed_ =
            request.cancellation != nullptr &&
            request.cancellation->IsCancellationRequested();
        if (completion_ == Completion::kThrowStandard)
            throw std::runtime_error("stale escaped failure");
        if (completion_ == Completion::kThrowUnknown)
            throw 9;
        return result_;
    }

    mutable std::mutex mutex_;
    std::condition_variable started_condition_;
    std::condition_variable finish_condition_;
    bool started_ = false;
    bool finish_ = false;
    bool cancellation_observed_ = false;
    std::optional<FullTextIndexWorkRequest> request_;
    Completion completion_ = Completion::kResult;
    FullTextIndexWorkResult result_{FullTextIndexWorkStatus::kCompleted,
                                    {},
                                    nullptr};
};

}  // namespace

class FullTextIndexTest : public QObject {
    Q_OBJECT
   private slots:
    void Lifecycle();
    void PreparesWithoutPersisting();
    void LifecycleContract();
    void RegistrationMetadataAndEligibility();
    void PolicyExcludedLifecycle();
    void AppliesPolicyToAllRegisteredEntries();
    void ReconcilesValidatedStartupArtifacts();
    void ProjectsBoundedWorkRequests();
    void CoordinatesExplicitLifecycleTransitions();
    void IsolatesAndMonotonicallyReplacesGenerations();
    void CancelsExactWorkIdempotently();
    void SuppressesStaleCompletions();
    void ContainsCoordinatorWorkFailures();
    void PublishesImmutableSnapshots();
    void CoordinatesGenerationAuthorizedSnapshotHandoff();
    void SkipsPublicationForUnsuccessfulOffSideWork();
    void QueryModesAndFilters();
    void AppliesIndependentIcuNormalizationPolicies();
    void PreservesNormalizedMatchOriginsAndProgress();
    void PreservesQueryModesAndWordConstraints();
    void ConstructsBoundedMatchCenteredExcerpts();
    void PreservesUtf8MatchAndExcerptBoundaries();
    void ResolvesOpaqueDocumentIdentity();
    void RejectsMalformedAndBoundedWork();
};

void FullTextIndexTest::LifecycleContract() {
    const FullTextIndexPolicy defaults;
    QVERIFY(defaults.enabled);
    QCOMPARE(defaults.maximum_dictionary_articles, 0U);
    QVERIFY(defaults.disabled_format_types.empty());

    FullTextIndexPolicy policy = defaults;
    policy.enabled = false;
    policy.maximum_dictionary_articles = 512U;
    policy.disabled_format_types = "AARD,DSL";
    QVERIFY(policy != defaults);
    QCOMPARE(policy, FullTextIndexPolicy(policy));

    ApplicationPreferences preferences;
    preferences.full_text_search_enabled = false;
    preferences.full_text_maximum_dictionary_articles = 10000000U;
    preferences.full_text_maximum_articles_per_dictionary = 7U;
    preferences.full_text_disabled_types =
        std::string("AARD\0DSL \xce\x94", 11U);
    const auto projected = ProjectFullTextIndexPolicy(preferences);
    QCOMPARE(projected.enabled, false);
    QCOMPARE(projected.maximum_dictionary_articles, 10000000U);
    QCOMPARE(projected.disabled_format_types,
             preferences.full_text_disabled_types);
    preferences.full_text_search_enabled = true;
    preferences.full_text_maximum_dictionary_articles = 1U;
    preferences.full_text_maximum_articles_per_dictionary = 99U;
    preferences.full_text_disabled_types = "changed";
    QCOMPARE(projected.enabled, false);
    QCOMPARE(projected.maximum_dictionary_articles, 10000000U);
    QCOMPARE(projected.disabled_format_types,
             std::string("AARD\0DSL \xce\x94", 11U));

    const FullTextIndexWorkIdentity identity{42U, "dictionary-a"};
    const FullTextIndexWorkIdentity stale_generation{41U, "dictionary-a"};
    const FullTextIndexWorkIdentity stale_dictionary{42U, "dictionary-b"};
    QVERIFY(identity != stale_generation);
    QVERIFY(identity != stale_dictionary);
    QCOMPARE((FullTextIndexRebuildIntent{identity, policy}),
             (FullTextIndexRebuildIntent{identity, policy}));
    QVERIFY(!(FullTextIndexRebuildIntent{identity, policy} ==
              FullTextIndexRebuildIntent{stale_generation, policy}));
    QCOMPARE(FullTextIndexCancelIntent{identity},
             (FullTextIndexCancelIntent{identity}));
    QVERIFY(!(FullTextIndexCancelIntent{identity} ==
              FullTextIndexCancelIntent{stale_dictionary}));

    const FullTextIndexLifecycleSnapshot snapshot{
        identity, FullTextIndexLifecycleState::kWorkRequested, true,
        "revision-1"};
    static_assert(std::is_same_v<decltype(snapshot.identity()),
                                 const FullTextIndexWorkIdentity&>);
    static_assert(std::is_same_v<decltype(snapshot.source_revision()),
                                 const std::string&>);
    QCOMPARE(snapshot.identity(), identity);
    QCOMPARE(snapshot.state(), FullTextIndexLifecycleState::kWorkRequested);
    QVERIFY(snapshot.format_capable());
    QCOMPARE(snapshot.source_revision(), std::string("revision-1"));
    QCOMPARE(snapshot,
             FullTextIndexLifecycleSnapshot(
                 identity, FullTextIndexLifecycleState::kWorkRequested, true,
                 "revision-1"));
    QVERIFY(!(snapshot == FullTextIndexLifecycleSnapshot(
                              stale_generation,
                              FullTextIndexLifecycleState::kWorkRequested, true,
                              "revision-1")));
    QVERIFY(!(snapshot == FullTextIndexLifecycleSnapshot(
                              stale_dictionary,
                              FullTextIndexLifecycleState::kWorkRequested, true,
                              "revision-1")));

    FakeFormatWorkPort port;
    QVERIFY(port.IsFullTextIndexSupported());
    QCOMPARE(port.FullTextIndexSourceRevision(), std::string("revision-1"));
    port.supported = false;
    QVERIFY(!port.IsFullTextIndexSupported());
    port.supported = true;
    port.source_revision = "revision-2";
    QCOMPARE(port.FullTextIndexSourceRevision(), std::string("revision-2"));

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    FullTextIndexWorkRequest request;
    request.identity = identity;
    request.policy = policy;
    request.source_revision = "revision-2";
    request.maximum_documents = 101U;
    request.maximum_document_bytes = 1024U;
    request.maximum_corpus_bytes = 4096U;
    request.deadline = deadline;
    auto result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kCompleted);
    QVERIFY(result.message.empty());
    QVERIFY(port.request.has_value());
    QCOMPARE(port.request->identity, identity);
    QCOMPARE(port.request->policy, policy);
    QCOMPARE(port.request->source_revision, std::string("revision-2"));
    QCOMPARE(port.request->maximum_documents, 101U);
    QCOMPARE(port.request->maximum_document_bytes, 1024U);
    QCOMPARE(port.request->maximum_corpus_bytes, 4096U);
    QCOMPARE(port.request->deadline, deadline);
    QVERIFY(port.request->cancellation == nullptr);

    Cancelled cancelled;
    request.cancellation = &cancelled;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kCancelled);
    QVERIFY(port.cancellation_observed);

    port.behavior = FakeFormatWorkPort::Behavior::kFail;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kFailed);
    QCOMPARE(result.message, std::string("adapter failure"));
    port.behavior = FakeFormatWorkPort::Behavior::kThrowStandard;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kFailed);
    QCOMPARE(result.message, std::string("escaped adapter failure"));
    port.behavior = FakeFormatWorkPort::Behavior::kThrowUnknown;
    result = port.PerformFullTextIndexWork(request);
    QCOMPARE(result.status, FullTextIndexWorkStatus::kFailed);
    QCOMPARE(result.message,
             std::string("Unknown full-text index work failure"));
}

void FullTextIndexTest::RegistrationMetadataAndEligibility() {
    TemporaryDirectory directory;
    const std::string canonical_format_types[] = {
        "AARD", "BGL",      "DICTD", "DSL", "MDICT",  "SDICT",
        "SLOB", "STARDICT", "XDXF",  "ZIM", "EPWING", "GLS",
    };
    for (const auto& format_type : canonical_format_types) {
        FullTextIndexLifecycleCoordinator coordinator;
        auto port = std::make_shared<FakeFormatWorkPort>();
        QVERIFY(Register(
            coordinator,
            Registration("dictionary-" + format_type, format_type, 7U), port));
        QCOMPARE(port->support_probe_count, 1U);
        QCOMPARE(port->revision_probe_count, 1U);
    }

    const std::string invalid_format_types[] = {
        "",
        "UNKNOWN",
        "aard",
        std::string("AARD\0", 5U),
        std::string("AARD\xc2\xa0", 6U),
    };
    FullTextIndexLifecycleCoordinator coordinator;
    auto existing = std::make_shared<FakeFormatWorkPort>();
    QVERIFY(
        Register(coordinator, Registration("existing", "AARD", 12U), existing));
    const auto original = coordinator.Snapshot("existing");
    for (const auto& format_type : invalid_format_types) {
        auto new_port = std::make_shared<FakeFormatWorkPort>();
        QVERIFY(
            !Register(coordinator,
                      Registration("new-" + std::to_string(format_type.size()),
                                   format_type),
                      new_port));
        QCOMPARE(new_port->support_probe_count, 0U);
        QCOMPARE(new_port->revision_probe_count, 0U);
        QVERIFY(
            !coordinator.Snapshot("new-" + std::to_string(format_type.size()))
                 .has_value());

        auto duplicate_port = std::make_shared<FakeFormatWorkPort>();
        QVERIFY(!Register(coordinator, Registration("existing", format_type),
                          duplicate_port));
        QCOMPARE(duplicate_port->support_probe_count, 0U);
        QCOMPARE(duplicate_port->revision_probe_count, 0U);
        QCOMPARE(coordinator.Snapshot("existing"), original);
    }
    auto rejected = std::make_shared<FakeFormatWorkPort>();
    QVERIFY(!Register(coordinator, Registration("", "AARD"), rejected));
    QVERIFY(!coordinator.RegisterDictionary(
        Registration("null", "AARD"), nullptr,
        std::make_shared<FullTextIndexSnapshotHolder>()));
    QVERIFY(!Register(coordinator, Registration("existing", "AARD"), rejected));
    QCOMPARE(rejected->support_probe_count, 0U);
    QCOMPARE(rejected->revision_probe_count, 0U);
    QCOMPARE(coordinator.Snapshot("existing"), original);
    QVERIFY(coordinator.SubmitRebuild({{1U, "existing"}, {}}));
    const auto requested_existing = coordinator.Snapshot("existing");
    auto invalid_duplicate = std::make_shared<FakeFormatWorkPort>();
    QVERIFY(!Register(coordinator, Registration("existing", "aArD"),
                      invalid_duplicate));
    QCOMPARE(invalid_duplicate->support_probe_count, 0U);
    QCOMPARE(invalid_duplicate->revision_probe_count, 0U);
    QCOMPARE(coordinator.Snapshot("existing"), requested_existing);
    FullTextIndexWorkRequest existing_work;
    existing_work.identity = {1U, "existing"};
    existing->replacement_snapshot = Snapshot(directory, "existing");
    QVERIFY(coordinator.ExecuteBoundedWork(existing_work));
    QCOMPARE(coordinator.Snapshot("existing")->state(),
             FullTextIndexLifecycleState::kCurrent);

    FullTextIndexRegistrationMetadata caller =
        Registration("copied", "MDICT", 9U);
    auto copied_port = std::make_shared<FakeFormatWorkPort>();
    QVERIFY(Register(coordinator, caller, copied_port));
    caller.dictionary_id = "changed";
    caller.format_type = "DSL";
    caller.article_count = 999U;
    FullTextIndexPolicy policy;
    policy.disabled_format_types = "mdict";
    QVERIFY(coordinator.SubmitRebuild({{1U, "copied"}, policy}));
    QCOMPARE(coordinator.Snapshot("copied")->state(),
             FullTextIndexLifecycleState::kPolicyExcluded);
    QVERIFY(!coordinator.Snapshot("changed").has_value());
    policy.disabled_format_types.clear();
    policy.maximum_dictionary_articles = 9U;
    QVERIFY(coordinator.SubmitRebuild({{2U, "copied"}, policy}));
    QCOMPARE(coordinator.Snapshot("copied")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    policy.maximum_dictionary_articles = 8U;
    QVERIFY(coordinator.SubmitRebuild({{3U, "copied"}, policy}));
    QCOMPARE(coordinator.Snapshot("copied")->state(),
             FullTextIndexLifecycleState::kPolicyExcluded);

    const auto metadata = Registration("predicate", "AARD", 10U);
    policy = {};
    QVERIFY(IsFullTextIndexPolicyEligible(metadata, policy));
    policy.enabled = false;
    QVERIFY(!IsFullTextIndexPolicyEligible(metadata, policy));
    policy.enabled = true;
    const std::string disabled_matches[] = {
        "AARD",
        "aard",
        "AaRd",
        "XAARD",
        "AARDX",
        " DSL|aard ;BGL ",
        std::string("x\0AaRd\0y", 8U),
        std::string("\xc2\xa0", 2U) + "aard" + std::string("\xc2\xa0", 2U),
    };
    for (const auto& disabled : disabled_matches) {
        policy.disabled_format_types = disabled;
        QVERIFY(!IsFullTextIndexPolicyEligible(metadata, policy));
    }
    const std::string disabled_non_matches[] = {
        "A ARD",
        "AAR D",
        "DSL|BGL",
        std::string("AA\0RD", 5U),
        std::string("AA", 2U) + std::string("\xc2\xa0", 2U) + "RD",
    };
    for (const auto& disabled : disabled_non_matches) {
        policy.disabled_format_types = disabled;
        QVERIFY(IsFullTextIndexPolicyEligible(metadata, policy));
    }
    policy.disabled_format_types.clear();
    policy.maximum_dictionary_articles = 0U;
    QVERIFY(IsFullTextIndexPolicyEligible(metadata, policy));
    policy.maximum_dictionary_articles = 11U;
    QVERIFY(IsFullTextIndexPolicyEligible(metadata, policy));
    policy.maximum_dictionary_articles = 10U;
    QVERIFY(IsFullTextIndexPolicyEligible(metadata, policy));
    policy.maximum_dictionary_articles = 9U;
    QVERIFY(!IsFullTextIndexPolicyEligible(metadata, policy));
}

void FullTextIndexTest::PolicyExcludedLifecycle() {
    FullTextIndexLifecycleCoordinator coordinator;
    auto supported = std::make_shared<FakeFormatWorkPort>();
    auto unsupported = std::make_shared<FakeFormatWorkPort>();
    unsupported->supported = false;
    QVERIFY(Register(coordinator, Registration("supported"), supported));
    QVERIFY(Register(coordinator, Registration("unsupported"), unsupported));
    auto failed_initial = std::make_shared<FakeFormatWorkPort>();
    failed_initial->throw_revision = true;
    QVERIFY(
        Register(coordinator, Registration("failed-initial"), failed_initial));
    QCOMPARE(coordinator.Snapshot("failed-initial")->state(),
             FullTextIndexLifecycleState::kFailed);

    FullTextIndexPolicy excluded_policy;
    excluded_policy.enabled = false;
    const auto supported_revision_probes = supported->revision_probe_count;
    QVERIFY(coordinator.SubmitRebuild({{1U, "supported"}, excluded_policy}));
    const auto excluded = coordinator.Snapshot("supported");
    QCOMPARE(excluded->state(), FullTextIndexLifecycleState::kPolicyExcluded);
    QVERIFY(excluded->format_capable());
    QVERIFY(excluded->source_revision().empty());
    QCOMPARE(supported->revision_probe_count, supported_revision_probes);
    QCOMPARE(supported->invocation_count, 0U);
    FullTextIndexWorkRequest work;
    work.identity = {1U, "supported"};
    QVERIFY(!coordinator.ExecuteBoundedWork(work));
    QCOMPARE(supported->invocation_count, 0U);
    QVERIFY(coordinator.Cancel({{1U, "supported"}}));
    QVERIFY(coordinator.Cancel({{1U, "supported"}}));
    QCOMPARE(coordinator.Snapshot("supported"), excluded);

    FullTextIndexPolicy eligible_policy;
    supported->source_revision = "recovered";
    QVERIFY(coordinator.SubmitRebuild({{2U, "supported"}, eligible_policy}));
    QCOMPARE(coordinator.Snapshot("supported")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(coordinator.Snapshot("supported")->source_revision(),
             std::string("recovered"));

    QVERIFY(coordinator.SubmitRebuild({{1U, "unsupported"}, excluded_policy}));
    QCOMPARE(coordinator.Snapshot("unsupported")->state(),
             FullTextIndexLifecycleState::kUnavailable);
    QVERIFY(!coordinator.Snapshot("unsupported")->format_capable());

    supported->throw_revision = true;
    QVERIFY(coordinator.SubmitRebuild({{3U, "supported"}, eligible_policy}));
    QCOMPARE(coordinator.Snapshot("supported")->state(),
             FullTextIndexLifecycleState::kFailed);
    QVERIFY(coordinator.Snapshot("supported")->format_capable());
    QVERIFY(coordinator.Snapshot("supported")->source_revision().empty());
}

void FullTextIndexTest::AppliesPolicyToAllRegisteredEntries() {
    TemporaryDirectory directory;
    FullTextIndexLifecycleCoordinator empty;
    QVERIFY(empty.ApplyPolicyToRegisteredEntries({}));

    FullTextIndexLifecycleCoordinator coordinator;
    auto eligible = std::make_shared<FakeFormatWorkPort>();
    auto threshold = std::make_shared<FakeFormatWorkPort>();
    auto excluded = std::make_shared<FakeFormatWorkPort>();
    auto incapable = std::make_shared<FakeFormatWorkPort>();
    incapable->supported = false;
    auto failed = std::make_shared<FakeFormatWorkPort>();
    auto holder = std::make_shared<FullTextIndexSnapshotHolder>();
    const auto current = Snapshot(directory, "policy-current");
    QVERIFY(holder->Publish(current));
    QVERIFY(Register(coordinator, Registration("eligible", "AARD", 9U),
                     eligible, holder));
    QVERIFY(Register(coordinator, Registration("threshold", "DSL", 10U),
                     threshold));
    QVERIFY(
        Register(coordinator, Registration("excluded", "BGL", 2U), excluded));
    QVERIFY(Register(coordinator, Registration("incapable", "MDICT", 1U),
                     incapable));
    QVERIFY(Register(coordinator, Registration("failed", "SDICT", 1U), failed));
    failed->throw_revision = true;

    FullTextIndexPolicy policy;
    policy.maximum_dictionary_articles = 10U;
    policy.disabled_format_types = "bgl";
    const auto eligible_revision_probes = eligible->revision_probe_count;
    QVERIFY(coordinator.ApplyPolicyToRegisteredEntries(policy));
    QCOMPARE(coordinator.Snapshot("eligible")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(coordinator.Snapshot("eligible")->identity().generation, 1U);
    QCOMPARE(coordinator.Snapshot("eligible")->source_revision(),
             std::string("revision-1"));
    QCOMPARE(eligible->revision_probe_count, eligible_revision_probes + 1U);
    QCOMPARE(coordinator.Snapshot("threshold")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(coordinator.Snapshot("excluded")->state(),
             FullTextIndexLifecycleState::kPolicyExcluded);
    QCOMPARE(coordinator.Snapshot("incapable")->state(),
             FullTextIndexLifecycleState::kUnavailable);
    QCOMPARE(coordinator.Snapshot("failed")->state(),
             FullTextIndexLifecycleState::kFailed);
    QCOMPARE(eligible->invocation_count, 0U);
    QCOMPARE(holder->Acquire().get(), current.get());

    FullTextIndexPolicy disabled;
    disabled.enabled = false;
    QVERIFY(coordinator.ApplyPolicyToRegisteredEntries(disabled));
    for (const auto* dictionary_id :
         {"eligible", "threshold", "excluded", "failed"}) {
        QCOMPARE(coordinator.Snapshot(dictionary_id)->identity().generation,
                 2U);
        QCOMPARE(coordinator.Snapshot(dictionary_id)->state(),
                 FullTextIndexLifecycleState::kPolicyExcluded);
    }
    QCOMPARE(coordinator.Snapshot("incapable")->identity().generation, 2U);
    QCOMPARE(coordinator.Snapshot("incapable")->state(),
             FullTextIndexLifecycleState::kUnavailable);
    QCOMPARE(holder->Acquire().get(), current.get());

    FullTextIndexPolicy below_threshold;
    below_threshold.maximum_dictionary_articles = 9U;
    QVERIFY(coordinator.ApplyPolicyToRegisteredEntries(below_threshold));
    QCOMPARE(coordinator.Snapshot("eligible")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(coordinator.Snapshot("threshold")->state(),
             FullTextIndexLifecycleState::kPolicyExcluded);
    QCOMPARE(coordinator.Snapshot("eligible")->identity().generation, 3U);
    QCOMPARE(coordinator.Snapshot("threshold")->identity().generation, 3U);

    FullTextIndexLifecycleCoordinator requested;
    auto requested_port = std::make_shared<FakeFormatWorkPort>();
    QVERIFY(Register(requested, Registration("requested"), requested_port));
    QVERIFY(requested.SubmitRebuild({{7U, "requested"}, {}}));
    const auto requested_token = requested_port;
    QVERIFY(requested.ApplyPolicyToRegisteredEntries(disabled));
    QCOMPARE(requested.Snapshot("requested")->identity().generation, 8U);
    FullTextIndexWorkRequest old_request;
    old_request.identity = {7U, "requested"};
    QVERIFY(!requested.ExecuteBoundedWork(old_request));
    QCOMPARE(requested_token->invocation_count, 0U);

    FullTextIndexLifecycleCoordinator working;
    auto blocking = std::make_shared<BlockingFormatWorkPort>();
    auto working_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    QVERIFY(working_holder->Publish(current));
    QVERIFY(
        Register(working, Registration("working"), blocking, working_holder));
    QVERIFY(working.SubmitRebuild({{4U, "working"}, {}}));
    FullTextIndexWorkRequest work;
    work.identity = {4U, "working"};
    auto stale = std::async(std::launch::async,
                            [&] { return working.ExecuteBoundedWork(work); });
    blocking->WaitUntilStarted();
    QVERIFY(working.ApplyPolicyToRegisteredEntries(disabled));
    const auto artifact = directory.path() / "stale-policy.gdfts";
    auto prepared = FullTextIndex::PrepareUpdate(
        artifact, {}, {Document("stale", "Stale", "stale policy content")});
    blocking->Finish({FullTextIndexWorkStatus::kCompleted,
                      {},
                      prepared->snapshot(),
                      prepared});
    QVERIFY(stale.get());
    QVERIFY(blocking->cancellation_observed());
    QVERIFY(!std::filesystem::exists(artifact));
    QCOMPARE(working.Snapshot("working")->identity().generation, 5U);
    QCOMPARE(working.Snapshot("working")->state(),
             FullTextIndexLifecycleState::kPolicyExcluded);
    QCOMPARE(working_holder->Acquire().get(), current.get());
}

void FullTextIndexTest::ReconcilesValidatedStartupArtifacts() {
    TemporaryDirectory directory;
    FullTextIndexLifecycleCoordinator empty;
    QVERIFY(empty.ApplyPolicyToRegisteredEntries({}));
    QVERIFY(!empty.ReconcileStartupArtifact({}));

    FullTextIndexLifecycleCoordinator coordinator;
    auto first_port = std::make_shared<FakeFormatWorkPort>();
    auto second_port = std::make_shared<FakeFormatWorkPort>();
    auto excluded_port = std::make_shared<FakeFormatWorkPort>();
    auto unavailable_port = std::make_shared<FakeFormatWorkPort>();
    unavailable_port->supported = false;
    auto failed_port = std::make_shared<FakeFormatWorkPort>();
    auto first_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    auto second_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    auto excluded_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    const auto first_snapshot = Snapshot(directory, "startup-first");
    const auto second_snapshot = Snapshot(directory, "startup-second");
    const auto excluded_snapshot = Snapshot(directory, "startup-excluded");
    QVERIFY(first_holder->Publish(first_snapshot));
    QVERIFY(second_holder->Publish(second_snapshot));
    QVERIFY(excluded_holder->Publish(excluded_snapshot));
    QVERIFY(
        Register(coordinator, Registration("first"), first_port, first_holder));
    QVERIFY(Register(coordinator, Registration("second"), second_port,
                     second_holder));
    QVERIFY(Register(coordinator, Registration("excluded", "BGL"),
                     excluded_port, excluded_holder));
    QVERIFY(
        Register(coordinator, Registration("unavailable"), unavailable_port));
    QVERIFY(Register(coordinator, Registration("failed"), failed_port));
    failed_port->throw_revision = true;

    FullTextIndexPolicy policy;
    policy.disabled_format_types = "BGL";
    QVERIFY(coordinator.ApplyPolicyToRegisteredEntries(policy));
    const auto first_identity = coordinator.Snapshot("first")->identity();
    const auto second_identity = coordinator.Snapshot("second")->identity();
    const auto first_support_probes = first_port->support_probe_count;
    const auto first_revision_probes = first_port->revision_probe_count;
    const FullTextIndexStartupArtifactEvidence first_evidence{
        first_identity, "revision-1", first_snapshot};

    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {{first_identity.generation + 1U, "first"},
         "revision-1",
         first_snapshot}));
    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {first_identity, "stale-revision", first_snapshot}));
    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {first_identity, "revision-1", second_snapshot}));
    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {first_identity, "revision-1", nullptr}));
    QCOMPARE(coordinator.Snapshot("first")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(first_holder->Acquire(), first_snapshot);

    QVERIFY(coordinator.ReconcileStartupArtifact(first_evidence));
    QCOMPARE(coordinator.Snapshot("first")->identity(), first_identity);
    QCOMPARE(coordinator.Snapshot("first")->state(),
             FullTextIndexLifecycleState::kCurrent);
    QCOMPARE(first_holder->Acquire(), first_snapshot);
    QVERIFY(!coordinator.ReconcileStartupArtifact(first_evidence));
    QCOMPARE(first_port->invocation_count, 0U);
    QCOMPARE(first_port->support_probe_count, first_support_probes);
    QCOMPARE(first_port->revision_probe_count, first_revision_probes);

    QVERIFY(second_holder->Publish(first_snapshot));
    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {second_identity, "revision-1", second_snapshot}));
    QCOMPARE(coordinator.Snapshot("second")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QVERIFY(second_holder->Publish(second_snapshot));
    QVERIFY(coordinator.ReconcileStartupArtifact(
        {second_identity, "revision-1", second_snapshot}));
    QCOMPARE(coordinator.Snapshot("second")->identity(), second_identity);
    QCOMPARE(coordinator.Snapshot("second")->state(),
             FullTextIndexLifecycleState::kCurrent);
    QCOMPARE(second_holder->Acquire(), second_snapshot);
    QCOMPARE(second_port->invocation_count, 0U);

    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {coordinator.Snapshot("excluded")->identity(), "", excluded_snapshot}));
    QCOMPARE(coordinator.Snapshot("excluded")->state(),
             FullTextIndexLifecycleState::kPolicyExcluded);
    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {coordinator.Snapshot("unavailable")->identity(), "", first_snapshot}));
    QCOMPARE(coordinator.Snapshot("unavailable")->state(),
             FullTextIndexLifecycleState::kUnavailable);
    QVERIFY(!coordinator.ReconcileStartupArtifact(
        {coordinator.Snapshot("failed")->identity(), "", first_snapshot}));
    QCOMPARE(coordinator.Snapshot("failed")->state(),
             FullTextIndexLifecycleState::kFailed);

    FullTextIndexLifecycleCoordinator cancelled;
    auto cancelled_port = std::make_shared<FakeFormatWorkPort>();
    auto cancelled_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    QVERIFY(cancelled_holder->Publish(first_snapshot));
    QVERIFY(Register(cancelled, Registration("cancelled"), cancelled_port,
                     cancelled_holder));
    QVERIFY(cancelled.ApplyPolicyToRegisteredEntries({}));
    const auto cancelled_identity = cancelled.Snapshot("cancelled")->identity();
    QVERIFY(cancelled.Cancel({cancelled_identity}));
    QVERIFY(!cancelled.ReconcileStartupArtifact(
        {cancelled_identity, "revision-1", first_snapshot}));
    QCOMPARE(cancelled.Snapshot("cancelled")->state(),
             FullTextIndexLifecycleState::kCancelled);

    FullTextIndexLifecycleCoordinator replaced;
    auto replaced_port = std::make_shared<FakeFormatWorkPort>();
    auto replaced_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    QVERIFY(replaced_holder->Publish(first_snapshot));
    QVERIFY(Register(replaced, Registration("replaced"), replaced_port,
                     replaced_holder));
    QVERIFY(replaced.ApplyPolicyToRegisteredEntries({}));
    const auto stale_identity = replaced.Snapshot("replaced")->identity();
    QVERIFY(replaced.ApplyPolicyToRegisteredEntries({}));
    QVERIFY(!replaced.ReconcileStartupArtifact(
        {stale_identity, "revision-1", first_snapshot}));
    QCOMPARE(replaced.Snapshot("replaced")->identity().generation, 2U);
    QCOMPARE(replaced.Snapshot("replaced")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(replaced_holder->Acquire(), first_snapshot);

    FullTextIndexLifecycleCoordinator working;
    auto blocking = std::make_shared<BlockingFormatWorkPort>();
    auto working_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    QVERIFY(working_holder->Publish(first_snapshot));
    QVERIFY(
        Register(working, Registration("working"), blocking, working_holder));
    QVERIFY(working.ApplyPolicyToRegisteredEntries({}));
    const auto working_identity = working.Snapshot("working")->identity();
    FullTextIndexWorkRequest request;
    request.identity = working_identity;
    auto work = std::async(std::launch::async,
                           [&] { return working.ExecuteBoundedWork(request); });
    blocking->WaitUntilStarted();
    QVERIFY(!working.ReconcileStartupArtifact(
        {working_identity, "blocking-revision", first_snapshot}));
    QCOMPARE(working.Snapshot("working")->state(),
             FullTextIndexLifecycleState::kWorking);
    blocking->Finish({FullTextIndexWorkStatus::kFailed, "expected"});
    QVERIFY(work.get());
    QCOMPARE(working.Snapshot("working")->state(),
             FullTextIndexLifecycleState::kFailed);
    QCOMPARE(working_holder->Acquire(), first_snapshot);
}

void FullTextIndexTest::ProjectsBoundedWorkRequests() {
    static_assert(!std::is_assignable_v<FullTextIndexExecutionBounds&,
                                        FullTextIndexExecutionBounds>);

    const auto future =
        std::chrono::steady_clock::now() + std::chrono::minutes(5);
    const FullTextIndexExecutionBounds bounds(17U, 1025U, 8193U, future);
    FullTextIndexLifecycleCoordinator empty;
    QVERIFY(!empty.ProjectBoundedWorkRequest({1U, "missing"}, bounds));

    TemporaryDirectory directory;
    FullTextIndexLifecycleCoordinator coordinator;
    auto first_port = std::make_shared<FakeFormatWorkPort>();
    auto second_port = std::make_shared<FakeFormatWorkPort>();
    auto excluded_port = std::make_shared<FakeFormatWorkPort>();
    auto unavailable_port = std::make_shared<FakeFormatWorkPort>();
    unavailable_port->supported = false;
    auto first_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    auto second_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    const auto first_snapshot = Snapshot(directory, "projection-first");
    const auto second_snapshot = Snapshot(directory, "projection-second");
    QVERIFY(first_holder->Publish(first_snapshot));
    QVERIFY(second_holder->Publish(second_snapshot));
    QVERIFY(Register(coordinator, Registration("first", "AARD", 10U),
                     first_port, first_holder));
    QVERIFY(Register(coordinator, Registration("second"), second_port,
                     second_holder));
    QVERIFY(
        Register(coordinator, Registration("excluded", "BGL"), excluded_port));
    QVERIFY(
        Register(coordinator, Registration("unavailable"), unavailable_port));

    QVERIFY(!coordinator.ProjectBoundedWorkRequest({0U, "first"}, bounds));
    QCOMPARE(coordinator.Snapshot("first")->state(),
             FullTextIndexLifecycleState::kNotIndexed);

    FullTextIndexPolicy policy;
    policy.maximum_dictionary_articles = 25U;
    policy.disabled_format_types = "BGL";
    QVERIFY(coordinator.ApplyPolicyToRegisteredEntries(policy));
    const auto first_before = coordinator.Snapshot("first");
    const auto second_before = coordinator.Snapshot("second");
    const auto first_support_probes = first_port->support_probe_count;
    const auto first_revision_probes = first_port->revision_probe_count;
    const auto first_invocations = first_port->invocation_count;

    const auto first =
        coordinator.ProjectBoundedWorkRequest(first_before->identity(), bounds);
    QVERIFY(first.has_value());
    QCOMPARE(first->identity, first_before->identity());
    QCOMPARE(first->policy, policy);
    QCOMPARE(first->source_revision, first_before->source_revision());
    QCOMPARE(first->maximum_documents, 17U);
    QCOMPARE(first->maximum_document_bytes, 1025U);
    QCOMPARE(first->maximum_corpus_bytes, 8193U);
    QCOMPARE(first->deadline, future);
    QVERIFY(first->cancellation != nullptr);
    QVERIFY(!first->cancellation->IsCancellationRequested());

    const auto repeated =
        coordinator.ProjectBoundedWorkRequest(first_before->identity(), bounds);
    QVERIFY(repeated.has_value());
    QCOMPARE(repeated->identity, first->identity);
    QCOMPARE(repeated->policy, first->policy);
    QCOMPARE(repeated->source_revision, first->source_revision);
    QCOMPARE(repeated->maximum_documents, first->maximum_documents);
    QCOMPARE(repeated->maximum_document_bytes, first->maximum_document_bytes);
    QCOMPARE(repeated->maximum_corpus_bytes, first->maximum_corpus_bytes);
    QCOMPARE(repeated->deadline, first->deadline);
    QCOMPARE(repeated->cancellation, first->cancellation);
    QCOMPARE(coordinator.Snapshot("first"), first_before);
    QCOMPARE(coordinator.Snapshot("second"), second_before);
    QCOMPARE(first_holder->Acquire(), first_snapshot);
    QCOMPARE(second_holder->Acquire(), second_snapshot);
    QCOMPARE(first_port->support_probe_count, first_support_probes);
    QCOMPARE(first_port->revision_probe_count, first_revision_probes);
    QCOMPARE(first_port->invocation_count, first_invocations);

    const FullTextIndexExecutionBounds zero_documents(0U, 1U, 1U, future);
    const FullTextIndexExecutionBounds zero_document_bytes(1U, 0U, 1U, future);
    const FullTextIndexExecutionBounds zero_corpus_bytes(1U, 1U, 0U, future);
    const FullTextIndexExecutionBounds overflow(
        2U, std::numeric_limits<std::size_t>::max(), 1U, future);
    const FullTextIndexExecutionBounds incoherent(2U, 3U, 7U, future);
    const FullTextIndexExecutionBounds expired(
        1U, 1U, 1U, std::chrono::steady_clock::time_point::min());
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(first->identity,
                                                   zero_documents));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(first->identity,
                                                   zero_document_bytes));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(first->identity,
                                                   zero_corpus_bytes));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(first->identity, overflow));
    QVERIFY(
        !coordinator.ProjectBoundedWorkRequest(first->identity, incoherent));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(first->identity, expired));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(
        {first->identity.generation + 1U, "first"}, bounds));
    const auto projected_second = coordinator.ProjectBoundedWorkRequest(
        {first->identity.generation, "second"}, bounds);
    QVERIFY(projected_second.has_value());
    QCOMPARE(projected_second->identity, second_before->identity());
    QCOMPARE(projected_second->source_revision,
             second_before->source_revision());
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(
        {first->identity.generation, "unknown"}, bounds));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(
        coordinator.Snapshot("excluded")->identity(), bounds));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(
        coordinator.Snapshot("unavailable")->identity(), bounds));
    QCOMPARE(coordinator.Snapshot("first"), first_before);
    QCOMPARE(coordinator.Snapshot("second"), second_before);
    QCOMPARE(excluded_port->invocation_count, 0U);
    QCOMPARE(unavailable_port->invocation_count, 0U);

    const auto second_identity = second_before->identity();
    QVERIFY(coordinator.Cancel({second_identity}));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(second_identity, bounds));
    QCOMPARE(coordinator.Snapshot("second")->state(),
             FullTextIndexLifecycleState::kCancelled);
    QCOMPARE(coordinator.Snapshot("first"), first_before);

    const auto stale_identity = first->identity;
    QVERIFY(coordinator.SubmitRebuild(
        {{stale_identity.generation + 1U, "first"}, policy}));
    QVERIFY(!coordinator.ProjectBoundedWorkRequest(stale_identity, bounds));
    const auto replacement_identity = coordinator.Snapshot("first")->identity();
    const auto projected_replacement =
        coordinator.ProjectBoundedWorkRequest(replacement_identity, bounds);
    QVERIFY(projected_replacement.has_value());
    first_port->replacement_snapshot =
        Snapshot(directory, "projection-replacement");
    QVERIFY(coordinator.ExecuteBoundedWork(*projected_replacement));
    QCOMPARE(coordinator.Snapshot("first")->state(),
             FullTextIndexLifecycleState::kCurrent);
    QCOMPARE(first_port->request->maximum_documents, 17U);
    QCOMPARE(first_port->request->maximum_document_bytes, 1025U);
    QCOMPARE(first_port->request->maximum_corpus_bytes, 8193U);
    QCOMPARE(first_port->request->deadline, future);
    QCOMPARE(first_port->request->identity, replacement_identity);
    QCOMPARE(first_port->request->policy, policy);
    QCOMPARE(first_port->request->source_revision, std::string("revision-1"));
    QVERIFY(
        !coordinator.ProjectBoundedWorkRequest(replacement_identity, bounds));

    FullTextIndexLifecycleCoordinator failed;
    auto failed_port = std::make_shared<FakeFormatWorkPort>();
    QVERIFY(Register(failed, Registration("failed"), failed_port));
    failed_port->throw_revision = true;
    QVERIFY(failed.ApplyPolicyToRegisteredEntries({}));
    QVERIFY(!failed.ProjectBoundedWorkRequest(
        failed.Snapshot("failed")->identity(), bounds));
    QCOMPARE(failed.Snapshot("failed")->state(),
             FullTextIndexLifecycleState::kFailed);
    QCOMPARE(failed_port->invocation_count, 0U);

    FullTextIndexLifecycleCoordinator working;
    auto blocking = std::make_shared<BlockingFormatWorkPort>();
    QVERIFY(Register(working, Registration("working"), blocking));
    QVERIFY(working.ApplyPolicyToRegisteredEntries({}));
    const auto working_identity = working.Snapshot("working")->identity();
    const auto projected_working =
        working.ProjectBoundedWorkRequest(working_identity, bounds);
    QVERIFY(projected_working.has_value());
    auto running = std::async(std::launch::async, [&] {
        return working.ExecuteBoundedWork(*projected_working);
    });
    blocking->WaitUntilStarted();
    QVERIFY(!working.ProjectBoundedWorkRequest(working_identity, bounds));
    QCOMPARE(working.Snapshot("working")->state(),
             FullTextIndexLifecycleState::kWorking);
    blocking->Finish({FullTextIndexWorkStatus::kFailed, "expected"});
    QVERIFY(running.get());
    QCOMPARE(working.Snapshot("working")->state(),
             FullTextIndexLifecycleState::kFailed);
}

void FullTextIndexTest::CoordinatesExplicitLifecycleTransitions() {
    TemporaryDirectory directory;
    FullTextIndexLifecycleCoordinator coordinator;
    auto supported = std::make_shared<FakeFormatWorkPort>();
    auto unsupported = std::make_shared<FakeFormatWorkPort>();
    unsupported->supported = false;
    QVERIFY(Register(coordinator, Registration("supported"), supported));
    QVERIFY(Register(coordinator, Registration("unsupported"), unsupported));
    QVERIFY(!Register(coordinator, Registration("supported"), supported));
    QVERIFY(!Register(coordinator, Registration(""), supported));
    QVERIFY(!coordinator.RegisterDictionary(
        Registration("null"), nullptr,
        std::make_shared<FullTextIndexSnapshotHolder>()));
    auto null_holder_port = std::make_shared<FakeFormatWorkPort>();
    QVERIFY(!coordinator.RegisterDictionary(Registration("null-holder"),
                                            null_holder_port, nullptr));
    QCOMPARE(null_holder_port->support_probe_count, 0U);
    QCOMPARE(null_holder_port->revision_probe_count, 0U);
    QVERIFY(!coordinator.Snapshot("missing").has_value());
    QVERIFY(!coordinator.Cancel({{0U, "supported"}}));

    const auto initial = coordinator.Snapshot("supported");
    QVERIFY(initial.has_value());
    QCOMPARE(initial->identity(), (FullTextIndexWorkIdentity{0U, "supported"}));
    QCOMPARE(initial->state(), FullTextIndexLifecycleState::kNotIndexed);
    QVERIFY(initial->format_capable());
    QCOMPARE(initial->source_revision(), std::string("revision-1"));
    QCOMPARE(coordinator.Snapshot("unsupported")->state(),
             FullTextIndexLifecycleState::kUnavailable);

    FullTextIndexPolicy policy;
    policy.maximum_dictionary_articles = 55U;
    policy.disabled_format_types = "DSL";
    supported->source_revision = "revision-2";
    const FullTextIndexWorkIdentity identity{1U, "supported"};
    QVERIFY(coordinator.SubmitRebuild({identity, policy}));
    const auto requested = coordinator.Snapshot("supported");
    QCOMPARE(requested->state(), FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(requested->source_revision(), std::string("revision-2"));
    QVERIFY(requested->format_capable());
    QCOMPARE(initial->state(), FullTextIndexLifecycleState::kNotIndexed);

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(4);
    FullTextIndexWorkRequest work;
    work.identity = identity;
    work.maximum_documents = 17U;
    work.maximum_document_bytes = 1025U;
    work.maximum_corpus_bytes = 8193U;
    work.deadline = deadline;
    supported->replacement_snapshot = Snapshot(directory, "supported");
    QVERIFY(coordinator.ExecuteBoundedWork(work));
    QCOMPARE(coordinator.Snapshot("supported")->state(),
             FullTextIndexLifecycleState::kCurrent);
    QCOMPARE(supported->invocation_count, 1U);
    QVERIFY(supported->request.has_value());
    QCOMPARE(supported->request->identity, identity);
    QCOMPARE(supported->request->policy, policy);
    QCOMPARE(supported->request->source_revision, std::string("revision-2"));
    QCOMPARE(supported->request->maximum_documents, 17U);
    QCOMPARE(supported->request->maximum_document_bytes, 1025U);
    QCOMPARE(supported->request->maximum_corpus_bytes, 8193U);
    QCOMPARE(supported->request->deadline, deadline);
    QVERIFY(supported->request->cancellation != nullptr);
    QVERIFY(!coordinator.ExecuteBoundedWork(work));
    QCOMPARE(supported->invocation_count, 1U);

    const FullTextIndexWorkIdentity unavailable{1U, "unsupported"};
    QVERIFY(coordinator.SubmitRebuild({unavailable, {}}));
    QCOMPARE(coordinator.Snapshot("unsupported")->state(),
             FullTextIndexLifecycleState::kUnavailable);
    work.identity = unavailable;
    QVERIFY(!coordinator.ExecuteBoundedWork(work));
    QCOMPARE(unsupported->invocation_count, 0U);
}

void FullTextIndexTest::IsolatesAndMonotonicallyReplacesGenerations() {
    TemporaryDirectory directory;
    FullTextIndexLifecycleCoordinator coordinator;
    auto first = std::make_shared<FakeFormatWorkPort>();
    auto second = std::make_shared<FakeFormatWorkPort>();
    auto first_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    auto second_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    const auto second_current = Snapshot(directory, "identity-current");
    QVERIFY(second_holder->Publish(second_current));
    QVERIFY(Register(coordinator, Registration("first"), first, first_holder));
    QVERIFY(
        Register(coordinator, Registration("second"), second, second_holder));

    QVERIFY(coordinator.SubmitRebuild({{0U, "second"}, {}}));
    QVERIFY(!coordinator.SubmitRebuild({{0U, "second"}, {}}));
    QVERIFY(coordinator.SubmitRebuild({{4U, "first"}, {}}));
    QVERIFY(!coordinator.SubmitRebuild({{4U, "first"}, {}}));
    QVERIFY(!coordinator.SubmitRebuild({{3U, "first"}, {}}));
    QVERIFY(!coordinator.SubmitRebuild({{99U, "missing"}, {}}));
    QCOMPARE(coordinator.Snapshot("first")->identity().generation, 4U);
    QCOMPARE(coordinator.Snapshot("second")->identity().generation, 0U);
    QCOMPARE(coordinator.Snapshot("second")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    QCOMPARE(first->invocation_count, 0U);
    QCOMPARE(second->invocation_count, 0U);

    FullTextIndexWorkRequest wrong;
    wrong.identity = {4U, "second"};
    QVERIFY(!coordinator.ExecuteBoundedWork(wrong));
    QCOMPARE(second->invocation_count, 0U);
    QCOMPARE(second_holder->Acquire().get(), second_current.get());
    QVERIFY(coordinator.SubmitRebuild({{8U, "first"}, {}}));
    QCOMPARE(coordinator.Snapshot("first")->identity().generation, 8U);
    QCOMPARE(coordinator.Snapshot("first")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
}

void FullTextIndexTest::CancelsExactWorkIdempotently() {
    TemporaryDirectory directory;
    FullTextIndexLifecycleCoordinator coordinator;
    auto port = std::make_shared<BlockingFormatWorkPort>();
    auto holder = std::make_shared<FullTextIndexSnapshotHolder>();
    const auto old_snapshot = Snapshot(directory, "cancel-old");
    const auto cancelled_candidate = Snapshot(directory, "cancel-candidate");
    QVERIFY(holder->Publish(old_snapshot));
    QVERIFY(Register(coordinator, Registration("dictionary"), port, holder));
    const FullTextIndexWorkIdentity first{1U, "dictionary"};
    QVERIFY(coordinator.SubmitRebuild({first, {}}));
    QVERIFY(coordinator.Cancel({first}));
    QVERIFY(coordinator.Cancel({first}));
    QCOMPARE(coordinator.Snapshot("dictionary")->state(),
             FullTextIndexLifecycleState::kCancelled);
    FullTextIndexWorkRequest work;
    work.identity = first;
    QVERIFY(!coordinator.ExecuteBoundedWork(work));

    const FullTextIndexWorkIdentity second{2U, "dictionary"};
    QVERIFY(coordinator.SubmitRebuild({second, {}}));
    work.identity = second;
    auto running = std::async(std::launch::async, [&] {
        return coordinator.ExecuteBoundedWork(work);
    });
    port->WaitUntilStarted();
    QCOMPARE(coordinator.Snapshot("dictionary")->state(),
             FullTextIndexLifecycleState::kWorking);
    QVERIFY(!coordinator.Cancel({{1U, "dictionary"}}));
    QVERIFY(!coordinator.Cancel({{2U, "other"}}));
    QVERIFY(coordinator.Cancel({second}));
    QVERIFY(coordinator.Cancel({second}));
    port->Finish(
        {FullTextIndexWorkStatus::kCompleted, {}, cancelled_candidate});
    QVERIFY(running.get());
    QVERIFY(port->cancellation_observed());
    QCOMPARE(coordinator.Snapshot("dictionary")->state(),
             FullTextIndexLifecycleState::kCancelled);
    QCOMPARE(holder->Acquire().get(), old_snapshot.get());
    QVERIFY(coordinator.Cancel({second}));
    QCOMPARE(coordinator.Snapshot("dictionary")->state(),
             FullTextIndexLifecycleState::kCancelled);
}

void FullTextIndexTest::SuppressesStaleCompletions() {
    TemporaryDirectory directory;
    const auto old_snapshot = Snapshot(directory, "stale-old");
    const auto stale_candidate = Snapshot(directory, "stale-candidate");
    const std::pair<BlockingFormatWorkPort::Completion, FullTextIndexWorkStatus>
        stale_outcomes[] = {
            {BlockingFormatWorkPort::Completion::kResult,
             FullTextIndexWorkStatus::kCompleted},
            {BlockingFormatWorkPort::Completion::kResult,
             FullTextIndexWorkStatus::kCancelled},
            {BlockingFormatWorkPort::Completion::kResult,
             FullTextIndexWorkStatus::kFailed},
            {BlockingFormatWorkPort::Completion::kThrowStandard,
             FullTextIndexWorkStatus::kFailed},
            {BlockingFormatWorkPort::Completion::kThrowUnknown,
             FullTextIndexWorkStatus::kFailed},
        };
    for (const auto& [completion, stale_status] : stale_outcomes) {
        FullTextIndexLifecycleCoordinator coordinator;
        auto port = std::make_shared<BlockingFormatWorkPort>();
        auto holder = std::make_shared<FullTextIndexSnapshotHolder>();
        QVERIFY(holder->Publish(old_snapshot));
        QVERIFY(
            Register(coordinator, Registration("dictionary"), port, holder));
        QVERIFY(coordinator.SubmitRebuild({{1U, "dictionary"}, {}}));
        FullTextIndexWorkRequest work;
        work.identity = {1U, "dictionary"};
        auto stale = std::async(std::launch::async, [&] {
            return coordinator.ExecuteBoundedWork(work);
        });
        port->WaitUntilStarted();
        FullTextIndexPolicy excluded;
        excluded.disabled_format_types = "aard";
        QVERIFY(coordinator.SubmitRebuild({{2U, "dictionary"}, excluded}));
        const auto replacement = coordinator.Snapshot("dictionary");
        const auto artifact =
            directory.path() /
            ("stale-artifact-" +
             std::to_string(static_cast<int>(stale_status)) + ".gdfts");
        if (completion == BlockingFormatWorkPort::Completion::kResult) {
            auto prepared = FullTextIndex::PrepareUpdate(
                artifact, {},
                {Document("stale", "Stale", "stale prepared content")});
            port->Finish({stale_status, "stale failure", prepared->snapshot(),
                          prepared});
            QVERIFY(!std::filesystem::exists(artifact));
        } else {
            port->FinishByThrowing(completion);
        }
        QVERIFY(stale.get());
        QVERIFY(!std::filesystem::exists(artifact));
        QVERIFY(port->cancellation_observed());
        QCOMPARE(coordinator.Snapshot("dictionary"), replacement);
        QCOMPARE(coordinator.Snapshot("dictionary")->identity().generation, 2U);
        QCOMPARE(coordinator.Snapshot("dictionary")->state(),
                 FullTextIndexLifecycleState::kPolicyExcluded);
        QCOMPARE(holder->Acquire().get(), old_snapshot.get());
        QVERIFY(coordinator.SubmitRebuild({{3U, "dictionary"}, {}}));
        QCOMPARE(coordinator.Snapshot("dictionary")->state(),
                 FullTextIndexLifecycleState::kWorkRequested);
    }
}

void FullTextIndexTest::ContainsCoordinatorWorkFailures() {
    TemporaryDirectory directory;
    const auto current = Snapshot(directory, "failure-current");
    const FakeFormatWorkPort::Behavior behaviors[] = {
        FakeFormatWorkPort::Behavior::kFail,
        FakeFormatWorkPort::Behavior::kThrowStandard,
        FakeFormatWorkPort::Behavior::kThrowUnknown,
    };
    std::uint64_t generation = 1U;
    for (const auto behavior : behaviors) {
        FullTextIndexLifecycleCoordinator coordinator;
        auto port = std::make_shared<FakeFormatWorkPort>();
        port->behavior = behavior;
        auto holder = std::make_shared<FullTextIndexSnapshotHolder>();
        QVERIFY(holder->Publish(current));
        QVERIFY(
            Register(coordinator, Registration("dictionary"), port, holder));
        const FullTextIndexWorkIdentity identity{generation++, "dictionary"};
        QVERIFY(coordinator.SubmitRebuild({identity, {}}));
        FullTextIndexWorkRequest work;
        work.identity = identity;
        QVERIFY(coordinator.ExecuteBoundedWork(work));
        QCOMPARE(coordinator.Snapshot("dictionary")->state(),
                 FullTextIndexLifecycleState::kFailed);
        QCOMPARE(holder->Acquire().get(), current.get());
        QCOMPARE(port->invocation_count, 1U);
    }
}

void FullTextIndexTest::PublishesImmutableSnapshots() {
    TemporaryDirectory directory;
    FullTextIndexSnapshotHolder holder;
    QVERIFY(holder.Acquire() == nullptr);

    auto old_snapshot =
        std::make_shared<const FullTextIndex>(FullTextIndex::OpenOrBuild(
            directory.path() / "old.gdfts", {},
            {Document("old", "Old", "old snapshot content")}));
    auto new_snapshot =
        std::make_shared<const FullTextIndex>(FullTextIndex::OpenOrBuild(
            directory.path() / "new.gdfts", {},
            {Document("new", "New", "new snapshot content")}));

    QVERIFY(holder.Publish(old_snapshot));
    const auto retained_old = holder.Acquire();
    QCOMPARE(retained_old.get(), old_snapshot.get());

    std::mutex mutex;
    std::condition_variable ready_condition;
    std::condition_variable start_condition;
    constexpr std::size_t kReaderCount = 16U;
    std::size_t ready_count = 0U;
    bool start = false;
    std::vector<std::future<std::shared_ptr<const FullTextIndex>>> readers;
    readers.reserve(kReaderCount);
    for (std::size_t i = 0U; i < kReaderCount; ++i) {
        readers.push_back(std::async(std::launch::async, [&] {
            {
                std::unique_lock lock(mutex);
                ++ready_count;
                ready_condition.notify_one();
                start_condition.wait(lock, [&] { return start; });
            }
            std::shared_ptr<const FullTextIndex> observed;
            for (std::size_t attempt = 0U; attempt < 256U; ++attempt) {
                observed = holder.Acquire();
                if (observed.get() != old_snapshot.get() &&
                    observed.get() != new_snapshot.get())
                    return observed;
            }
            return observed;
        }));
    }

    {
        std::unique_lock lock(mutex);
        ready_condition.wait(lock, [&] { return ready_count == kReaderCount; });
        start = true;
    }
    start_condition.notify_all();
    QVERIFY(holder.Publish(new_snapshot));
    for (auto& reader : readers) {
        const auto observed = reader.get();
        QVERIFY(observed != nullptr);
        QVERIFY(observed.get() == old_snapshot.get() ||
                observed.get() == new_snapshot.get());
    }

    const auto acquired_new = holder.Acquire();
    QCOMPARE(acquired_new.get(), new_snapshot.get());
    QCOMPARE(retained_old.get(), old_snapshot.get());
    QVERIFY(retained_old->ResolveDocument("Old-article").has_value());
    QVERIFY(!retained_old->ResolveDocument("New-article").has_value());
    QVERIFY(acquired_new->ResolveDocument("New-article").has_value());

    QVERIFY(!holder.Publish(nullptr));
    QCOMPARE(holder.Acquire().get(), new_snapshot.get());
}

void FullTextIndexTest::CoordinatesGenerationAuthorizedSnapshotHandoff() {
    TemporaryDirectory directory;
    auto holder = std::make_shared<FullTextIndexSnapshotHolder>();
    const auto old_snapshot = Snapshot(directory, "handoff-old");
    const auto replacement = Snapshot(directory, "handoff-replacement");
    QVERIFY(holder->Publish(old_snapshot));
    const auto retained_old = holder->Acquire();

    FullTextIndexLifecycleCoordinator coordinator;
    auto port = std::make_shared<BlockingFormatWorkPort>();
    QVERIFY(Register(coordinator, Registration("dictionary"), port, holder));
    const FullTextIndexWorkIdentity identity{1U, "dictionary"};
    QVERIFY(coordinator.SubmitRebuild({identity, {}}));
    FullTextIndexWorkRequest request;
    request.identity = identity;
    auto running = std::async(std::launch::async, [&] {
        return coordinator.ExecuteBoundedWork(request);
    });
    port->WaitUntilStarted();
    QCOMPARE(coordinator.Snapshot("dictionary")->state(),
             FullTextIndexLifecycleState::kWorking);
    QCOMPARE(holder->Acquire().get(), old_snapshot.get());

    port->Finish({FullTextIndexWorkStatus::kCompleted, {}, replacement});
    QVERIFY(running.get());
    QCOMPARE(coordinator.Snapshot("dictionary")->state(),
             FullTextIndexLifecycleState::kCurrent);
    QCOMPARE(holder->Acquire().get(), replacement.get());
    QCOMPARE(retained_old.get(), old_snapshot.get());
    QVERIFY(retained_old->ResolveDocument("handoff-old-article").has_value());
    QVERIFY(!retained_old->ResolveDocument("handoff-replacement-article")
                 .has_value());
    QVERIFY(holder->Acquire()
                ->ResolveDocument("handoff-replacement-article")
                .has_value());

    FullTextIndexLifecycleCoordinator null_coordinator;
    auto null_port = std::make_shared<FakeFormatWorkPort>();
    auto null_holder = std::make_shared<FullTextIndexSnapshotHolder>();
    QVERIFY(null_holder->Publish(old_snapshot));
    QVERIFY(Register(null_coordinator, Registration("null"), null_port,
                     null_holder));
    QVERIFY(null_coordinator.SubmitRebuild({{1U, "null"}, {}}));
    request.identity = {1U, "null"};
    QVERIFY(null_coordinator.ExecuteBoundedWork(request));
    QCOMPARE(null_coordinator.Snapshot("null")->state(),
             FullTextIndexLifecycleState::kFailed);
    QCOMPARE(null_holder->Acquire().get(), old_snapshot.get());
}

void FullTextIndexTest::SkipsPublicationForUnsuccessfulOffSideWork() {
    TemporaryDirectory directory;
    auto holder = std::make_shared<FullTextIndexSnapshotHolder>();
    auto current =
        std::make_shared<const FullTextIndex>(FullTextIndex::OpenOrBuild(
            directory.path() / "current.gdfts", {},
            {Document("current", "Current", "published content")}));
    QVERIFY(holder->Publish(current));

    const auto verify_unchanged = [&] {
        QCOMPARE(holder->Acquire().get(), current.get());
    };
    try {
        throw std::runtime_error("off-side build failure");
    } catch (const std::runtime_error&) {}
    verify_unchanged();

    Cancelled cancelled;
    try {
        (void)FullTextIndex::OpenOrBuild(
            directory.path() / "cancelled.gdfts", {},
            {Document("cancelled", "Cancelled", "content")}, &cancelled);
        QFAIL("Cancelled off-side construction unexpectedly succeeded");
    } catch (const FullTextIndexError& error) {
        QCOMPARE(error.code(), FullTextErrorCode::kCancelled);
    }
    verify_unchanged();

    try {
        (void)FullTextIndex::OpenOrBuild(
            directory.path() / "expired.gdfts", {},
            {Document("expired", "Expired", "content")}, nullptr,
            std::chrono::steady_clock::time_point::min());
        QFAIL("Expired off-side construction unexpectedly succeeded");
    } catch (const FullTextIndexError& error) {
        QCOMPARE(error.code(), FullTextErrorCode::kDeadlineExceeded);
    }
    verify_unchanged();

    try {
        (void)FullTextIndex::OpenOrBuild(
            directory.path() / "over-budget.gdfts", {},
            {Document("large", "Large",
                      std::string(kMaximumFullTextDocumentBytes + 1U, 'x'))});
        QFAIL("Over-budget off-side construction unexpectedly succeeded");
    } catch (const FullTextIndexError& error) {
        QCOMPARE(error.code(), FullTextErrorCode::kResourceLimit);
    }
    verify_unchanged();

    FullTextIndexLifecycleCoordinator coordinator;
    auto port = std::make_shared<BlockingFormatWorkPort>();
    QVERIFY(Register(coordinator, Registration("stale"), port, holder));
    QVERIFY(coordinator.SubmitRebuild({{1U, "stale"}, {}}));
    FullTextIndexWorkRequest request;
    request.identity = {1U, "stale"};
    auto stale = std::async(std::launch::async, [&] {
        return coordinator.ExecuteBoundedWork(request);
    });
    port->WaitUntilStarted();
    QVERIFY(coordinator.SubmitRebuild({{2U, "stale"}, {}}));
    port->Finish({FullTextIndexWorkStatus::kCompleted,
                  {},
                  Snapshot(directory, "stale-off-side")});
    QVERIFY(stale.get());
    QCOMPARE(coordinator.Snapshot("stale")->identity().generation, 2U);
    QCOMPARE(coordinator.Snapshot("stale")->state(),
             FullTextIndexLifecycleState::kWorkRequested);
    verify_unchanged();
}

void FullTextIndexTest::ConstructsBoundedMatchCenteredExcerpts() {
    TemporaryDirectory directory;
    const std::string middle =
        std::string(2999U, 'a') + " MATCH " + std::string(2999U, 'b');
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "excerpts.gdfts", {},
        {Document("a", "Start", "MATCH " + std::string(5000U, 'a')),
         Document("b", "Middle", middle),
         Document("c", "End", std::string(5000U, 'a') + " MATCH")});
    FullTextQuery query;
    query.text = "MATCH";
    query.mode = FullTextQueryMode::kPlainText;
    query.result_limit = 3U;
    const auto response = index.Search(query);
    QCOMPARE(response.results.size(), 3U);

    const auto verify = [](const FullTextResult& result,
                           std::string_view document) {
        QCOMPARE(result.matches.size(), 1U);
        const auto& match = result.matches.front();
        QVERIFY(match.byte_offset <= document.size());
        QVERIFY(match.byte_length <= document.size() - match.byte_offset);
        QCOMPARE(match.text, std::string(document.substr(match.byte_offset,
                                                         match.byte_length)));
        QVERIFY(result.excerpt.size() <= kMaximumFullTextExcerptBytes);
        QVERIFY(foundation::IsValidUtf8(result.excerpt));
        QVERIFY(match.byte_offset >= result.excerpt_byte_offset);
        const auto relative = match.byte_offset - result.excerpt_byte_offset;
        QVERIFY(relative <= result.excerpt.size());
        QVERIFY(match.byte_length <= result.excerpt.size() - relative);
        QCOMPARE(result.excerpt.substr(relative, match.byte_length),
                 match.text);
    };
    verify(response.results[0], "MATCH " + std::string(5000U, 'a'));
    verify(response.results[1], middle);
    verify(response.results[2], std::string(5000U, 'a') + " MATCH");
    QCOMPARE(response.results[0].excerpt_byte_offset, 0U);
    QCOMPARE(response.results[0].excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(response.results[1].excerpt_byte_offset, 954U);
    QCOMPARE(response.results[1].excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(response.results[2].excerpt_byte_offset, 910U);
    QCOMPARE(response.results[2].excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(index.Search(query).results[1].excerpt,
             response.results[1].excerpt);
    QCOMPARE(index.Search(query).results[1].excerpt_byte_offset, 954U);

    query.text = "missing";
    QVERIFY(index.Search(query).results.empty());
    FullTextResult empty;
    QVERIFY(empty.excerpt.empty());
    QCOMPARE(empty.excerpt_byte_offset, 0U);
}

void FullTextIndexTest::PreservesUtf8MatchAndExcerptBoundaries() {
    TemporaryDirectory directory;
    std::string multibyte;
    for (std::size_t i = 0U; i < 1000U; ++i)
        multibyte += u8"日";
    multibyte += " MATCH ";
    for (std::size_t i = 0U; i < 1000U; ++i)
        multibyte += u8"本";
    const std::string oversized = std::string(5000U, 'x') + u8"日";
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "utf8-excerpts.gdfts", {},
        {Document("a", "Multibyte", multibyte),
         Document("b", "Pattern", u8"A é é Z"),
         Document("c", "Oversized", oversized),
         Document("d", "Normalized", u8"A CAFÉ noir"),
         Document("e", "Utf8Start", u8"日 " + std::string(5000U, 'a')),
         Document("f", "Utf8Middle",
                  std::string(2999U, 'a') + u8" 日 " + std::string(2999U, 'b')),
         Document("g", "Utf8End", std::string(5000U, 'a') + u8" 日")});

    FullTextQuery query;
    query.text = "MATCH";
    query.mode = FullTextQueryMode::kPlainText;
    auto result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string("MATCH"));
    QVERIFY(result.excerpt.size() <= kMaximumFullTextExcerptBytes);
    QVERIFY(result.excerpt.size() >= kMaximumFullTextExcerptBytes - 2U);
    QVERIFY(foundation::IsValidUtf8(result.excerpt));
    QVERIFY((static_cast<unsigned char>(multibyte[result.excerpt_byte_offset]) &
             0xc0U) != 0x80U);

    query.mode = FullTextQueryMode::kRegularExpression;
    query.match_case = true;
    query.text = ".";
    query.dictionary_filter_active = true;
    query.dictionary_ids = {"b"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().byte_offset, 0U);
    QCOMPARE(result.matches.front().byte_length, 1U);
    QCOMPARE(result.matches.front().text, std::string("A"));
    query.text = u8"é";
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"é"));
    QCOMPARE(result.matches.front().byte_length, std::string(u8"é").size());
    query.text = u8"é";
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"é"));
    QVERIFY(foundation::IsValidUtf8(result.matches.front().text));

    query.text = "x+";
    query.dictionary_ids = {"c"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().byte_length, 5000U);
    QCOMPARE(result.excerpt_byte_offset, 0U);
    QCOMPARE(result.excerpt.size(), kMaximumFullTextExcerptBytes);
    QCOMPARE(result.excerpt, std::string(kMaximumFullTextExcerptBytes, 'x'));

    query = {};
    query.text = "cafe";
    query.ignore_diacritics = true;
    query.dictionary_filter_active = true;
    query.dictionary_ids = {"d"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"CAFÉ"));
    QCOMPARE(result.matches.front().byte_offset, 2U);
    QCOMPARE(result.matches.front().byte_length, std::string(u8"CAFÉ").size());

    query = {};
    query.text = u8"日";
    query.dictionary_filter_active = true;
    for (const auto& [id, expected_origin] :
         std::vector<std::pair<std::string, std::size_t>>{
             {"e", 0U}, {"f", 953U}, {"g", 908U}}) {
        query.dictionary_ids = {id};
        result = index.Search(query).results.front();
        QCOMPARE(result.matches.front().text, std::string(u8"日"));
        QCOMPARE(result.matches.front().byte_length,
                 std::string(u8"日").size());
        QCOMPARE(result.excerpt_byte_offset, expected_origin);
        QVERIFY(foundation::IsValidUtf8(result.excerpt));
        QVERIFY(result.excerpt.size() <= kMaximumFullTextExcerptBytes);
    }

    query.mode = FullTextQueryMode::kWildcard;
    query.match_case = true;
    query.text = "?";
    query.dictionary_ids = {"e"};
    result = index.Search(query).results.front();
    QCOMPARE(result.matches.front().text, std::string(u8"日"));
    QCOMPARE(result.matches.front().byte_length, std::string(u8"日").size());
}

void FullTextIndexTest::ResolvesOpaqueDocumentIdentity() {
    TemporaryDirectory directory;
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "reference.gdfts", {},
        {Document("a", "Alpha", "first"), Document("b", "Beta", "second")});
    const auto resolved = index.ResolveDocument("Beta-article");
    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->dictionary.id, std::string("b"));
    QCOMPARE(resolved->document_id, std::string("Beta-article"));
    QCOMPARE(resolved->headword, std::string("Beta"));
    QVERIFY(!index.ResolveDocument("missing").has_value());
    QVERIFY(!index.ResolveDocument("").has_value());
}

void FullTextIndexTest::Lifecycle() {
    TemporaryDirectory directory;
    const auto path = directory.path() / "reference.gdfts";
    const auto source = directory.path() / "source.txt";
    {
        std::ofstream output(source);
        output << "one";
    }
    auto sources = CaptureSourceSnapshot({source});
    const std::vector documents{Document("a", "Alpha", "quick brown fox")};
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kCreated);
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kReused);
    {
        std::ofstream output(source, std::ios::app);
        output << "two";
    }
    sources = CaptureSourceSnapshot({source});
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kRebuiltStale);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "corrupt";
    }
    QCOMPARE(FullTextIndex::OpenOrBuild(path, sources, documents).state(),
             FullTextIndexState::kRebuiltCorrupt);
}

void FullTextIndexTest::PreparesWithoutPersisting() {
    TemporaryDirectory directory;
    const auto path = directory.path() / "prepared.gdfts";
    const auto source = directory.path() / "source.txt";
    std::ofstream(source) << "source";
    const auto sources = CaptureSourceSnapshot({source});
    const std::vector documents{Document("a", "Alpha", "prepared content")};

    auto created = FullTextIndex::PrepareUpdate(path, sources, documents);
    QVERIFY(created->snapshot() != nullptr);
    QCOMPARE(created->snapshot()->state(), FullTextIndexState::kCreated);
    QVERIFY(!std::filesystem::exists(path));
    QVERIFY(created->Finalize());
    QVERIFY(std::filesystem::exists(path));

    std::ifstream before_input(path, std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(before_input)),
                             std::istreambuf_iterator<char>());
    auto reused = FullTextIndex::PrepareUpdate(path, sources, documents);
    QCOMPARE(reused->snapshot()->state(), FullTextIndexState::kReused);
    std::ifstream reused_input(path, std::ios::binary);
    QCOMPARE(std::string(std::istreambuf_iterator<char>(reused_input), {}),
             before);

    std::ofstream(path, std::ios::binary | std::ios::trunc) << "corrupt";
    auto rebuilt = FullTextIndex::PrepareUpdate(path, sources, documents);
    QCOMPARE(rebuilt->snapshot()->state(), FullTextIndexState::kRebuiltCorrupt);
    std::ifstream corrupt_input(path, std::ios::binary);
    QCOMPARE(std::string(std::istreambuf_iterator<char>(corrupt_input), {}),
             std::string("corrupt"));
}

void FullTextIndexTest::QueryModesAndFilters() {
    TemporaryDirectory directory;
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "reference.gdfts", {},
        {Document("a", "Alpha", "The quick brown fox"),
         Document("b", "Cafe", "A CAFÉ noir")});
    FullTextQuery query;
    query.text = "quick";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kPlainText;
    query.text = "row";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kWildcard;
    query.text = "quick*fox";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kRegularExpression;
    query.text = "brown.+fox";
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.mode = FullTextQueryMode::kWholeWords;
    query.text = "fox quick";
    query.ignore_word_order = true;
    query.maximum_word_distance = 2U;
    QCOMPARE(index.Search(query).results.size(), 1U);
    query.maximum_word_distance = 0U;
    QCOMPARE(index.Search(query).results.size(), 0U);
    query.ignore_word_order = false;
    query.maximum_word_distance.reset();
    query.text = "cafe";
    query.ignore_diacritics = true;
    query.dictionary_filter_active = true;
    query.dictionary_ids = {"b"};
    const auto response = index.Search(query);
    QCOMPARE(response.results.size(), 1U);
    QCOMPARE(response.results.front().dictionary.id, std::string("b"));
    QCOMPARE(response.results.front().match.mode, MatchMode::kFullText);
    QCOMPARE(response.results.front().matches.front().text,
             std::string("CAFÉ"));
}

void FullTextIndexTest::AppliesIndependentIcuNormalizationPolicies() {
    const auto match = [](std::string_view text, std::string_view query,
                          bool match_case, bool ignore_diacritics) {
        return MatchFullText(text,
                             {query, FullTextQueryMode::kPlainText, match_case,
                              ignore_diacritics, false, std::nullopt},
                             nullptr,
                             std::chrono::steady_clock::time_point::max());
    };

    QVERIFY(!match(u8"CAFÉ", u8"café", false, false).empty());
    QVERIFY(match(u8"CAFÉ", u8"café", true, false).empty());
    QVERIFY(!match(u8"CAFÉ", "CAFE", true, true).empty());
    QVERIFY(match(u8"CAFÉ", "cafe", true, true).empty());
    QVERIFY(!match(u8"CAFÉ", "cafe", false, true).empty());
    QVERIFY(match(u8"CAFÉ", "cafe", false, false).empty());

    const std::string reordered = u8"ạ́";
    const auto canonical = match(reordered, u8"ạ́", true, false);
    QCOMPARE(canonical.size(), 1U);
    QCOMPARE(canonical.front().byte_offset, 0U);
    QCOMPARE(canonical.front().byte_length, reordered.size());

    QCOMPARE(NormalizeFullTextQuery(u8"İ", false, false), std::string(u8"i̇"));
    QCOMPARE(NormalizeFullTextQuery(u8"İ", false, true), std::string("i"));
    QCOMPARE(NormalizeFullTextQuery(u8"가", true, false), std::string(u8"가"));
    QCOMPARE(NormalizeFullTextQuery(u8"가", true, false), std::string(u8"가"));
    const auto folded_mark = match(u8"İ", "i", false, true);
    QCOMPARE(folded_mark.size(), 1U);
    QCOMPARE(folded_mark.front().byte_length, std::string(u8"İ").size());
}

void FullTextIndexTest::PreservesNormalizedMatchOriginsAndProgress() {
    const auto match = [](std::string_view text, std::string_view query,
                          FullTextQueryMode mode =
                              FullTextQueryMode::kPlainText,
                          bool match_case = false,
                          bool ignore_diacritics = false) {
        return MatchFullText(
            text,
            {query, mode, match_case, ignore_diacritics, false, std::nullopt},
            nullptr, std::chrono::steady_clock::time_point::max());
    };

    const std::string expansion = u8"ß ß";
    const auto expanded = match(expansion, "s");
    QCOMPARE(expanded.size(), 2U);
    QCOMPARE(expanded[0].byte_offset, 0U);
    QCOMPARE(expanded[0].byte_length, std::string(u8"ß").size());
    QCOMPARE(expansion.substr(expanded[0].byte_offset, expanded[0].byte_length),
             std::string(u8"ß"));
    QCOMPARE(expansion.substr(expanded[1].byte_offset, expanded[1].byte_length),
             std::string(u8"ß"));

    const std::string marks = u8"á का x⃝ .́ ́";
    for (const auto& [query, literal] :
         std::vector<std::pair<std::string, std::string>>{
             {"a", u8"á"}, {u8"क", u8"का"}, {"x", u8"x⃝"}}) {
        const auto ranges =
            match(marks, query, FullTextQueryMode::kPlainText, true, true);
        QCOMPARE(ranges.size(), 1U);
        QCOMPARE(marks.substr(ranges.front().byte_offset,
                              ranges.front().byte_length),
                 literal);
    }
    QVERIFY(
        match(u8"́⃝", u8"́", FullTextQueryMode::kPlainText, true, true).empty());

    const std::string hangul = u8"가";
    const auto contracted =
        match(hangul, u8"가", FullTextQueryMode::kPlainText, true, false);
    QCOMPARE(contracted.size(), 1U);
    QCOMPARE(contracted.front().byte_length, hangul.size());

    const std::string supplementary = u8"😀😀";
    const auto regex =
        match(supplementary, ".", FullTextQueryMode::kRegularExpression, true);
    QCOMPARE(regex.size(), 2U);
    for (const auto& range : regex) {
        QCOMPARE(range.byte_length, std::string(u8"😀").size());
        QCOMPARE(supplementary.substr(range.byte_offset, range.byte_length),
                 std::string(u8"😀"));
    }
    const auto wildcard =
        match(u8"😀", "?", FullTextQueryMode::kWildcard, true);
    QCOMPARE(wildcard.size(), 1U);
    QCOMPARE(wildcard.front().byte_length, std::string(u8"😀").size());

    TemporaryDirectory directory;
    auto index = FullTextIndex::OpenOrBuild(
        directory.path() / "agreement.gdfts", {},
        {Document("agreement", "Agreement", expansion)});
    FullTextQuery query;
    query.text = "ss";
    query.mode = FullTextQueryMode::kRegularExpression;
    const auto indexed = index.Search(query);
    QCOMPARE(indexed.results.size(), 1U);
    QCOMPARE(indexed.results.front().matches.front().byte_offset,
             expanded.front().byte_offset);
    QCOMPARE(indexed.results.front().matches.front().byte_length,
             expanded.front().byte_length);
    QCOMPARE(indexed.results.front().matches.front().text, std::string(u8"ß"));
}

void FullTextIndexTest::PreservesQueryModesAndWordConstraints() {
    const std::string source = "alpha beta gamma alpha gamma beta";
    for (const auto mode :
         {FullTextQueryMode::kWholeWords, FullTextQueryMode::kPlainText}) {
        for (const bool ignore_order : {false, true}) {
            for (const auto distance : {std::optional<std::uint32_t>{},
                                        std::optional<std::uint32_t>{0U},
                                        std::optional<std::uint32_t>{2U}}) {
                const auto ranges = MatchFullText(
                    source,
                    {"alpha beta", mode, true, false, ignore_order, distance},
                    nullptr, std::chrono::steady_clock::time_point::max());
                QVERIFY(!ranges.empty());
            }
        }
    }
    for (const auto mode : {FullTextQueryMode::kWildcard,
                            FullTextQueryMode::kRegularExpression}) {
        const auto query = mode == FullTextQueryMode::kWildcard
                               ? std::string_view("alpha*beta")
                               : std::string_view("alpha.+beta");
        QVERIFY(!MatchFullText(
                     source, {query, mode, true, false, false, std::nullopt},
                     nullptr, std::chrono::steady_clock::time_point::max())
                     .empty());
    }
}

void FullTextIndexTest::RejectsMalformedAndBoundedWork() {
    TemporaryDirectory directory;
    auto index =
        FullTextIndex::OpenOrBuild(directory.path() / "reference.gdfts", {},
                                   {Document("a", "Alpha", "quick brown fox")});
    FullTextQuery query;
    query.text = "[";
    query.mode = FullTextQueryMode::kRegularExpression;
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query.text.assign(kMaximumFullTextQueryBytes + 1U, 'x');
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query = {};
    query.text = "text";
    query.timeout = std::chrono::milliseconds::zero();
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query = {};
    query.text = "*";
    query.mode = FullTextQueryMode::kWildcard;
    query.ignore_word_order = true;
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    query.ignore_word_order = false;
    query.maximum_word_distance = 1U;
    QCOMPARE(index.Search(query).errors.front().code,
             FullTextErrorCode::kInvalidQuery);
    Cancelled cancelled;
    QVERIFY_EXCEPTION_THROWN(
        FullTextIndex::OpenOrBuild(directory.path() / "cancelled", {},
                                   {Document("a", "A", "text")}, &cancelled),
        FullTextIndexError);
}

}  // namespace goldendict::core::dictionary

using goldendict::core::dictionary::FullTextIndexTest;
QTEST_APPLESS_MAIN(FullTextIndexTest)
#include "full_text_index_test.moc"
