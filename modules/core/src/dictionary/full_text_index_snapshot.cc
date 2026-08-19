// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_index_snapshot.h"

#include <atomic>
#include <utility>

namespace goldendict::core::dictionary {

std::shared_ptr<const FullTextIndex> FullTextIndexSnapshotHolder::Acquire()
    const noexcept {
    return std::atomic_load_explicit(&snapshot_, std::memory_order_acquire);
}

bool FullTextIndexSnapshotHolder::Publish(
    std::shared_ptr<const FullTextIndex> snapshot) noexcept {
    if (snapshot == nullptr)
        return false;
    std::atomic_store_explicit(&snapshot_, std::move(snapshot),
                               std::memory_order_release);
    return true;
}

}  // namespace goldendict::core::dictionary
