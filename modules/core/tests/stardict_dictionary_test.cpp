// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "../src/formats/stardict/stardict_dictionary.h"
#include "support/stardict_fixture.h"

namespace goldendict::core::formats::stardict {
namespace {

class CancelledSignal final : public dictionary::CancellationSignal {
   public:
    bool IsCancellationRequested() const noexcept override { return true; }
};

class StardictDictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void ExposesIdentityAndBoundedArticles();
    void PreservesFormattedArticleData();
    void HonorsCancellationAndDeadline();
    void TranslatesReaderFailures();
    void LoadsTypedResourcesAndLegacyDelimiters();
    void ReturnsMissingResourceWithoutAnError();
    void RejectsUnsafeResourcePaths();
    void RejectsResourceSymlinkEscapes();
    void RejectsOversizedResources();
};

std::filesystem::path TemporaryPath(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

void StardictDictionaryTest::ExposesIdentityAndBoundedArticles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(
        TemporaryPath(directory),
        {{"example", "first"}, {"example", "second"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    dictionary::RequestOptions options;
    options.result_limit = 1;

    const auto articles = dictionary.LookupExact("example", options);

    QCOMPARE(dictionary.identity().id, "fixture-id");
    QCOMPARE(dictionary.identity().name, "Generated Test Dictionary");
    QCOMPARE(dictionary.identity().source, info_path.string());
    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().headword, "example");
    QCOMPARE(articles.front().format, "stardict/m");
    QCOMPARE(articles.front().data, "first");
}

void StardictDictionaryTest::PreservesFormattedArticleData() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto html =
        "<p><b>Example</b> <a href=\"bword://linked\">linked</a>"
        "<img src=\"images/pixel.png\"></p>";
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", html}}, "h");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const auto articles = dictionary.LookupExact("example");

    QCOMPARE(articles.size(), std::size_t{1});
    QCOMPARE(articles.front().format, "stardict/h");
    QCOMPARE(articles.front().data, html);
}

void StardictDictionaryTest::HonorsCancellationAndDeadline() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);
    CancelledSignal cancellation;
    dictionary::RequestOptions cancelled;
    cancelled.cancellation = &cancellation;

    try {
        static_cast<void>(dictionary.LookupExact("example", cancelled));
        QFAIL("LookupExact should honor cancellation");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
    }
    try {
        static_cast<void>(dictionary.GetResource("resource.bin", cancelled));
        QFAIL("GetResource should honor cancellation");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kCancelled);
    }

    dictionary::RequestOptions expired;
    expired.deadline = std::chrono::steady_clock::time_point::min();
    try {
        static_cast<void>(dictionary.LookupExact("example", expired));
        QFAIL("LookupExact should honor an expired deadline");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kDeadlineExceeded);
    }
}

void StardictDictionaryTest::TranslatesReaderFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto missing = TemporaryPath(directory) / "missing.ifo";

    try {
        static_cast<void>(Dictionary::Open("missing", missing));
        QFAIL("Dictionary::Open should translate missing input");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kUnavailable);
    }
}

void StardictDictionaryTest::LoadsTypedResourcesAndLegacyDelimiters() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const std::string image_data("\x89PNG\r\n\x1a\nfixture", 15);
    test::WriteStardictResource(root, "images/pixel.png", image_data);
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const auto resource = dictionary.GetResource("\x1eimages/pixel.png\x1f");

    QVERIFY(resource.has_value());
    QCOMPARE(resource->id, "images/pixel.png");
    QCOMPARE(resource->media_type, "image/png");
    const std::string loaded(
        reinterpret_cast<const char*>(resource->data.data()),
        resource->data.size());
    QCOMPARE(loaded, image_data);
}

void StardictDictionaryTest::ReturnsMissingResourceWithoutAnError() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto info_path = test::WriteStardictFixture(TemporaryPath(directory),
                                                      {{"example", "article"}});
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    QVERIFY(!dictionary.GetResource("images/missing.png").has_value());
}

void StardictDictionaryTest::RejectsUnsafeResourcePaths() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    test::WriteStardictResource(root, "safe.txt", "safe");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    const std::vector<std::string> unsafe_paths = {
        "../fixture.dict", "images/../../fixture.dict", "/etc/passwd",
        "..\\fixture.dict"};
    for (const auto& path : unsafe_paths) {
        try {
            static_cast<void>(dictionary.GetResource(path));
            QFAIL("GetResource should reject an unsafe path");
        } catch (const dictionary::Error& error) {
            QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
        }
    }
}

void StardictDictionaryTest::RejectsResourceSymlinkEscapes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto outside = root / "outside.txt";
    test::WriteBinaryFile(outside, "outside");
    const auto resource_root = root / "res";
    QVERIFY(std::filesystem::create_directory(resource_root));
    std::filesystem::create_symlink(outside, resource_root / "escape.txt");
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    try {
        static_cast<void>(dictionary.GetResource("escape.txt"));
        QFAIL("GetResource should reject a symlink escape");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

void StardictDictionaryTest::RejectsOversizedResources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = TemporaryPath(directory);
    const auto info_path =
        test::WriteStardictFixture(root, {{"example", "article"}});
    const auto resource = test::WriteStardictResource(root, "large.bin", "");
    std::filesystem::resize_file(resource, 64U * 1024U * 1024U + 1U);
    const Dictionary dictionary = Dictionary::Open("fixture-id", info_path);

    try {
        static_cast<void>(dictionary.GetResource("large.bin"));
        QFAIL("GetResource should reject an oversized resource");
    } catch (const dictionary::Error& error) {
        QCOMPARE(error.code(), dictionary::ErrorCode::kInvalidData);
    }
}

}  // namespace
}  // namespace goldendict::core::formats::stardict

using goldendict::core::formats::stardict::StardictDictionaryTest;

QTEST_APPLESS_MAIN(StardictDictionaryTest)

#include "stardict_dictionary_test.moc"
