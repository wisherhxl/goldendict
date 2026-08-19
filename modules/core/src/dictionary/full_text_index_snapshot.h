// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_SNAPSHOT_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_SNAPSHOT_H_

#include <memory>

#include "full_text_index.h"

namespace goldendict::core::dictionary {

class FullTextIndexSnapshotHolder final {
   public:
    std::shared_ptr<const FullTextIndex> Acquire() const noexcept;
    bool Publish(std::shared_ptr<const FullTextIndex> snapshot) noexcept;

   private:
    std::shared_ptr<const FullTextIndex> snapshot_;
};

}  // namespace goldendict::core::dictionary

#endif
