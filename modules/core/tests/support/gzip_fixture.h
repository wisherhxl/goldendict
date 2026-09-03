// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_GZIP_FIXTURE_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_GZIP_FIXTURE_H_

#include <filesystem>

#include <zlib.h>

namespace goldendict::core::test {

inline gzFile OpenGzipFixture(const std::filesystem::path& path,
                              const char* mode) {
#ifdef _WIN32
    return gzopen_w(path.c_str(), mode);
#else
    return gzopen(path.c_str(), mode);
#endif
}

}  // namespace goldendict::core::test

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_GZIP_FIXTURE_H_
