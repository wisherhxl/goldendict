// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <string>
#include <utility>

#include "goldendict/external/external_program_source.h"

namespace goldendict::external {
namespace {

class Helper final {
   public:
    std::string Path() const { return GOLDENDICT_EXTERNAL_PROGRAM_TEST_HELPER; }
};

template <typename Callback>
void VerifyError(ExternalProgramErrorCode expected, Callback&& callback) {
    try {
        callback();
        QFAIL("Expected external program error");
    } catch (const ExternalProgramError& error) {
        QCOMPARE(error.code(), expected);
    }
}

ExternalProgramOptions Options(const Helper& helper,
                               std::vector<std::string> arguments) {
    ExternalProgramOptions options;
    options.executable = helper.Path();
    options.arguments = std::move(arguments);
    options.timeout = std::chrono::seconds(1);
    return options;
}

}  // namespace

class ExternalProgramSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void ReplacesArgumentsAndWritesStandardInput();
    void DecodesUnicodeAndReportsFailures();
    void EnforcesCancellationDeadlineAndOutputBounds();
    void RejectsInvalidConfigurationAndRequests();
};

void ExternalProgramSourceTest::ReplacesArgumentsAndWritesStandardInput() {
    Helper helper;
    const ExternalProgramSource argument_source(
        Options(helper, {"arg", "prefix:%GDWORD%:suffix"}));
    QCOMPARE(argument_source.Run("A&B").standard_output,
             std::string("prefix:A&B:suffix"));

    const ExternalProgramSource stdin_source(Options(helper, {"stdin"}));
    QCOMPARE(stdin_source.Run("héllo").standard_output, std::string("héllo"));
}

void ExternalProgramSourceTest::DecodesUnicodeAndReportsFailures() {
    Helper helper;
    const ExternalProgramSource unicode_source(Options(helper, {"utf16"}));
    QCOMPARE(unicode_source.Run("word").standard_output, std::string("Hi"));

    const ExternalProgramSource failure_source(Options(helper, {"stderr"}));
    VerifyError(ExternalProgramErrorCode::kNonZeroExit,
                [&]() { static_cast<void>(failure_source.Run("word")); });
}

void ExternalProgramSourceTest::EnforcesCancellationDeadlineAndOutputBounds() {
    Helper helper;
    const ExternalProgramSource source(Options(helper, {"arg", "%GDWORD%"}));
    VerifyError(ExternalProgramErrorCode::kCancelled, [&]() {
        static_cast<void>(source.Run("word", []() { return true; }));
    });

    ExternalProgramOptions deadline_options = Options(helper, {"slow"});
    deadline_options.timeout = std::chrono::milliseconds(100);
    const ExternalProgramSource deadline_source(std::move(deadline_options));
    VerifyError(ExternalProgramErrorCode::kDeadlineExceeded,
                [&]() { static_cast<void>(deadline_source.Run("word")); });

    ExternalProgramOptions bounded_options = Options(helper, {"large"});
    bounded_options.maximum_output_bytes = 5U;
    const ExternalProgramSource bounded_source(std::move(bounded_options));
    VerifyError(ExternalProgramErrorCode::kOutputTooLarge,
                [&]() { static_cast<void>(bounded_source.Run("word")); });
}

void ExternalProgramSourceTest::RejectsInvalidConfigurationAndRequests() {
    ExternalProgramOptions relative;
    relative.executable = "helper";
    VerifyError(ExternalProgramErrorCode::kInvalidConfiguration,
                [&]() { static_cast<void>(ExternalProgramSource(relative)); });

    QTemporaryDir missing_program_directory;
    QVERIFY(missing_program_directory.isValid());
    ExternalProgramOptions missing;
    missing.executable =
        QDir(missing_program_directory.path())
            .absoluteFilePath("goldendict-definitely-missing-external-helper")
            .toStdString();
    const ExternalProgramSource missing_source(std::move(missing));
    VerifyError(ExternalProgramErrorCode::kFailedToStart,
                [&]() { static_cast<void>(missing_source.Run("word")); });

    Helper helper;
    const ExternalProgramSource source(Options(helper, {"stdin"}));
    VerifyError(ExternalProgramErrorCode::kInvalidRequest,
                [&]() { static_cast<void>(source.Run("")); });
    VerifyError(ExternalProgramErrorCode::kInvalidRequest, [&]() {
        const std::string invalid_utf8(1U, static_cast<char>(0xFF));
        static_cast<void>(source.Run(invalid_utf8));
    });
}

}  // namespace goldendict::external

using goldendict::external::ExternalProgramSourceTest;

QTEST_GUILESS_MAIN(ExternalProgramSourceTest)

#include "external_program_source_test.moc"
