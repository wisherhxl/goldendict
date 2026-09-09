// SPDX-License-Identifier: GPL-3.0-or-later

#include "configuration_file.h"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <memory>
#include <system_error>

#ifdef _WIN32
#include <share.h>
#endif

namespace goldendict::core {
namespace {

std::error_code StreamError() {
    return errno != 0 ? std::error_code(errno, std::generic_category())
                      : std::make_error_code(std::errc::io_error);
}

void WriteTemporary(
    const std::filesystem::path& temporary, std::string_view contents,
    const std::function<void(const std::filesystem::path&)>& temporary_written,
    bool& owns_temporary) {
    errno = 0;
#ifdef _WIN32
    // N sets noninheritability during creation, before another thread can
    // launch a child. Keep the former ofstream's read/write sharing mode.
    std::unique_ptr<std::FILE, decltype(&std::fclose)> output(
        _wfsopen(temporary.c_str(), L"wbN", _SH_DENYNO), &std::fclose);
#else
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
#endif
    if (!output) {
        throw std::filesystem::filesystem_error(
            "Cannot open configuration temporary", temporary, StreamError());
    }
    owns_temporary = true;
    errno = 0;
#ifdef _WIN32
    if (std::fwrite(contents.data(), 1U, contents.size(), output.get()) !=
        contents.size()) {
#else
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    if (!output) {
#endif
        throw std::filesystem::filesystem_error(
            "Cannot write configuration temporary", temporary, StreamError());
    }
    if (temporary_written)
        temporary_written(temporary);
    errno = 0;
#ifdef _WIN32
    if (std::fclose(output.release()) != 0) {
#else
    output.close();
    if (!output) {
#endif
        throw std::filesystem::filesystem_error(
            "Cannot close configuration temporary", temporary, StreamError());
    }
}

}  // namespace

void PublishConfigurationFile(
    const std::filesystem::path& destination, std::string_view contents,
    const std::function<void(const std::filesystem::path&)>&
        temporary_written) {
    if (!destination.parent_path().empty())
        std::filesystem::create_directories(destination.parent_path());
    const std::filesystem::path temporary(destination.string() + ".tmp");
    bool owns_temporary = false;
    try {
        WriteTemporary(temporary, contents, temporary_written, owns_temporary);
        std::error_code error;
        std::filesystem::rename(temporary, destination, error);
        if (error) {
            throw std::filesystem::filesystem_error(
                "Cannot replace configuration file", temporary, destination,
                error);
        }
    } catch (...) {
        if (owns_temporary) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        throw;
    }
}

}  // namespace goldendict::core
