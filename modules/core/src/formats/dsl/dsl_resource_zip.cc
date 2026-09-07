// SPDX-License-Identifier: GPL-3.0-or-later

#include "dsl_resource_zip.h"

#include <algorithm>
#include <cctype>

namespace goldendict::core::formats::dsl {
namespace {

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

std::vector<std::filesystem::path> ArchiveCandidates(
    const std::filesystem::path& dictionary_path) {
    const std::string filename = dictionary_path.filename().u8string();
    const std::string lowered = Lower(filename);
    const std::size_t suffix =
        lowered.size() >= 7U &&
                lowered.compare(lowered.size() - 7U, 7U, ".dsl.dz") == 0
            ? 7U
            : 4U;
    const auto base =
        dictionary_path.parent_path() /
        std::filesystem::u8path(filename.substr(0U, filename.size() - suffix));
    std::vector<std::filesystem::path> result;
    for (const std::string_view ending :
         {".dsl.files.zip", ".dsl.dz.files.zip", ".DSL.FILES.ZIP",
          ".DSL.DZ.FILES.ZIP"}) {
        result.push_back(
            std::filesystem::u8path(base.u8string() + std::string(ending)));
    }
    return result;
}

}  // namespace

std::optional<ResourceZip> ResourceZip::OpenAdjacent(
    const std::filesystem::path& dictionary_path) {
    for (const auto& candidate : ArchiveCandidates(dictionary_path)) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            return ResourceZip(foundation::ZipArchive::Open(candidate));
        }
    }
    return std::nullopt;
}

}  // namespace goldendict::core::formats::dsl
