// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgets_facade_binding.h"

#include <algorithm>
#include <thread>

namespace goldendict::widgets {

void WidgetsFacadeBindingRegistry::Lease::Reset() noexcept {
    if (slot_ != nullptr) {
        const auto previous =
            slot_->lease_word.fetch_sub(1U, std::memory_order_release);
        if ((previous & kReaderMask) == 0U)
            std::terminate();
    }
    slot_ = nullptr;
    descriptor_ = nullptr;
}

WidgetsFacadeBindingRegistry::WidgetsFacadeBindingRegistry() noexcept = default;

WidgetsFacadeBindingRegistry::~WidgetsFacadeBindingRegistry() {
    Shutdown();
}

std::optional<std::uint8_t> WidgetsFacadeBindingRegistry::Prepare(
    WidgetsFacadeBindingDescriptor descriptor) {
    if (shutdown_.load(std::memory_order_acquire) || NeedsReclaim())
        return std::nullopt;
    const bool already_prepared =
        std::any_of(slots_.begin(), slots_.end(), [](const auto& slot) {
            const auto state = slot.state.load(std::memory_order_acquire);
            return state == SlotState::kPreparing || state == SlotState::kReady;
        });
    if (already_prepared)
        return std::nullopt;
    for (std::uint8_t index = 0U; index < slots_.size(); ++index) {
        auto& slot = slots_[index];
        auto expected = SlotState::kEmpty;
        if (!slot.state.compare_exchange_strong(expected, SlotState::kPreparing,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
            continue;
        }
        if (slot.lease_word.load(std::memory_order_acquire) != kClosed ||
            slot.constructed) {
            slot.state.store(SlotState::kEmpty, std::memory_order_release);
            std::terminate();
        }
        if (NeedsReclaim()) {
            slot.state.store(SlotState::kEmpty, std::memory_order_release);
            return std::nullopt;
        }
        const auto generation =
            next_generation_.fetch_add(1U, std::memory_order_relaxed);
        if (generation == 0U || generation >= (std::uint64_t{1} << 62U))
            std::terminate();
        descriptor.generation = generation;
        new (slot.storage)
            WidgetsFacadeBindingDescriptor(std::move(descriptor));
        slot.constructed = true;
        slot.generation.store(generation, std::memory_order_release);
        slot.state.store(SlotState::kReady, std::memory_order_release);
        slot.lease_word.store(0U, std::memory_order_release);
        if (!slot.Descriptor()->Complete()) {
            Abandon(index);
            return std::nullopt;
        }
        return index;
    }
    return std::nullopt;
}

bool WidgetsFacadeBindingRegistry::Publish(
    std::uint8_t prepared_slot) noexcept {
    if (prepared_slot >= slots_.size() ||
        shutdown_.load(std::memory_order_acquire))
        return false;
    auto& prepared = slots_[prepared_slot];
    auto expected = SlotState::kReady;
    if (!prepared.state.compare_exchange_strong(expected, SlotState::kActive,
                                                std::memory_order_release,
                                                std::memory_order_acquire))
        return false;
    const auto generation = prepared.generation.load(std::memory_order_acquire);
    const auto old = published_.exchange(Pack(prepared_slot, generation),
                                         std::memory_order_acq_rel);
    const auto old_index = SlotIndex(old);
    if (old_index < slots_.size() && old_index != prepared_slot) {
        auto& retired = slots_[old_index];
        retired.state.store(SlotState::kRetired, std::memory_order_release);
        Close(retired);
    }
    return true;
}

void WidgetsFacadeBindingRegistry::ClearPublished() noexcept {
    const auto old =
        published_.exchange(kInvalidPublication, std::memory_order_acq_rel);
    const auto old_index = SlotIndex(old);
    if (old_index < slots_.size()) {
        auto& retired = slots_[old_index];
        retired.state.store(SlotState::kRetired, std::memory_order_release);
        Close(retired);
    }
}

void WidgetsFacadeBindingRegistry::Close(Slot& slot) noexcept {
    slot.lease_word.fetch_or(kClosed, std::memory_order_acq_rel);
}

void WidgetsFacadeBindingRegistry::DestroyClosed(
    Slot& slot, SlotState final_state) noexcept {
    Close(slot);
    while ((slot.lease_word.load(std::memory_order_acquire) & kReaderMask) !=
           0U)
        std::this_thread::yield();
    if (slot.constructed) {
        slot.Descriptor()->~WidgetsFacadeBindingDescriptor();
        slot.constructed = false;
    }
    slot.state.store(final_state, std::memory_order_release);
}

void WidgetsFacadeBindingRegistry::Abandon(
    std::uint8_t prepared_slot) noexcept {
    if (prepared_slot >= slots_.size())
        return;
    auto& slot = slots_[prepared_slot];
    const auto state = slot.state.load(std::memory_order_acquire);
    if (state != SlotState::kReady && state != SlotState::kPreparing)
        return;
    slot.state.store(SlotState::kReclaiming, std::memory_order_release);
    DestroyClosed(slot, SlotState::kEmpty);
}

WidgetsFacadeBindingRegistry::Lease WidgetsFacadeBindingRegistry::Acquire()
    const noexcept {
    for (;;) {
        if (shutdown_.load(std::memory_order_acquire))
            return {};
        const auto first = published_.load(std::memory_order_acquire);
        const auto index = SlotIndex(first);
        if (index >= slots_.size())
            return {};
        auto& slot = slots_[index];
        auto word = slot.lease_word.load(std::memory_order_acquire);
        for (;;) {
            if ((word & kClosed) != 0U)
                break;
            if (ReaderCountWouldOverflow(word))
                std::terminate();
            if (slot.lease_word.compare_exchange_weak(
                    word, word + 1U, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                const auto state = slot.state.load(std::memory_order_acquire);
                const auto generation =
                    slot.generation.load(std::memory_order_acquire);
                const auto second = published_.load(std::memory_order_acquire);
                if (!shutdown_.load(std::memory_order_acquire) &&
                    first == second && state == SlotState::kActive &&
                    generation == PublishedGeneration(first)) {
                    return Lease(&slot, slot.Descriptor());
                }
                slot.lease_word.fetch_sub(1U, std::memory_order_release);
                break;
            }
        }
        if (shutdown_.load(std::memory_order_acquire))
            return {};
    }
}

void WidgetsFacadeBindingRegistry::ReclaimRetired() noexcept {
    for (auto& slot : slots_) {
        auto expected = SlotState::kRetired;
        slot.state.compare_exchange_strong(expected, SlotState::kReclaiming,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire);
        if (slot.state.load(std::memory_order_acquire) !=
                SlotState::kReclaiming ||
            (slot.lease_word.load(std::memory_order_acquire) & kReaderMask) !=
                0U)
            continue;
        if (slot.constructed) {
            slot.Descriptor()->~WidgetsFacadeBindingDescriptor();
            slot.constructed = false;
        }
        slot.state.store(SlotState::kEmpty, std::memory_order_release);
    }
}

bool WidgetsFacadeBindingRegistry::NeedsReclaim() const noexcept {
    return std::any_of(slots_.begin(), slots_.end(), [](const auto& slot) {
        const auto state = slot.state.load(std::memory_order_acquire);
        return state == SlotState::kRetired || state == SlotState::kReclaiming;
    });
}

void WidgetsFacadeBindingRegistry::Shutdown() noexcept {
    if (shutdown_.exchange(true, std::memory_order_acq_rel))
        return;
    published_.store(kInvalidPublication, std::memory_order_release);
    for (auto& slot : slots_)
        Close(slot);
    for (auto& slot : slots_)
        DestroyClosed(slot, SlotState::kShutdown);
}

bool WidgetsFacadeBindingRegistry::Ready(std::uint8_t slot) const noexcept {
    return slot < slots_.size() &&
           slots_[slot].state.load(std::memory_order_acquire) ==
               SlotState::kReady &&
           slots_[slot].lease_word.load(std::memory_order_acquire) == 0U;
}

bool WidgetsFacadeBindingRegistry::AuditClosedLeaseProtocol() const noexcept {
    for (const auto& slot : slots_) {
        const auto state = slot.state.load(std::memory_order_acquire);
        const auto word = slot.lease_word.load(std::memory_order_acquire);
        if ((state == SlotState::kEmpty || state == SlotState::kShutdown ||
             state == SlotState::kReclaiming) &&
            (word & kClosed) == 0U)
            return false;
    }
    return true;
}

bool WidgetsFacadeBindingRegistry::RunClosedLeaseProtocolSmokeCheck() {
    std::atomic<std::uint64_t> word = 0U;
    std::atomic_bool stale_loaded = false;
    std::atomic_bool close_complete = false;
    std::atomic_bool stale_increment_rejected = false;
    std::thread stale_reader([&]() {
        auto stale = word.load(std::memory_order_acquire);
        stale_loaded.store(true, std::memory_order_release);
        while (!close_complete.load(std::memory_order_acquire))
            std::this_thread::yield();
        stale_increment_rejected.store(
            !word.compare_exchange_strong(stale, stale + 1U,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire) &&
                (stale & kClosed) != 0U,
            std::memory_order_release);
    });
    while (!stale_loaded.load(std::memory_order_acquire))
        std::this_thread::yield();
    word.fetch_or(kClosed, std::memory_order_acq_rel);
    close_complete.store(true, std::memory_order_release);
    stale_reader.join();

    word.store(0U, std::memory_order_release);
    std::atomic_bool increment_complete = false;
    std::atomic_bool allow_release = false;
    std::atomic_bool increment_before_close = false;
    std::thread validated_reader([&]() {
        auto open = std::uint64_t{0U};
        increment_before_close.store(
            word.compare_exchange_strong(open, 1U, std::memory_order_acq_rel,
                                         std::memory_order_acquire),
            std::memory_order_release);
        increment_complete.store(true, std::memory_order_release);
        while (!allow_release.load(std::memory_order_acquire))
            std::this_thread::yield();
        word.fetch_sub(1U, std::memory_order_release);
    });
    while (!increment_complete.load(std::memory_order_acquire))
        std::this_thread::yield();
    const auto closed_with_reader =
        word.fetch_or(kClosed, std::memory_order_acq_rel) | kClosed;
    allow_release.store(true, std::memory_order_release);
    validated_reader.join();
    const auto released = word.load(std::memory_order_acquire);

    const auto first = Pack(0U, 1U);
    const auto reused = Pack(0U, 2U);
    if (!stale_increment_rejected.load(std::memory_order_acquire) ||
        !increment_before_close.load(std::memory_order_acquire) ||
        closed_with_reader != (kClosed | 1U) || released != kClosed ||
        first == reused || PublishedGeneration(first) != 1U ||
        PublishedGeneration(reused) != 2U || !AuditPublicationOperations() ||
        !ReaderCountWouldOverflow(kReaderMask) ||
        ReaderCountWouldOverflow(kReaderMask - 1U)) {
        return false;
    }

    WidgetsFacadeBindingRegistry registry;
    const auto seed = [&registry](std::uint8_t index, std::uint64_t generation,
                                  SlotState state) {
        auto& slot = registry.slots_[index];
        if (slot.constructed ||
            slot.lease_word.load(std::memory_order_acquire) != kClosed ||
            slot.state.load(std::memory_order_acquire) != SlotState::kEmpty) {
            return false;
        }
        slot.state.store(SlotState::kPreparing, std::memory_order_release);
        new (slot.storage) WidgetsFacadeBindingDescriptor;
        slot.constructed = true;
        slot.generation.store(generation, std::memory_order_release);
        slot.state.store(state, std::memory_order_release);
        slot.lease_word.store(0U, std::memory_order_release);
        return slot.Descriptor()->generation == 0U;
    };

    if (!seed(0U, 1U, SlotState::kActive))
        return false;
    registry.published_.store(Pack(0U, 1U), std::memory_order_release);
    auto late_release = registry.Acquire();
    if (!late_release ||
        registry.slots_[0].lease_word.load(std::memory_order_acquire) != 1U)
        return false;

    if (!seed(1U, 2U, SlotState::kReady) || !registry.Publish(1U))
        return false;
    if (registry.slots_[0].state.load(std::memory_order_acquire) !=
            SlotState::kRetired ||
        registry.slots_[0].lease_word.load(std::memory_order_acquire) !=
            (kClosed | 1U)) {
        return false;
    }

    registry.ReclaimRetired();
    if (registry.slots_[0].state.load(std::memory_order_acquire) !=
            SlotState::kReclaiming ||
        !registry.slots_[0].constructed ||
        registry.Prepare(WidgetsFacadeBindingDescriptor{}).has_value()) {
        return false;
    }
    late_release = {};
    registry.ReclaimRetired();
    if (registry.slots_[0].state.load(std::memory_order_acquire) !=
            SlotState::kEmpty ||
        registry.slots_[0].constructed ||
        registry.slots_[0].lease_word.load(std::memory_order_acquire) !=
            kClosed) {
        return false;
    }

    auto current = registry.Acquire();
    if (!current)
        return false;
    current = {};
    if (!seed(2U, 3U, SlotState::kReady) || !registry.Publish(2U))
        return false;
    registry.ReclaimRetired();
    if (registry.slots_[1].state.load(std::memory_order_acquire) !=
        SlotState::kEmpty) {
        return false;
    }
    if (!seed(0U, 4U, SlotState::kReady) || !registry.Publish(0U))
        return false;
    registry.ReclaimRetired();
    if (registry.slots_[2].state.load(std::memory_order_acquire) !=
            SlotState::kEmpty ||
        registry.published_.load(std::memory_order_acquire) != Pack(0U, 4U) ||
        PublishedGeneration(first) ==
            PublishedGeneration(
                registry.published_.load(std::memory_order_acquire))) {
        return false;
    }

    auto shutdown_lease = registry.Acquire();
    if (!shutdown_lease)
        return false;
    std::atomic_bool shutdown_started = false;
    std::atomic_bool shutdown_finished = false;
    std::thread shutdown_thread([&]() {
        shutdown_started.store(true, std::memory_order_release);
        registry.Shutdown();
        shutdown_finished.store(true, std::memory_order_release);
    });
    while (!shutdown_started.load(std::memory_order_acquire) ||
           !registry.shutdown_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    if (shutdown_finished.load(std::memory_order_acquire) ||
        registry.Acquire()) {
        shutdown_lease = {};
        shutdown_thread.join();
        return false;
    }
    shutdown_lease = {};
    shutdown_thread.join();
    return shutdown_finished.load(std::memory_order_acquire) &&
           registry.AuditClosedLeaseProtocol();
}

}  // namespace goldendict::widgets
