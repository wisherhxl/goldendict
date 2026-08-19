// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_index_lifecycle.h"

#include <exception>
#include <new>
#include <stdexcept>

namespace goldendict::core::dictionary {

FullTextIndexWorkResult FullTextIndexFormatWorkPort::PerformFullTextIndexWork(
    const FullTextIndexWorkRequest& request) noexcept {
    try {
        return DoPerformFullTextIndexWork(request);
    } catch (const std::bad_alloc&) {
        return {FullTextIndexWorkStatus::kFailed, "Resource limit exceeded"};
    } catch (const std::length_error&) {
        return {FullTextIndexWorkStatus::kFailed, "Resource limit exceeded"};
    } catch (const std::exception& error) {
        return {FullTextIndexWorkStatus::kFailed, error.what()};
    } catch (...) {
        return {FullTextIndexWorkStatus::kFailed,
                "Unknown full-text index work failure"};
    }
}

}  // namespace goldendict::core::dictionary
