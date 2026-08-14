// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_HEADWORD_EXPORT_H_
#define GOLDENDICT_CORE_HEADWORD_EXPORT_H_

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

#include "goldendict/base/goldendict_def.tp.h"

namespace goldendict::core {

struct HeadwordExportRequest {
    std::string dictionary_id;
    std::string destination_path;
    std::size_t page_size = 256U;
    std::chrono::milliseconds timeout = std::chrono::minutes(5);
};

enum class HeadwordExportErrorCode {
    kNone,
    kInvalidRequest,
    kDictionaryUnavailable,
    kUnsupported,
    kMalformedCursor,
    kStaleCursor,
    kCancelled,
    kDeadlineExceeded,
    kOpenFailed,
    kWriteFailed,
    kFlushFailed,
    kReplaceFailed,
    kInternal,
};

struct HeadwordExportResult {
    HeadwordExportErrorCode error = HeadwordExportErrorCode::kNone;
    std::size_t exported_headwords = 0U;
    std::string message;

    explicit operator bool() const noexcept {
        return error == HeadwordExportErrorCode::kNone;
    }
};

class GOLDENDICT_EXPORTS HeadwordExportOperation {
   public:
    virtual ~HeadwordExportOperation();
    virtual void Cancel() noexcept = 0;
    virtual bool IsFinished() const noexcept = 0;
    virtual std::size_t ExportedHeadwords() const noexcept = 0;
    virtual HeadwordExportResult Await() = 0;
};

class DictionaryService;

GOLDENDICT_EXPORTS std::unique_ptr<HeadwordExportOperation> StartHeadwordExport(
    const DictionaryService& service, HeadwordExportRequest request);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_HEADWORD_EXPORT_H_
