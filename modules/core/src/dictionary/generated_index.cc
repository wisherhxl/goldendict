// SPDX-License-Identifier: GPL-3.0-or-later

#include "generated_index.h"

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace goldendict::core::dictionary {
namespace {

constexpr std::array<char, 8> kMagic = {'G', 'D', 'I', 'N',
                                        'D', 'E', 'X', '\0'};
constexpr std::uint32_t kVersion = 1;
constexpr std::uintmax_t kMaximumIndexSize = 256U * 1024U * 1024U;
constexpr std::uint32_t kMaximumSources = 64;
constexpr std::uint32_t kMaximumStringSize = 1024U * 1024U;
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::filesystem::path MakeTemporaryPath(
    const std::filesystem::path& index_path) {
    static std::atomic<std::uint64_t> counter{0};
    const auto timestamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary_path = index_path;
    temporary_path += ".tmp." + std::to_string(timestamp) + "." +
                      std::to_string(counter.fetch_add(1));
    return temporary_path;
}

void AppendUnsigned32(std::uint32_t value, std::string* output) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

void AppendUnsigned64(std::uint64_t value, std::string* output) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

void AppendString(std::string_view value, std::string* output) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw GeneratedIndexError("Generated index string is too large");
    }
    AppendUnsigned32(static_cast<std::uint32_t>(value.size()), output);
    output->append(value);
}

std::uint64_t Hash(std::string_view data) {
    std::uint64_t result = kFnvOffsetBasis;
    for (const unsigned char byte : data) {
        result ^= byte;
        result *= kFnvPrime;
    }
    return result;
}

class Cursor final {
   public:
    explicit Cursor(std::string_view data) : data_(data) {}

    std::uint32_t ReadUnsigned32() {
        Require(4U);
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < 4U; ++index) {
            value = (value << 8U) | Byte(position_++);
        }
        return value;
    }

    std::uint64_t ReadUnsigned64() {
        Require(8U);
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < 8U; ++index) {
            value = (value << 8U) | Byte(position_++);
        }
        return value;
    }

    std::string ReadString() {
        const auto size = ReadUnsigned32();
        if (size > kMaximumStringSize) {
            throw GeneratedIndexError("Generated index string is too large");
        }
        return std::string(ReadBytes(size));
    }

    std::string_view ReadBytes(std::uint64_t size) {
        if (size > std::numeric_limits<std::size_t>::max()) {
            throw GeneratedIndexError("Generated index field is too large");
        }
        const auto converted_size = static_cast<std::size_t>(size);
        Require(converted_size);
        const auto result = data_.substr(position_, converted_size);
        position_ += converted_size;
        return result;
    }

    std::size_t position() const noexcept { return position_; }

    bool AtEnd() const noexcept { return position_ == data_.size(); }

   private:
    void Require(std::size_t size) const {
        if (size > data_.size() - position_) {
            throw GeneratedIndexError("Generated index is truncated");
        }
    }

    std::uint64_t Byte(std::size_t position) const {
        return static_cast<unsigned char>(data_[position]);
    }

    std::string_view data_;
    std::size_t position_ = 0;
};

std::string ReadIndexFile(const std::filesystem::path& path) {
    std::error_code filesystem_error;
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error || size > kMaximumIndexSize ||
        size > std::numeric_limits<std::streamsize>::max()) {
        throw GeneratedIndexError("Cannot read generated index: " +
                                  path.string());
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw GeneratedIndexError("Cannot open generated index: " +
                                  path.string());
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    if (!data.empty() &&
        !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        throw GeneratedIndexError("Cannot read complete generated index: " +
                                  path.string());
    }
    return data;
}

SourceSnapshot ParseSources(Cursor* cursor) {
    const auto count = cursor->ReadUnsigned32();
    if (count > kMaximumSources) {
        throw GeneratedIndexError("Generated index has too many sources");
    }
    SourceSnapshot sources;
    sources.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        SourceStamp source;
        source.path = cursor->ReadString();
        source.size = cursor->ReadUnsigned64();
        source.modified = static_cast<std::int64_t>(cursor->ReadUnsigned64());
        sources.push_back(std::move(source));
    }
    return sources;
}

}  // namespace

SourceSnapshot CaptureSourceSnapshot(
    const std::vector<std::filesystem::path>& source_files) {
    SourceSnapshot sources;
    sources.reserve(source_files.size());
    for (const auto& source_file : source_files) {
        std::error_code filesystem_error;
        SourceStamp source;
        source.path = std::filesystem::absolute(source_file, filesystem_error)
                          .lexically_normal()
                          .generic_string();
        if (filesystem_error) {
            throw GeneratedIndexError("Cannot resolve source file: " +
                                      source_file.string());
        }
        source.size = std::filesystem::file_size(source_file, filesystem_error);
        if (filesystem_error) {
            throw GeneratedIndexError("Cannot inspect source file: " +
                                      source_file.string());
        }
        const auto modified =
            std::filesystem::last_write_time(source_file, filesystem_error);
        if (filesystem_error) {
            throw GeneratedIndexError("Cannot inspect source timestamp: " +
                                      source_file.string());
        }
        source.modified =
            static_cast<std::int64_t>(modified.time_since_epoch().count());
        sources.push_back(std::move(source));
    }
    return sources;
}

GeneratedIndexLoadResult LoadGeneratedIndex(
    const std::filesystem::path& index_path, std::string_view format,
    const SourceSnapshot& expected_sources) {
    std::error_code filesystem_error;
    if (!std::filesystem::exists(index_path, filesystem_error)) {
        if (filesystem_error) {
            return {GeneratedIndexState::kCorrupt, {}};
        }
        return {GeneratedIndexState::kMissing, {}};
    }

    try {
        const std::string data = ReadIndexFile(index_path);
        if (data.size() < kMagic.size() + 4U + 8U) {
            return {GeneratedIndexState::kCorrupt, {}};
        }
        const auto stored_checksum_offset = data.size() - 8U;
        Cursor checksum_cursor(
            std::string_view(data).substr(stored_checksum_offset));
        const auto stored_checksum = checksum_cursor.ReadUnsigned64();
        if (Hash(std::string_view(data).substr(0, stored_checksum_offset)) !=
            stored_checksum) {
            return {GeneratedIndexState::kCorrupt, {}};
        }

        Cursor cursor(std::string_view(data).substr(0, stored_checksum_offset));
        if (cursor.ReadBytes(kMagic.size()) !=
            std::string_view(kMagic.data(), kMagic.size())) {
            return {GeneratedIndexState::kCorrupt, {}};
        }
        if (cursor.ReadUnsigned32() != kVersion) {
            return {GeneratedIndexState::kCorrupt, {}};
        }
        if (cursor.ReadString() != format) {
            return {GeneratedIndexState::kStale, {}};
        }
        if (ParseSources(&cursor) != expected_sources) {
            return {GeneratedIndexState::kStale, {}};
        }
        const auto payload_size = cursor.ReadUnsigned64();
        std::string payload(cursor.ReadBytes(payload_size));
        if (!cursor.AtEnd()) {
            return {GeneratedIndexState::kCorrupt, {}};
        }
        return {GeneratedIndexState::kCurrent, std::move(payload)};
    } catch (const GeneratedIndexError&) {
        return {GeneratedIndexState::kCorrupt, {}};
    }
}

void StoreGeneratedIndex(const std::filesystem::path& index_path,
                         std::string_view format, const SourceSnapshot& sources,
                         std::string_view payload) {
    if (sources.size() > kMaximumSources) {
        throw GeneratedIndexError("Too many generated index sources");
    }
    std::string data(kMagic.data(), kMagic.size());
    AppendUnsigned32(kVersion, &data);
    AppendString(format, &data);
    AppendUnsigned32(static_cast<std::uint32_t>(sources.size()), &data);
    for (const auto& source : sources) {
        AppendString(source.path, &data);
        AppendUnsigned64(source.size, &data);
        AppendUnsigned64(static_cast<std::uint64_t>(source.modified), &data);
    }
    AppendUnsigned64(payload.size(), &data);
    data.append(payload);
    AppendUnsigned64(Hash(data), &data);
    if (data.size() > kMaximumIndexSize) {
        throw GeneratedIndexError("Generated index exceeds the size limit");
    }

    std::error_code filesystem_error;
    if (!index_path.parent_path().empty()) {
        std::filesystem::create_directories(index_path.parent_path(),
                                            filesystem_error);
        if (filesystem_error) {
            throw GeneratedIndexError("Cannot create index directory: " +
                                      index_path.parent_path().string());
        }
    }
    if (std::filesystem::exists(index_path, filesystem_error) &&
        !std::filesystem::is_regular_file(index_path, filesystem_error)) {
        throw GeneratedIndexError(
            "Generated index target is not a regular file: " +
            index_path.string());
    }
    if (filesystem_error) {
        throw GeneratedIndexError("Cannot inspect generated index target: " +
                                  index_path.string());
    }

    const auto temporary_path = MakeTemporaryPath(index_path);
    std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
    if (!output ||
        !output.write(data.data(), static_cast<std::streamsize>(data.size())) ||
        !output.flush()) {
        output.close();
        std::filesystem::remove(temporary_path, filesystem_error);
        throw GeneratedIndexError("Cannot write generated index: " +
                                  temporary_path.string());
    }
    output.close();

    std::filesystem::rename(temporary_path, index_path, filesystem_error);
    if (filesystem_error) {
        std::error_code remove_error;
        if (std::filesystem::is_regular_file(index_path, remove_error)) {
            std::filesystem::remove(index_path, remove_error);
        }
        if (remove_error) {
            std::filesystem::remove(temporary_path, filesystem_error);
            throw GeneratedIndexError("Cannot replace generated index: " +
                                      index_path.string());
        }
        filesystem_error.clear();
        std::filesystem::rename(temporary_path, index_path, filesystem_error);
    }
    if (filesystem_error) {
        std::filesystem::remove(temporary_path, filesystem_error);
        throw GeneratedIndexError("Cannot commit generated index: " +
                                  index_path.string());
    }
}

}  // namespace goldendict::core::dictionary
