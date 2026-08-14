// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <QFile>
#include <QTemporaryDir>

#include <string>
#include <utility>

#include "goldendict/external/external_program_source.h"

namespace goldendict::external {
namespace {

class Helper final {
   public:
    Helper() {
        if (!directory_.isValid()) {
            qFatal("Could not create external program fixture directory");
        }
        path_ = directory_.filePath("helper.sh");
        QFile file(path_);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write("#!/bin/sh\n"
                       "case \"$1\" in\n"
                       "  arg) printf '%s' \"$2\" ;;\n"
                       "  stdin) cat ;;\n"
                       "  utf16) printf '\\377\\376H\\000i\\000' ;;\n"
                       "  stderr) printf 'failure' >&2; exit 7 ;;\n"
                       "  slow) sleep 2 ;;\n"
                       "  large) printf '0123456789' ;;\n"
                       "esac\n") < 0) {
            qFatal("Could not write external program fixture");
        }
        file.close();
        QFile::Permissions permissions = file.permissions();
        permissions |= QFileDevice::ExeOwner | QFileDevice::ExeGroup |
                       QFileDevice::ExeOther;
        if (!file.setPermissions(permissions)) {
            qFatal("Could not make external program fixture executable");
        }
    }

    std::string Path() const { return path_.toStdString(); }

   private:
    QTemporaryDir directory_;
    QString path_;
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

    ExternalProgramOptions missing;
    missing.executable = "/definitely/missing/goldendict-external-helper";
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
