// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/external/external_program_source.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QStringConverter>
#include <QStringDecoder>

#include <utility>

namespace goldendict::external {
namespace {

constexpr std::size_t kMaximumWordBytes = 4096U;
constexpr std::size_t kMaximumArgumentBytes = 16U * 1024U;
constexpr std::size_t kMaximumArguments = 256U;
constexpr int kWaitSliceMilliseconds = 50;

QString DecodeInput(std::string_view value, const char* name,
                    ExternalProgramErrorCode code) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded =
        decoder(QByteArray(value.data(), static_cast<qsizetype>(value.size())));
    if (decoder.hasError()) {
        throw ExternalProgramError(code, std::string("External program ") +
                                             name + " must be valid UTF-8");
    }
    return decoded;
}

std::string DecodeOutput(QByteArray bytes, const char* name) {
    QStringConverter::Encoding encoding = QStringConverter::Utf8;
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        bytes.remove(0, 3);
    } else if (bytes.startsWith("\xFF\xFE")) {
        bytes.remove(0, 2);
        if ((bytes.size() % 2) != 0) {
            throw ExternalProgramError(ExternalProgramErrorCode::kInvalidOutput,
                                       std::string("External program ") + name +
                                           " contains truncated UTF-16LE");
        }
        encoding = QStringConverter::Utf16LE;
    } else if (bytes.startsWith("\xFE\xFF")) {
        bytes.remove(0, 2);
        if ((bytes.size() % 2) != 0) {
            throw ExternalProgramError(ExternalProgramErrorCode::kInvalidOutput,
                                       std::string("External program ") + name +
                                           " contains truncated UTF-16BE");
        }
        encoding = QStringConverter::Utf16BE;
    }
    QStringDecoder decoder(encoding);
    const QString decoded = decoder(bytes);
    if (decoder.hasError()) {
        throw ExternalProgramError(ExternalProgramErrorCode::kInvalidOutput,
                                   std::string("External program ") + name +
                                       " contains invalid Unicode");
    }
    return decoded.toStdString();
}

void StopProcess(QProcess& process) {
    if (process.state() == QProcess::NotRunning) {
        return;
    }
    process.terminate();
    if (!process.waitForFinished(100)) {
        process.kill();
        process.waitForFinished(1000);
    }
}

}  // namespace

ExternalProgramError::ExternalProgramError(ExternalProgramErrorCode code,
                                           std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

ExternalProgramSource::ExternalProgramSource(ExternalProgramOptions options)
    : options_(std::move(options)) {
    const QFileInfo executable(QString::fromStdString(options_.executable));
    if (options_.executable.empty() || !executable.isAbsolute() ||
        options_.executable.find('\0') != std::string::npos ||
        options_.arguments.size() > kMaximumArguments ||
        options_.timeout.count() <= 0 || options_.maximum_output_bytes == 0U ||
        options_.maximum_output_bytes > 64U * 1024U * 1024U) {
        throw ExternalProgramError(
            ExternalProgramErrorCode::kInvalidConfiguration,
            "External program configuration is invalid");
    }
    static_cast<void>(
        DecodeInput(options_.executable, "executable",
                    ExternalProgramErrorCode::kInvalidConfiguration));
    if (!options_.working_directory.empty()) {
        const QFileInfo directory(
            QString::fromStdString(options_.working_directory));
        if (!directory.isAbsolute() ||
            options_.working_directory.find('\0') != std::string::npos) {
            throw ExternalProgramError(
                ExternalProgramErrorCode::kInvalidConfiguration,
                "External program working directory is invalid");
        }
        static_cast<void>(
            DecodeInput(options_.working_directory, "working directory",
                        ExternalProgramErrorCode::kInvalidConfiguration));
    }
    for (const std::string& argument : options_.arguments) {
        if (argument.size() > kMaximumArgumentBytes ||
            argument.find('\0') != std::string::npos) {
            throw ExternalProgramError(
                ExternalProgramErrorCode::kInvalidConfiguration,
                "External program argument template is invalid");
        }
        static_cast<void>(
            DecodeInput(argument, "argument template",
                        ExternalProgramErrorCode::kInvalidConfiguration));
    }
}

ExternalProgramResult ExternalProgramSource::Run(
    std::string_view word,
    const std::function<bool()>& is_cancellation_requested) const {
    if (word.empty() || word.size() > kMaximumWordBytes ||
        word.find('\0') != std::string_view::npos) {
        throw ExternalProgramError(ExternalProgramErrorCode::kInvalidRequest,
                                   "External program word is invalid");
    }
    const QString decoded_word =
        DecodeInput(word, "word", ExternalProgramErrorCode::kInvalidRequest);
    if (is_cancellation_requested && is_cancellation_requested()) {
        throw ExternalProgramError(ExternalProgramErrorCode::kCancelled,
                                   "External program request was cancelled");
    }

    QStringList arguments;
    bool uses_placeholder = false;
    for (const std::string& argument : options_.arguments) {
        QString expanded = QString::fromStdString(argument);
        if (expanded.contains("%GDWORD%")) {
            uses_placeholder = true;
            expanded.replace("%GDWORD%", decoded_word);
        }
        arguments.push_back(std::move(expanded));
    }

    QProcess process;
    process.setProgram(QString::fromStdString(options_.executable));
    process.setArguments(arguments);
    if (!options_.working_directory.empty()) {
        process.setWorkingDirectory(
            QString::fromStdString(options_.working_directory));
    }
    process.setProcessChannelMode(QProcess::SeparateChannels);

    QElapsedTimer timer;
    timer.start();
    process.start(QIODevice::ReadWrite);
    while (!process.waitForStarted(kWaitSliceMilliseconds)) {
        if (is_cancellation_requested && is_cancellation_requested()) {
            StopProcess(process);
            throw ExternalProgramError(
                ExternalProgramErrorCode::kCancelled,
                "External program request was cancelled");
        }
        if (process.error() == QProcess::FailedToStart) {
            throw ExternalProgramError(ExternalProgramErrorCode::kFailedToStart,
                                       "External program failed to start");
        }
        if (timer.elapsed() >= options_.timeout.count()) {
            StopProcess(process);
            throw ExternalProgramError(
                ExternalProgramErrorCode::kDeadlineExceeded,
                "External program start deadline exceeded");
        }
    }

    if (!uses_placeholder) {
        process.write(decoded_word.toUtf8());
    }
    process.closeWriteChannel();

    QByteArray standard_output;
    QByteArray standard_error;
    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(kWaitSliceMilliseconds);
        standard_output += process.readAllStandardOutput();
        standard_error += process.readAllStandardError();
        if (static_cast<std::size_t>(standard_output.size()) +
                static_cast<std::size_t>(standard_error.size()) >
            options_.maximum_output_bytes) {
            StopProcess(process);
            throw ExternalProgramError(
                ExternalProgramErrorCode::kOutputTooLarge,
                "External program output is too large");
        }
        if (is_cancellation_requested && is_cancellation_requested()) {
            StopProcess(process);
            throw ExternalProgramError(
                ExternalProgramErrorCode::kCancelled,
                "External program request was cancelled");
        }
        if (timer.elapsed() >= options_.timeout.count()) {
            StopProcess(process);
            throw ExternalProgramError(
                ExternalProgramErrorCode::kDeadlineExceeded,
                "External program deadline exceeded");
        }
    }
    standard_output += process.readAllStandardOutput();
    standard_error += process.readAllStandardError();
    if (static_cast<std::size_t>(standard_output.size()) +
            static_cast<std::size_t>(standard_error.size()) >
        options_.maximum_output_bytes) {
        throw ExternalProgramError(ExternalProgramErrorCode::kOutputTooLarge,
                                   "External program output is too large");
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        throw ExternalProgramError(ExternalProgramErrorCode::kCrashed,
                                   "External program crashed");
    }
    if (process.exitCode() != 0) {
        throw ExternalProgramError(ExternalProgramErrorCode::kNonZeroExit,
                                   "External program returned exit code " +
                                       std::to_string(process.exitCode()));
    }
    return {DecodeOutput(std::move(standard_output), "standard output"),
            DecodeOutput(std::move(standard_error), "standard error")};
}

}  // namespace goldendict::external
