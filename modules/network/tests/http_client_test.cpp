// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <chrono>
#include <future>
#include <string>
#include <type_traits>
#include <utility>

#include "../src/http_client.h"
#include "../src/network_cache_storage.h"
#include "../src/network_runtime_test_access.h"
#include "goldendict/network/network_runtime.h"

namespace goldendict::network {
namespace {

class HttpFixture final : public QObject {
   public:
    HttpFixture() {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            while (server_.hasPendingConnections()) {
                QTcpSocket* socket = server_.nextPendingConnection();
                connect(
                    socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                        QByteArray request =
                            socket->property("request").toByteArray();
                        request += socket->readAll();
                        if (!request.contains("\r\n\r\n")) {
                            socket->setProperty("request", request);
                            return;
                        }
                        const QByteArray target = request.split(' ').value(1);
                        QByteArray response;
                        if (target == "/ok") {
                            response =
                                "HTTP/1.1 200 OK\r\nContent-Type: text/plain; "
                                "charset=utf-8\r\nContent-Length: 5\r\n"
                                "Connection: close\r\n\r\nhello";
                        } else if (target == "/cache") {
                            ++cache_requests_;
                            response =
                                "HTTP/1.1 200 OK\r\nContent-Type: "
                                "text/plain\r\n"
                                "Cache-Control: public, max-age=3600\r\n"
                                "Content-Length: 6\r\nConnection: close\r\n\r\n"
                                "cached";
                        } else if (target == "/redirect") {
                            response =
                                "HTTP/1.1 302 Found\r\nLocation: /ok\r\n"
                                "Content-Length: 0\r\nConnection: "
                                "close\r\n\r\n";
                        } else if (target == "/loop") {
                            response =
                                "HTTP/1.1 302 Found\r\nLocation: /loop\r\n"
                                "Content-Length: 0\r\nConnection: "
                                "close\r\n\r\n";
                        } else if (target == "/large") {
                            response =
                                "HTTP/1.1 200 OK\r\nContent-Length: 8\r\n"
                                "Connection: close\r\n\r\n12345678";
                        } else if (target == "/slow") {
                            return;
                        } else if (target == "/authenticated" ||
                                   target == "/authenticated-redirect") {
                            if (request.contains("\r\nAuthorization: Basic "
                                                 "dXNlcjpwYXNz\r\n")) {
                                if (target == "/authenticated-redirect") {
                                    response =
                                        "HTTP/1.1 302 Found\r\nLocation: " +
                                        QByteArray::fromStdString(
                                            cross_origin_target_) +
                                        "\r\nContent-Length: 0\r\n"
                                        "Connection: close\r\n\r\n";
                                } else {
                                    response =
                                        "HTTP/1.1 200 OK\r\nContent-Length: "
                                        "2\r\n"
                                        "Connection: close\r\n\r\nok";
                                }
                            } else {
                                response =
                                    "HTTP/1.1 401 Unauthorized\r\n"
                                    "WWW-Authenticate: Basic "
                                    "realm=\"fixture\"\r\n"
                                    "Content-Length: 0\r\nConnection: "
                                    "close\r\n\r\n";
                            }
                        } else if (target == "/credential-leak") {
                            if (request.contains("\r\nAuthorization:")) {
                                response =
                                    "HTTP/1.1 400 Bad Request\r\n"
                                    "Content-Length: 0\r\nConnection: "
                                    "close\r\n\r\n";
                            } else {
                                response =
                                    "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
                                    "Connection: close\r\n\r\nsafe";
                            }
                        } else {
                            response =
                                "HTTP/1.1 404 Not Found\r\nContent-Length: "
                                "0\r\n"
                                "Connection: close\r\n\r\n";
                        }
                        socket->write(response);
                        socket->disconnectFromHost();
                    });
            }
        });
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            qFatal("Could not start local HTTP fixture");
        }
    }

    std::string Url(std::string_view path) const {
        return "http://127.0.0.1:" +
               std::to_string(static_cast<unsigned int>(server_.serverPort())) +
               std::string(path);
    }

    void SetCrossOriginTarget(std::string target) {
        cross_origin_target_ = std::move(target);
    }

    int cache_requests() const { return cache_requests_; }

   private:
    QTcpServer server_;
    std::string cross_origin_target_;
    int cache_requests_ = 0;
};

class ProxyFixture final : public QObject {
   public:
    ProxyFixture() {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            while (server_.hasPendingConnections()) {
                QTcpSocket* socket = server_.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                    QByteArray request =
                        socket->property("request").toByteArray();
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n")) {
                        socket->setProperty("request", request);
                        return;
                    }
                    QByteArray response;
                    if (!request.contains("\r\nProxy-Authorization: Basic "
                                          "cHJveHk6c2VjcmV0\r\n")) {
                        response =
                            "HTTP/1.1 407 Proxy Authentication Required\r\n"
                            "Proxy-Authenticate: Basic realm=\"proxy\"\r\n"
                            "Content-Length: 0\r\nConnection: close\r\n\r\n";
                    } else if (request.startsWith(
                                   "GET http://example.test/proxied ")) {
                        response =
                            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                            "Content-Length: 7\r\nConnection: close\r\n\r\n"
                            "proxied";
                    } else {
                        response =
                            "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n"
                            "Connection: close\r\n\r\n";
                    }
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            qFatal("Could not start local proxy fixture");
        }
    }

    HttpRequest::Proxy Configuration() const {
        return {"127.0.0.1", static_cast<unsigned short>(server_.serverPort()),
                HttpRequest::Credentials{"proxy", "secret"}};
    }

   private:
    QTcpServer server_;
};

template <typename Callback>
void VerifyError(HttpErrorCode expected, Callback&& callback) {
    try {
        callback();
        QFAIL("Expected HTTP error");
    } catch (const HttpError& error) {
        QCOMPARE(error.code(), expected);
    }
}

HttpResponse FetchWhileProcessingEvents(
    const std::shared_ptr<NetworkRuntime>& runtime,
    const HttpRequest& request) {
    auto future = std::async(std::launch::async, [runtime, request]() {
        return runtime->Fetch(request);
    });
    while (future.wait_for(std::chrono::milliseconds(1)) !=
           std::future_status::ready) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return future.get();
}

}  // namespace

class HttpClientTest : public QObject {
    Q_OBJECT

   private slots:
    void FetchesResponseAndFollowsRelativeRedirect();
    void RejectsInvalidUrlsRedirectLoopsAndHttpErrors();
    void EnforcesResponseAndTimeBudgets();
    void ObservesCancellation();
    void SupportsScopedOriginAndProxyAuthentication();
    void EnforcesMoveOnlyStorageLease();
    void RejectsDuplicateRuntimeStorageAuthority();
    void PreparesAndCleansOnlyOwnedCacheStorage();
    void OwnsExactIsolatedPersistentCache();
    void DisablesAndClearsOnlyOwnedCache();
    void CancelsAndJoinsBeforeShutdownCleanup();
    void ActivatesPreparedPolicyWithExistingStorageLease();
    void ReusesLifetimeBoundCacheAcrossZeroAndPositivePolicies();
    void PreparesMoveOnlyCandidatesOnOwnerThreadWithoutActiveMutation();
    void PublishesPreparedCandidatesAndOrdersPostWork();
    void ReservesPreparedPublicationBeforeDecision();
    void KeepsOwnerEventLoopResponsiveAtReadyBarrier();
    void RunsDispatcherTimerOnlyForPreparedCandidates();
    void RejectsUnreadyAndSupportsOwnerThreadCommit();
    void ResolvesCandidateDuringConcurrentShutdown();
    void RejectsInvalidCrossRuntimeStaleAndReusedCandidates();
    void AbandonsCandidatesOnOwnerThreadWithoutStorageMutation();
};

void HttpClientTest::EnforcesMoveOnlyStorageLease() {
    static_assert(
        !std::is_copy_constructible_v<NetworkCacheStorageSlot::Lease>);
    static_assert(!std::is_copy_assignable_v<NetworkCacheStorageSlot::Lease>);
    static_assert(std::is_move_constructible_v<NetworkCacheStorageSlot::Lease>);

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const std::string owned =
        QDir(root.path()).filePath("qt-network-http").toStdString();
    NetworkCacheStorageSlot first_slot(owned);
    NetworkCacheStorageSlot duplicate_slot(owned);

    auto first = first_slot.Acquire();
    QVERIFY(first);
    QVERIFY(!duplicate_slot.Acquire());

    auto moved = std::move(first);
    QVERIFY(!first);
    QVERIFY(moved);
    QVERIFY(!first.Release());
    QVERIFY(moved.Release());
    QVERIFY(!moved.Release());

    auto replacement = duplicate_slot.Acquire();
    QVERIFY(replacement);
    QVERIFY(!moved.Release());
    QVERIFY(!first_slot.Acquire());
    QVERIFY(replacement.Release());
}

void HttpClientTest::
    PreparesMoveOnlyCandidatesOnOwnerThreadWithoutActiveMutation() {
    static_assert(
        !std::is_copy_constructible_v<NetworkRuntime::PreparedCandidate>);
    static_assert(
        !std::is_copy_assignable_v<NetworkRuntime::PreparedCandidate>);
    static_assert(std::is_nothrow_move_constructible_v<
                  NetworkRuntime::PreparedCandidate>);
    static_assert(
        std::is_nothrow_move_assignable_v<NetworkRuntime::PreparedCandidate>);
    static_assert(
        !std::is_copy_constructible_v<NetworkRuntime::CommitReservation>);
    static_assert(std::is_nothrow_move_constructible_v<
                  NetworkRuntime::CommitReservation>);

    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    const QString owned = QString::fromStdString(runtime->cache_directory());
    QFile sentinel(QDir(owned).filePath("active-candidate-sentinel"));
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write("active", 6), qint64{6});
    sentinel.close();

    auto positive_preparation =
        NetworkRuntime::Prepare({3U, true}, root.path().toStdString());
    auto positive = runtime->PrepareCandidate(std::move(positive_preparation));
    QVERIFY(positive);
    QVERIFY(NetworkRuntimeTestAccess::IsCurrent(*runtime, positive));
    QVERIFY(NetworkRuntimeTestAccess::WasConstructedOnOwnerThread(*runtime,
                                                                  positive));
    QCOMPARE(NetworkRuntimeTestAccess::MaximumCacheBytes(positive),
             3LL * 1024LL * 1024LL);
    QCOMPARE(NetworkRuntimeTestAccess::CacheDirectory(positive), std::string());
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);
    QVERIFY(sentinel.exists());

    auto moved = std::move(positive);
    QVERIFY(!positive);
    QVERIFY(moved);
    QVERIFY(NetworkRuntimeTestAccess::IsCurrent(*runtime, moved));

    auto zero_preparation =
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString());
    auto zero = runtime->PrepareCandidate(std::move(zero_preparation));
    QVERIFY(zero);
    QVERIFY(
        NetworkRuntimeTestAccess::WasConstructedOnOwnerThread(*runtime, zero));
    QCOMPARE(NetworkRuntimeTestAccess::MaximumCacheBytes(zero), 0);
    QCOMPARE(NetworkRuntimeTestAccess::CacheDirectory(zero), std::string());
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);
    QVERIFY(QDir(owned).exists());
    QVERIFY(sentinel.exists());
}

void HttpClientTest::RejectsInvalidCrossRuntimeStaleAndReusedCandidates() {
    QTemporaryDir root;
    QTemporaryDir other_root;
    QVERIFY(root.isValid());
    QVERIFY(other_root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    auto other = NetworkRuntime::Create(
        NetworkRuntime::Prepare({4U, false}, other_root.path().toStdString()));

    NetworkRuntime::PreparedCandidate invalid;
    QVERIFY(!invalid);
    QVERIFY(!NetworkRuntimeTestAccess::Consume(*runtime, invalid));

    auto wrong_directory = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({1U, false}, other_root.path().toStdString()));
    QVERIFY(!wrong_directory);

    QFile unavailable_root(QDir(root.path()).filePath("not-a-directory"));
    QVERIFY(unavailable_root.open(QIODevice::WriteOnly));
    unavailable_root.close();
    auto unavailable_preparation = NetworkRuntime::Prepare(
        {1U, false}, unavailable_root.fileName().toStdString());
    QVERIFY(!unavailable_preparation.cache_available);
    auto unavailable =
        runtime->PrepareCandidate(std::move(unavailable_preparation));
    QVERIFY(!unavailable);

    auto candidate = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({2U, false}, root.path().toStdString()));
    QVERIFY(candidate);
    QVERIFY(!NetworkRuntimeTestAccess::IsCurrent(*other, candidate));
    QVERIFY(!NetworkRuntimeTestAccess::Consume(*other, candidate));
    QVERIFY(NetworkRuntimeTestAccess::Consume(*runtime, candidate));
    QVERIFY(!candidate);
    QVERIFY(!NetworkRuntimeTestAccess::Consume(*runtime, candidate));
    QCOMPARE(runtime->maximum_cache_bytes(), 2LL * 1024LL * 1024LL);

    auto stale = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({3U, false}, root.path().toStdString()));
    QVERIFY(stale);
    QVERIFY(runtime->Activate(
        NetworkRuntime::Prepare({7U, false}, root.path().toStdString())));
    QVERIFY(!NetworkRuntimeTestAccess::IsCurrent(*runtime, stale));
    QVERIFY(!NetworkRuntimeTestAccess::Consume(*runtime, stale));
    QCOMPARE(runtime->maximum_cache_bytes(), 7LL * 1024LL * 1024LL);

    auto shutdown_candidate = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({5U, false}, root.path().toStdString()));
    QVERIFY(shutdown_candidate);
    bool shutdown_cleanup_on_owner_thread = false;
    NetworkRuntimeTestAccess::ObserveDestruction(
        shutdown_candidate, [&](bool on_owner_thread) {
            shutdown_cleanup_on_owner_thread = on_owner_thread;
        });
    runtime->Shutdown();
    QVERIFY(shutdown_cleanup_on_owner_thread);
    QVERIFY(!NetworkRuntimeTestAccess::IsCurrent(*runtime, shutdown_candidate));
    QVERIFY(!NetworkRuntimeTestAccess::Consume(*runtime, shutdown_candidate));
}

void HttpClientTest::AbandonsCandidatesOnOwnerThreadWithoutStorageMutation() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    const QString owned = QString::fromStdString(runtime->cache_directory());
    QFile sentinel(QDir(owned).filePath("abandoned-candidate-sentinel"));
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write("active", 6), qint64{6});
    sentinel.close();

    bool destroyed_on_owner_thread = false;
    {
        auto candidate = runtime->PrepareCandidate(
            NetworkRuntime::Prepare({1U, false}, root.path().toStdString()));
        QVERIFY(candidate);
        NetworkRuntimeTestAccess::ObserveDestruction(
            candidate, [&](bool on_owner_thread) {
                destroyed_on_owner_thread = on_owner_thread;
            });
    }
    QVERIFY(destroyed_on_owner_thread);
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);
    QVERIFY(QDir(owned).exists());
    QVERIFY(sentinel.exists());

    auto owner_destroyed = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({2U, false}, root.path().toStdString()));
    QVERIFY(owner_destroyed);
    bool direct_abort_on_owner = false;
    NetworkRuntimeTestAccess::ObserveDestruction(
        owner_destroyed,
        [&](bool on_owner_thread) { direct_abort_on_owner = on_owner_thread; });
    QVERIFY(NetworkRuntimeTestAccess::DestroyOnOwnerThread(*runtime,
                                                           owner_destroyed));
    QVERIFY(!owner_destroyed);
    QVERIFY(direct_abort_on_owner);
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);
    QVERIFY(sentinel.exists());
}

void HttpClientTest::PublishesPreparedCandidatesAndOrdersPostWork() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    const void* const cache =
        NetworkRuntimeTestAccess::BoundDiskCache(*runtime);
    const auto directory_count =
        NetworkRuntimeTestAccess::DirectoryConfigurationCount(*runtime);

    auto positive = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({3U, true}, root.path().toStdString()));
    QVERIFY(positive);
    QCOMPARE(NetworkRuntimeTestAccess::DirectoryConfigurationCount(*runtime),
             directory_count + 1U);
    bool positive_published = false;
    NetworkRuntimeTestAccess::ObservePublication(positive, [&]() {
        positive_published = true;
        QCOMPARE(runtime->maximum_cache_bytes(), 3LL * 1024LL * 1024LL);
        QCOMPARE(NetworkRuntimeTestAccess::BoundDiskCache(*runtime), cache);
    });
    QCOMPARE(runtime->Commit(positive),
             NetworkRuntime::CommitResult::kPublished);
    QVERIFY(positive_published);
    QVERIFY(!positive);
    QCOMPARE(NetworkRuntimeTestAccess::DirectoryConfigurationCount(*runtime),
             directory_count + 1U);

    const QString owned = QString::fromStdString(runtime->cache_directory());
    QFile sentinel(QDir(owned).filePath("post-publication-sentinel"));
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write("cache", 5), qint64{5});
    sentinel.close();
    auto zero = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString()));
    QVERIFY(zero);
    bool zero_published_before_cleanup = false;
    NetworkRuntimeTestAccess::ObservePublication(zero, [&]() {
        zero_published_before_cleanup = true;
        QCOMPARE(runtime->maximum_cache_bytes(), 0);
        QVERIFY(sentinel.exists());
        QCOMPARE(NetworkRuntimeTestAccess::BoundDiskCache(*runtime), cache);
    });
    QCOMPARE(runtime->Commit(zero), NetworkRuntime::CommitResult::kPublished);
    QVERIFY(zero_published_before_cleanup);
    QVERIFY(!QDir(owned).exists());
    QCOMPARE(NetworkRuntimeTestAccess::BoundDiskCache(*runtime), cache);
    QCOMPARE(NetworkRuntimeTestAccess::DirectoryConfigurationCount(*runtime),
             directory_count + 1U);

    auto restored = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({1U, false}, root.path().toStdString()));
    QCOMPARE(runtime->Commit(restored),
             NetworkRuntime::CommitResult::kPublished);
    auto maintenance_failure = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString()));
    QVERIFY(maintenance_failure);
    NetworkRuntimeTestAccess::ForcePostWorkFailure(maintenance_failure);
    QCOMPARE(runtime->Commit(maintenance_failure),
             NetworkRuntime::CommitResult::kPublishedWithPostWorkFailure);
    QCOMPARE(runtime->maximum_cache_bytes(), 0);
    QCOMPARE(runtime->diagnostic(),
             std::string(NetworkCacheStorage::CleanupDiagnostic()));
}

void HttpClientTest::ReservesPreparedPublicationBeforeDecision() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));

    auto abort_candidate = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({4U, false}, root.path().toStdString()));
    auto aborted = runtime->Reserve(abort_candidate);
    QVERIFY(aborted);
    QVERIFY(!abort_candidate);
    QVERIFY(!runtime->PrepareCandidate(
        NetworkRuntime::Prepare({3U, false}, root.path().toStdString())));
    HttpRequest blocked;
    blocked.url = "http://127.0.0.1/";
    VerifyError(HttpErrorCode::kCancelled,
                [&]() { static_cast<void>(runtime->Fetch(blocked)); });
    runtime->Shutdown();
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);
    runtime->Abort(aborted);
    QVERIFY(!aborted);

    auto publish_candidate = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({2U, false}, root.path().toStdString()));
    auto reserved = runtime->Reserve(publish_candidate);
    QVERIFY(reserved);
    QCOMPARE(runtime->Publish(reserved),
             NetworkRuntime::CommitResult::kPublished);
    QVERIFY(!reserved);
    QCOMPARE(runtime->maximum_cache_bytes(), 2LL * 1024LL * 1024LL);
}

void HttpClientTest::KeepsOwnerEventLoopResponsiveAtReadyBarrier() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    auto candidate = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({4U, false}, root.path().toStdString()));
    QVERIFY(candidate);

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/ok");
    request.runtime = runtime;
    const auto response = FetchWhileProcessingEvents(runtime, request);
    QCOMPARE(response.status_code, 200);
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);
}

void HttpClientTest::RunsDispatcherTimerOnlyForPreparedCandidates() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));

    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerCreationCount(*runtime),
             1U);
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerConnectionCount(*runtime),
             1U);
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerStartCount(*runtime), 0U);
    QVERIFY(!NetworkRuntimeTestAccess::DispatcherTimerActive(*runtime));
    const auto idle_wakeups =
        NetworkRuntimeTestAccess::DispatcherTimerWakeupCount(*runtime);
    QTest::qWait(10);
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerWakeupCount(*runtime),
             idle_wakeups);

    auto committed = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({4U, false}, root.path().toStdString()));
    QVERIFY(committed);
    QVERIFY(NetworkRuntimeTestAccess::DispatcherTimerActive(*runtime));
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerStartCount(*runtime), 1U);
    QTest::qWait(5);
    QVERIFY(NetworkRuntimeTestAccess::DispatcherTimerWakeupCount(*runtime) >
            idle_wakeups);
    const auto creations_before_commit =
        NetworkRuntimeTestAccess::DispatcherTimerCreationCount(*runtime);
    const auto connections_before_commit =
        NetworkRuntimeTestAccess::DispatcherTimerConnectionCount(*runtime);
    const auto starts_before_commit =
        NetworkRuntimeTestAccess::DispatcherTimerStartCount(*runtime);
    QCOMPARE(runtime->Commit(committed),
             NetworkRuntime::CommitResult::kPublished);
    QVERIFY(!NetworkRuntimeTestAccess::DispatcherTimerActive(*runtime));
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerCreationCount(*runtime),
             creations_before_commit);
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerConnectionCount(*runtime),
             connections_before_commit);
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerStartCount(*runtime),
             starts_before_commit);

    auto aborted = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({2U, false}, root.path().toStdString()));
    QVERIFY(aborted);
    QVERIFY(NetworkRuntimeTestAccess::DispatcherTimerActive(*runtime));
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerStartCount(*runtime),
             starts_before_commit + 1U);
    QVERIFY(NetworkRuntimeTestAccess::DestroyOnOwnerThread(*runtime, aborted));
    QVERIFY(!NetworkRuntimeTestAccess::DispatcherTimerActive(*runtime));
    const auto terminal_wakeups =
        NetworkRuntimeTestAccess::DispatcherTimerWakeupCount(*runtime);
    QTest::qWait(10);
    QCOMPARE(NetworkRuntimeTestAccess::DispatcherTimerWakeupCount(*runtime),
             terminal_wakeups);
}

void HttpClientTest::RejectsUnreadyAndSupportsOwnerThreadCommit() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));

    auto unready = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({2U, false}, root.path().toStdString()));
    QVERIFY(unready);
    NetworkRuntimeTestAccess::MakeUnready(unready);
    QCOMPARE(runtime->Commit(unready), NetworkRuntime::CommitResult::kRejected);
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);

    auto wrong_lease = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({4U, false}, root.path().toStdString()));
    QVERIFY(wrong_lease);
    NetworkRuntimeTestAccess::InvalidateLeaseIdentity(wrong_lease);
    QCOMPARE(runtime->Commit(wrong_lease),
             NetworkRuntime::CommitResult::kRejected);
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);

    auto owner_commit = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({5U, false}, root.path().toStdString()));
    QVERIFY(owner_commit);
    QCOMPARE(
        NetworkRuntimeTestAccess::CommitOnOwnerThread(*runtime, owner_commit),
        NetworkRuntime::CommitResult::kPublished);
    QCOMPARE(runtime->maximum_cache_bytes(), 5LL * 1024LL * 1024LL);
}

void HttpClientTest::ResolvesCandidateDuringConcurrentShutdown() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    auto candidate = runtime->PrepareCandidate(
        NetworkRuntime::Prepare({3U, false}, root.path().toStdString()));
    QVERIFY(candidate);
    std::atomic<bool> finalized_on_owner{false};
    NetworkRuntimeTestAccess::ObserveDestruction(
        candidate, [&](bool on_owner_thread) {
            finalized_on_owner.store(on_owner_thread);
        });

    auto shutdown =
        std::async(std::launch::async, [runtime]() { runtime->Shutdown(); });
    candidate = NetworkRuntime::PreparedCandidate{};
    QCOMPARE(shutdown.wait_for(std::chrono::seconds(2)),
             std::future_status::ready);
    shutdown.get();
    QVERIFY(finalized_on_owner.load());
    QVERIFY(!candidate);
}

void HttpClientTest::RejectsDuplicateRuntimeStorageAuthority() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto active = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    const QString owned = QString::fromStdString(active->cache_directory());
    QFile sentinel(QDir(owned).filePath("active-storage-sentinel"));
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write("active", 6), qint64{6});
    sentinel.close();

    auto abandoned =
        NetworkRuntime::Prepare({1U, false}, root.path().toStdString());
    QVERIFY(abandoned.cache_available);
    QVERIFY(sentinel.exists());

    auto duplicate = NetworkRuntime::Create(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString()));
    QCOMPARE(duplicate->diagnostic(),
             std::string(NetworkCacheStorage::SetupDiagnostic()));
    QVERIFY(QDir(owned).exists());
    QVERIFY(sentinel.exists());
    QVERIFY(!duplicate->Activate(std::move(abandoned)));
    QVERIFY(QDir(owned).exists());
    QVERIFY(sentinel.exists());

    duplicate->Shutdown();
    active->Shutdown();
    QVERIFY(QDir(owned).exists());
    QVERIFY(sentinel.exists());

    auto restarted = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    QCOMPARE(restarted->diagnostic(), std::string());
    QVERIFY(sentinel.exists());
    auto disabled =
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString());
    QVERIFY(restarted->Activate(std::move(disabled)));
    QVERIFY(!QDir(owned).exists());
}

void HttpClientTest::FetchesResponseAndFollowsRelativeRedirect() {
    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/redirect");

    const HttpResponse response = FetchHttp(request);

    QCOMPARE(response.status_code, 200);
    QCOMPARE(response.content_type, std::string("text/plain; charset=utf-8"));
    QCOMPARE(std::string(response.body.begin(), response.body.end()),
             std::string("hello"));
    QVERIFY(response.final_url.size() >= 3U);
    QCOMPARE(response.final_url.substr(response.final_url.size() - 3U),
             std::string("/ok"));
}

void HttpClientTest::RejectsInvalidUrlsRedirectLoopsAndHttpErrors() {
    HttpRequest invalid_url;
    invalid_url.url = "file:///etc/passwd";
    VerifyError(HttpErrorCode::kInvalidUrl, [&]() { FetchHttp(invalid_url); });
    invalid_url.url = "http://user:secret@example.test/";
    VerifyError(HttpErrorCode::kInvalidUrl, [&]() { FetchHttp(invalid_url); });
    HttpRequest invalid_request;
    invalid_request.url = "http://example.test/";
    invalid_request.maximum_response_bytes = 0U;
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { FetchHttp(invalid_request); });

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/loop");
    request.maximum_redirects = 1U;
    VerifyError(HttpErrorCode::kTooManyRedirects,
                [&]() { FetchHttp(request); });

    request.url = fixture.Url("/missing");
    VerifyError(HttpErrorCode::kHttpStatus, [&]() { FetchHttp(request); });
}

void HttpClientTest::EnforcesResponseAndTimeBudgets() {
    HttpFixture fixture;
    HttpRequest large;
    large.url = fixture.Url("/large");
    large.maximum_response_bytes = 4U;
    VerifyError(HttpErrorCode::kResponseTooLarge, [&]() { FetchHttp(large); });

    HttpRequest slow;
    slow.url = fixture.Url("/slow");
    slow.timeout = std::chrono::milliseconds(30);
    VerifyError(HttpErrorCode::kDeadlineExceeded, [&]() { FetchHttp(slow); });
}

void HttpClientTest::ObservesCancellation() {
    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/slow");
    int polls = 0;
    VerifyError(HttpErrorCode::kCancelled,
                [&]() { FetchHttp(request, [&]() { return ++polls > 1; }); });
}

void HttpClientTest::SupportsScopedOriginAndProxyAuthentication() {
    HttpFixture fixture;
    HttpRequest origin_request;
    origin_request.url = fixture.Url("/authenticated");
    origin_request.credentials = HttpRequest::Credentials{"user", "pass"};
    const HttpResponse origin_response = FetchHttp(origin_request);
    QCOMPARE(
        std::string(origin_response.body.begin(), origin_response.body.end()),
        std::string("ok"));

    HttpFixture cross_origin;
    fixture.SetCrossOriginTarget(cross_origin.Url("/credential-leak"));
    origin_request.url = fixture.Url("/authenticated-redirect");
    const HttpResponse redirected_response = FetchHttp(origin_request);
    QCOMPARE(std::string(redirected_response.body.begin(),
                         redirected_response.body.end()),
             std::string("safe"));

    ProxyFixture proxy;
    HttpRequest proxy_request;
    proxy_request.url = "http://example.test/proxied";
    proxy_request.proxy = proxy.Configuration();
    const HttpResponse proxy_response = FetchHttp(proxy_request);
    QCOMPARE(
        std::string(proxy_response.body.begin(), proxy_response.body.end()),
        std::string("proxied"));

    HttpRequest invalid_proxy = proxy_request;
    invalid_proxy.proxy->port = 0U;
    VerifyError(HttpErrorCode::kInvalidRequest,
                [&]() { FetchHttp(invalid_proxy); });

    HttpRequest redacted_proxy;
    redacted_proxy.url = "http://example.test/";
    redacted_proxy.timeout = std::chrono::milliseconds(100);
    redacted_proxy.proxy = HttpRequest::Proxy{
        "secret-proxy.invalid", 3128U,
        HttpRequest::Credentials{"secret-user", "secret-password"}};
    try {
        static_cast<void>(FetchHttp(redacted_proxy));
        QFAIL("Expected proxy transport error");
    } catch (const HttpError& error) {
        QCOMPARE(error.code(), HttpErrorCode::kTransport);
        const QByteArray message(error.what());
        QVERIFY(!message.contains("secret-proxy"));
        QVERIFY(!message.contains("secret-user"));
        QVERIFY(!message.contains("secret-password"));
    }
}

void HttpClientTest::PreparesAndCleansOnlyOwnedCacheStorage() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile sibling(QDir(root.path()).filePath("webengine-sentinel"));
    QVERIFY(sibling.open(QIODevice::WriteOnly));
    QCOMPARE(sibling.write("browser"), qint64{7});
    sibling.close();

    const auto prepared =
        NetworkRuntime::Prepare({1U, false}, root.path().toStdString());
    QVERIFY(prepared.cache_available);
    QCOMPARE(prepared.cache_directory,
             QDir(root.path()).filePath("qt-network-http").toStdString());
    QVERIFY(QDir(QString::fromStdString(prepared.cache_directory)).exists());
    QVERIFY(
        !QFile::exists(QDir(QString::fromStdString(prepared.cache_directory))
                           .filePath(".goldendict-write-test")));

    auto disabled = NetworkRuntime::Create(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString()));
    QVERIFY(
        !QDir(QString::fromStdString(disabled->cache_directory())).exists());
    QVERIFY(sibling.exists());
    disabled.reset();

    auto idempotent = NetworkRuntime::Create(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString()));
    QVERIFY(
        !QDir(QString::fromStdString(idempotent->cache_directory())).exists());
    QVERIFY(sibling.exists());
}

void HttpClientTest::OwnsExactIsolatedPersistentCache() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto preparation =
        NetworkRuntime::Prepare({7U, false}, root.path().toStdString());
    QVERIFY(preparation.cache_available);
    QCOMPARE(preparation.cache_directory,
             QDir(root.path()).filePath("qt-network-http").toStdString());
    auto runtime = NetworkRuntime::Create(std::move(preparation));
    QCOMPARE(runtime->maximum_cache_bytes(), 7LL * 1024LL * 1024LL);

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/cache");
    request.runtime = runtime;
    const auto first = FetchWhileProcessingEvents(runtime, request);
    const auto second = FetchWhileProcessingEvents(runtime, request);
    QCOMPARE(std::string(first.body.begin(), first.body.end()),
             std::string("cached"));
    QCOMPARE(std::string(second.body.begin(), second.body.end()),
             std::string("cached"));
    QCOMPARE(fixture.cache_requests(), 1);
    request.runtime.reset();
    runtime.reset();

    auto restarted = NetworkRuntime::Create(
        NetworkRuntime::Prepare({7U, false}, root.path().toStdString()));
    request.runtime = restarted;
    const auto response = FetchWhileProcessingEvents(restarted, request);
    QCOMPARE(std::string(response.body.begin(), response.body.end()),
             std::string("cached"));
    QCOMPARE(fixture.cache_requests(), 1);
}

void HttpClientTest::DisablesAndClearsOnlyOwnedCache() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile sibling(QDir(root.path()).filePath("webengine-sentinel"));
    QVERIFY(sibling.open(QIODevice::WriteOnly));
    QCOMPARE(sibling.write("browser"), qint64{7});
    sibling.close();

    auto disabled = NetworkRuntime::Create(
        NetworkRuntime::Prepare({0U, true}, root.path().toStdString()));
    QCOMPARE(disabled->maximum_cache_bytes(), 0);
    QVERIFY(
        !QDir(QString::fromStdString(disabled->cache_directory())).exists());
    disabled.reset();
    QVERIFY(sibling.exists());

    auto clearing = NetworkRuntime::Create(
        NetworkRuntime::Prepare({1U, true}, root.path().toStdString()));
    const QString owned = QString::fromStdString(clearing->cache_directory());
    QVERIFY(QDir(owned).exists());
    clearing.reset();
    QVERIFY(!QDir(owned).exists());
    QVERIFY(sibling.exists());

    const auto failed =
        NetworkRuntime::Prepare({1U, false}, sibling.fileName().toStdString());
    QVERIFY(!failed.cache_available);
    QCOMPARE(failed.diagnostic,
             std::string("Qt Network cache setup failed; HTTP traffic will "
                         "remain uncached"));
    const auto missing_root = NetworkRuntime::Prepare({1U, false}, {});
    QVERIFY(!missing_root.cache_available);
    QVERIFY(missing_root.cache_directory.empty());
}

void HttpClientTest::CancelsAndJoinsBeforeShutdownCleanup() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({10240U, true}, root.path().toStdString()));
    QCOMPARE(runtime->maximum_cache_bytes(), 10240LL * 1024LL * 1024LL);
    const QString owned = QString::fromStdString(runtime->cache_directory());

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/slow");
    request.timeout = std::chrono::seconds(5);
    auto fetch = std::async(std::launch::async, [runtime, request]() {
        try {
            static_cast<void>(runtime->Fetch(request));
            return false;
        } catch (const HttpError& error) {
            return error.code() == HttpErrorCode::kCancelled;
        }
    });
    QTest::qWait(20);
    runtime->Shutdown();
    QVERIFY(fetch.get());
    QVERIFY(!QDir(owned).exists());

    HttpRequest rejected;
    rejected.url = fixture.Url("/ok");
    VerifyError(HttpErrorCode::kCancelled,
                [&]() { static_cast<void>(runtime->Fetch(rejected)); });
}

void HttpClientTest::ActivatesPreparedPolicyWithExistingStorageLease() {
    QTemporaryDir root;
    QTemporaryDir other_root;
    QVERIFY(root.isValid());
    QVERIFY(other_root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));

    auto wrong_owner =
        NetworkRuntime::Prepare({1U, false}, other_root.path().toStdString());
    QVERIFY(!runtime->Activate(std::move(wrong_owner)));
    QCOMPARE(runtime->maximum_cache_bytes(), 8LL * 1024LL * 1024LL);

    auto reduced =
        NetworkRuntime::Prepare({1U, false}, root.path().toStdString());
    QVERIFY(runtime->Activate(std::move(reduced)));
    QCOMPARE(runtime->maximum_cache_bytes(), 1LL * 1024LL * 1024LL);

    const QString owned = QString::fromStdString(runtime->cache_directory());
    QVERIFY(QDir(owned).exists());
    auto disabled =
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString());
    QVERIFY(runtime->Activate(std::move(disabled)));
    QCOMPARE(runtime->maximum_cache_bytes(), 0);
    QVERIFY(!QDir(owned).exists());
}

void HttpClientTest::ReusesLifetimeBoundCacheAcrossZeroAndPositivePolicies() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto runtime = NetworkRuntime::Create(
        NetworkRuntime::Prepare({8U, false}, root.path().toStdString()));
    const void* const cache =
        NetworkRuntimeTestAccess::BoundDiskCache(*runtime);
    QVERIFY(cache != nullptr);
    QCOMPARE(NetworkRuntimeTestAccess::DirectoryConfigurationCount(*runtime),
             1U);

    QVERIFY(runtime->Activate(
        NetworkRuntime::Prepare({0U, false}, root.path().toStdString())));
    QCOMPARE(NetworkRuntimeTestAccess::BoundDiskCache(*runtime), cache);
    QCOMPARE(runtime->maximum_cache_bytes(), 0);
    const QString owned = QString::fromStdString(runtime->cache_directory());
    QVERIFY(!QDir(owned).exists());

    {
        auto abandoned = runtime->PrepareCandidate(
            NetworkRuntime::Prepare({3U, false}, root.path().toStdString()));
        QVERIFY(abandoned);
        QCOMPARE(NetworkRuntimeTestAccess::BoundDiskCache(*runtime), cache);
        QCOMPARE(
            NetworkRuntimeTestAccess::DirectoryConfigurationCount(*runtime),
            2U);
        QCOMPARE(runtime->maximum_cache_bytes(), 0);
    }
    QCOMPARE(runtime->maximum_cache_bytes(), 0);

    QVERIFY(runtime->Activate(
        NetworkRuntime::Prepare({3U, false}, root.path().toStdString())));
    QCOMPARE(NetworkRuntimeTestAccess::BoundDiskCache(*runtime), cache);
    QCOMPARE(NetworkRuntimeTestAccess::DirectoryConfigurationCount(*runtime),
             3U);
    QCOMPARE(runtime->maximum_cache_bytes(), 3LL * 1024LL * 1024LL);

    HttpFixture fixture;
    HttpRequest request;
    request.url = fixture.Url("/cache");
    request.runtime = runtime;
    static_cast<void>(FetchWhileProcessingEvents(runtime, request));
    static_cast<void>(FetchWhileProcessingEvents(runtime, request));
    QCOMPARE(fixture.cache_requests(), 1);
}

}  // namespace goldendict::network

using goldendict::network::HttpClientTest;

QTEST_GUILESS_MAIN(HttpClientTest)

#include "http_client_test.moc"
