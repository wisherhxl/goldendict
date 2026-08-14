// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_ORDERED_HEADWORD_INDEX_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_ORDERED_HEADWORD_INDEX_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace goldendict::core::dictionary {

enum class OrderedHeadwordErrorCode {
    kInvalidRecordCount,
    kInvalidOffset,
    kHeadwordTooLarge
};

class OrderedHeadwordError final : public std::runtime_error {
   public:
    OrderedHeadwordError(OrderedHeadwordErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}

    OrderedHeadwordErrorCode code() const noexcept { return code_; }

   private:
    OrderedHeadwordErrorCode code_;
};

// Lazily publishes a compact, immutable ordering over backend-owned records.
// The backend record collection and the strings returned by HeadwordAt must
// remain immutable for the lifetime of this object.
class OrderedHeadwordIndex final {
   public:
    using Checkpoint = std::function<void()>;

    OrderedHeadwordIndex() : state_(std::make_shared<State>()) {}

    template <typename HeadwordAt>
    std::pair<std::vector<std::string>, bool> Page(
        std::size_t record_count, HeadwordAt headword_at, std::size_t offset,
        std::size_t result_limit, std::size_t byte_limit,
        const Checkpoint& checkpoint = {}) const {
        std::unique_lock<std::mutex> lock(state_->mutex, std::defer_lock);
        while (!lock.try_lock()) {
            if (checkpoint)
                checkpoint();
            std::this_thread::yield();
        }
        if (!state_->ordinals) {
            if (record_count > std::numeric_limits<std::uint32_t>::max()) {
                throw OrderedHeadwordError(
                    OrderedHeadwordErrorCode::kInvalidRecordCount,
                    "Dictionary has too many headwords to enumerate");
            }
            auto ordinals = std::make_shared<std::vector<std::uint32_t>>();
            ordinals->reserve(record_count);
            for (std::size_t index = 0; index < record_count; ++index) {
                if (checkpoint && index % 1024U == 0U)
                    checkpoint();
                ordinals->push_back(static_cast<std::uint32_t>(index));
            }
            std::size_t comparisons = 0U;
            std::stable_sort(ordinals->begin(), ordinals->end(),
                             [&headword_at, &checkpoint, &comparisons](
                                 std::uint32_t left, std::uint32_t right) {
                                 if (checkpoint && comparisons++ % 1024U == 0U)
                                     checkpoint();
                                 return CompareLikeQString(headword_at(left),
                                                           headword_at(right)) <
                                        0;
                             });
            const auto unique_end = std::unique(
                ordinals->begin(), ordinals->end(),
                [&headword_at](std::uint32_t left, std::uint32_t right) {
                    return headword_at(left) == headword_at(right);
                });
            ordinals->erase(unique_end, ordinals->end());
            state_->ordinals = std::move(ordinals);
        }
        const auto ordinals = state_->ordinals;
        lock.unlock();

        if (offset > ordinals->size()) {
            throw OrderedHeadwordError(OrderedHeadwordErrorCode::kInvalidOffset,
                                       "Headword enumeration offset is stale");
        }
        std::vector<std::string> headwords;
        headwords.reserve(std::min(result_limit, ordinals->size() - offset));
        std::size_t bytes = 0U;
        std::size_t position = offset;
        while (position < ordinals->size() && headwords.size() < result_limit) {
            if (checkpoint && headwords.size() % 64U == 0U)
                checkpoint();
            const std::string_view headword =
                headword_at((*ordinals)[position]);
            if (headword.size() > byte_limit) {
                throw OrderedHeadwordError(
                    OrderedHeadwordErrorCode::kHeadwordTooLarge,
                    "Headword exceeds the enumeration response bound");
            }
            if (!headwords.empty() && headword.size() > byte_limit - bytes)
                break;
            bytes += headword.size();
            headwords.emplace_back(headword);
            ++position;
        }
        return {std::move(headwords), position == ordinals->size()};
    }

   private:
    class Utf16Units final {
       public:
        explicit Utf16Units(std::string_view input) : input_(input) {}

        std::optional<std::uint16_t> Next() {
            if (low_surrogate_ != 0U) {
                const auto result = low_surrogate_;
                low_surrogate_ = 0U;
                return result;
            }
            if (offset_ == input_.size())
                return std::nullopt;
            const auto first = static_cast<unsigned char>(input_[offset_++]);
            std::uint32_t point = first;
            std::size_t continuation = 0U;
            if ((first & 0xe0U) == 0xc0U) {
                point = first & 0x1fU;
                continuation = 1U;
            } else if ((first & 0xf0U) == 0xe0U) {
                point = first & 0x0fU;
                continuation = 2U;
            } else if ((first & 0xf8U) == 0xf0U) {
                point = first & 0x07U;
                continuation = 3U;
            }
            for (std::size_t index = 0U; index < continuation; ++index) {
                point = (point << 6U) |
                        (static_cast<unsigned char>(input_[offset_++]) & 0x3fU);
            }
            if (point <= 0xffffU)
                return static_cast<std::uint16_t>(point);
            point -= 0x10000U;
            low_surrogate_ =
                static_cast<std::uint16_t>(0xdc00U + (point & 0x3ffU));
            return static_cast<std::uint16_t>(0xd800U + (point >> 10U));
        }

       private:
        std::string_view input_;
        std::size_t offset_ = 0U;
        std::uint16_t low_surrogate_ = 0U;
    };

    static int CompareLikeQString(std::string_view left,
                                  std::string_view right) {
        Utf16Units left_units(left);
        Utf16Units right_units(right);
        while (true) {
            const auto left_unit = left_units.Next();
            const auto right_unit = right_units.Next();
            if (!left_unit || !right_unit)
                return left_unit ? 1 : right_unit ? -1 : 0;
            if (*left_unit != *right_unit)
                return *left_unit < *right_unit ? -1 : 1;
        }
    }

    struct State {
        std::mutex mutex;
        std::shared_ptr<const std::vector<std::uint32_t>> ordinals;
    };

    std::shared_ptr<State> state_;
};

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_ORDERED_HEADWORD_INDEX_H_
