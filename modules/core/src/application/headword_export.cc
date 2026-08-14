// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/headword_export.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <future>
#include <limits>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "goldendict/core/dictionary_service.h"

namespace goldendict::core {
namespace {

class AtomicCancellation final : public CancellationToken {
   public:
    bool IsCancellationRequested() const noexcept override {
        return cancelled_.load();
    }

    void Cancel() noexcept { cancelled_.store(true); }

   private:
    std::atomic<bool> cancelled_{false};
};

HeadwordExportErrorCode Translate(HeadwordEnumerationErrorCode code) {
    switch (code) {
        case HeadwordEnumerationErrorCode::kInvalidRequest:
            return HeadwordExportErrorCode::kInvalidRequest;
        case HeadwordEnumerationErrorCode::kDictionaryUnavailable:
            return HeadwordExportErrorCode::kDictionaryUnavailable;
        case HeadwordEnumerationErrorCode::kUnsupported:
            return HeadwordExportErrorCode::kUnsupported;
        case HeadwordEnumerationErrorCode::kMalformedCursor:
            return HeadwordExportErrorCode::kMalformedCursor;
        case HeadwordEnumerationErrorCode::kStaleCursor:
            return HeadwordExportErrorCode::kStaleCursor;
        case HeadwordEnumerationErrorCode::kCancelled:
            return HeadwordExportErrorCode::kCancelled;
        case HeadwordEnumerationErrorCode::kDeadlineExceeded:
            return HeadwordExportErrorCode::kDeadlineExceeded;
        case HeadwordEnumerationErrorCode::kInternal:
            return HeadwordExportErrorCode::kInternal;
    }
    return HeadwordExportErrorCode::kInternal;
}

std::filesystem::path TemporaryPath(const std::filesystem::path& destination) {
    static std::atomic<unsigned long long> sequence{0U};
    const auto parent = destination.parent_path();
    const auto name = destination.filename().string();
    for (unsigned attempt = 0U; attempt < 1000U; ++attempt) {
        const auto suffix = sequence.fetch_add(1U);
        auto candidate = parent / ("." + name + ".goldendict-export-" +
                                   std::to_string(suffix) + ".tmp");
        if (!std::filesystem::exists(candidate))
            return candidate;
    }
    throw std::runtime_error("Could not allocate a temporary export file");
}

bool Replace(const std::filesystem::path& source,
             const std::filesystem::path& destination, std::string* error) {
#ifdef _WIN32
    if (MoveFileExW(source.c_str(), destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    *error = "Could not replace the destination file";
    return false;
#else
    if (::rename(source.c_str(), destination.c_str()) == 0)
        return true;
    *error = std::strerror(errno);
    return false;
#endif
}

class ExportOperation final : public HeadwordExportOperation {
   public:
    ExportOperation(const DictionaryService& service,
                    HeadwordExportRequest request)
        : future_(std::async(std::launch::async,
                             [this, &service, request = std::move(request)] {
                                 try {
                                     return Run(service, request);
                                 } catch (const std::exception& error) {
                                     finished_.store(true);
                                     return HeadwordExportResult{
                                         HeadwordExportErrorCode::kInternal,
                                         exported_.load(), error.what()};
                                 }
                             })) {}

    ~ExportOperation() override {
        Cancel();
        if (future_.valid())
            future_.wait();
    }

    void Cancel() noexcept override { cancellation_.Cancel(); }

    bool IsFinished() const noexcept override { return finished_.load(); }

    std::size_t ExportedHeadwords() const noexcept override {
        return exported_.load();
    }

    HeadwordExportResult Await() override { return future_.get(); }

   private:
    HeadwordExportResult Run(const DictionaryService& service,
                             const HeadwordExportRequest& request) {
        struct Finish {
            std::atomic<bool>* flag;

            ~Finish() { flag->store(true); }
        } finish{&finished_};

        if (request.dictionary_id.empty() || request.destination_path.empty() ||
            request.page_size == 0U ||
            request.page_size > kMaximumHeadwordEnumerationPageSize ||
            request.timeout <= std::chrono::milliseconds::zero()) {
            return {HeadwordExportErrorCode::kInvalidRequest, 0U,
                    "Invalid headword export request"};
        }
        const auto destination =
            std::filesystem::u8path(request.destination_path);
        std::filesystem::path temporary;
        try {
            temporary = TemporaryPath(destination);
        } catch (const std::exception& error) {
            return {HeadwordExportErrorCode::kOpenFailed, 0U, error.what()};
        }

        struct Cleanup {
            std::filesystem::path path;

            ~Cleanup() {
                std::error_code ignored;
                if (!path.empty())
                    std::filesystem::remove(path, ignored);
            }
        } cleanup{temporary};

        std::FILE* file = std::fopen(temporary.string().c_str(), "wbx");
        if (file == nullptr) {
            return {HeadwordExportErrorCode::kOpenFailed, 0U,
                    std::strerror(errno)};
        }
        const auto fail = [&](HeadwordExportErrorCode code,
                              const std::string& message) {
            std::fclose(file);
            file = nullptr;
            return HeadwordExportResult{code, exported_.load(), message};
        };
        const unsigned char bom[] = {0xefU, 0xbbU, 0xbfU};
        if (std::fwrite(bom, 1U, sizeof(bom), file) != sizeof(bom)) {
            return fail(HeadwordExportErrorCode::kWriteFailed,
                        "Could not write the export file");
        }
        const auto deadline =
            std::chrono::steady_clock::now() + request.timeout;
        HeadwordEnumerationQuery query;
        query.dictionary_id = request.dictionary_id;
        query.page_size = request.page_size;
        while (true) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return fail(HeadwordExportErrorCode::kDeadlineExceeded,
                            "Headword export deadline exceeded");
            }
            query.timeout =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                      now);
            const auto page = service.EnumerateHeadwords(query, &cancellation_);
            if (page.error.has_value()) {
                return fail(Translate(page.error->code), page.error->message);
            }
            for (const auto& headword : page.headwords) {
                if (cancellation_.IsCancellationRequested()) {
                    return fail(HeadwordExportErrorCode::kCancelled,
                                "Headword export cancelled");
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    return fail(HeadwordExportErrorCode::kDeadlineExceeded,
                                "Headword export deadline exceeded");
                }
                std::string line = headword;
                for (char& character : line) {
                    if (character == '\r' || character == '\n')
                        character = ' ';
                }
                line.push_back('\n');
                if (std::fwrite(line.data(), 1U, line.size(), file) !=
                    line.size()) {
                    return fail(HeadwordExportErrorCode::kWriteFailed,
                                "Could not write the export file");
                }
                exported_.fetch_add(1U);
            }
            if (page.complete)
                break;
            if (page.next_cursor.empty() || page.headwords.empty()) {
                return fail(HeadwordExportErrorCode::kInternal,
                            "Headword enumeration did not advance");
            }
            query.cursor = page.next_cursor;
        }
        if (std::fflush(file) != 0) {
            return fail(HeadwordExportErrorCode::kFlushFailed,
                        "Could not flush the export file");
        }
#ifdef _WIN32
        if (_commit(_fileno(file)) != 0) {
#else
        if (::fsync(fileno(file)) != 0) {
#endif
            return fail(HeadwordExportErrorCode::kFlushFailed,
                        "Could not flush the export file");
        }
        if (std::fclose(file) != 0) {
            file = nullptr;
            return {HeadwordExportErrorCode::kFlushFailed, exported_.load(),
                    "Could not close the export file"};
        }
        file = nullptr;
        std::string replace_error;
        if (!Replace(temporary, destination, &replace_error)) {
            return {HeadwordExportErrorCode::kReplaceFailed, exported_.load(),
                    std::move(replace_error)};
        }
        cleanup.path.clear();
        return {HeadwordExportErrorCode::kNone, exported_.load(), {}};
    }

    AtomicCancellation cancellation_;
    std::atomic<bool> finished_{false};
    std::atomic<std::size_t> exported_{0U};
    std::future<HeadwordExportResult> future_;
};

}  // namespace

HeadwordExportOperation::~HeadwordExportOperation() = default;

std::unique_ptr<HeadwordExportOperation> StartHeadwordExport(
    const DictionaryService& service, HeadwordExportRequest request) {
    return std::make_unique<ExportOperation>(service, std::move(request));
}

}  // namespace goldendict::core
