// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_APPLICATION_CONFIGURATION_FILE_H_
#define GOLDENDICT_CORE_SRC_APPLICATION_CONFIGURATION_FILE_H_

#include <filesystem>
#include <functional>
#include <string_view>

namespace goldendict::core {

// The optional checkpoint observes the written temporary before its writer
// closes. It is private test access for file ownership and failure
// interleavings.
void PublishConfigurationFile(
    const std::filesystem::path& destination, std::string_view contents,
    const std::function<void(const std::filesystem::path&)>& temporary_written =
        {});

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_SRC_APPLICATION_CONFIGURATION_FILE_H_
