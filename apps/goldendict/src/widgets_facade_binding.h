// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_WIDGETS_FACADE_BINDING_H_
#define GOLDENDICT_APPS_GOLDENDICT_WIDGETS_FACADE_BINDING_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "goldendict/core/desktop_facade.h"

namespace goldendict::widgets {

enum class FacadeBindingConsumer : std::uint32_t {
    kSchemeHandler = 1U << 0U,
    kDictionaryBrowser = 1U << 1U,
    kFullTextDialog = 1U << 2U,
    kArticlePages = 1U << 3U,
    kArticleViews = 1U << 4U,
    kRenderedMatches = 1U << 5U,
    kSuggestions = 1U << 6U,
};

constexpr std::uint32_t kCompleteFacadeBindingConsumers =
    static_cast<std::uint32_t>(FacadeBindingConsumer::kSchemeHandler) |
    static_cast<std::uint32_t>(FacadeBindingConsumer::kDictionaryBrowser) |
    static_cast<std::uint32_t>(FacadeBindingConsumer::kFullTextDialog) |
    static_cast<std::uint32_t>(FacadeBindingConsumer::kArticlePages) |
    static_cast<std::uint32_t>(FacadeBindingConsumer::kArticleViews) |
    static_cast<std::uint32_t>(FacadeBindingConsumer::kRenderedMatches) |
    static_cast<std::uint32_t>(FacadeBindingConsumer::kSuggestions);

struct WidgetsFacadeBindingDescriptor final {
    std::shared_ptr<core::DesktopFacade> facade_owner;
    core::DesktopFacade* facade = nullptr;
    const core::DictionaryService* service = nullptr;
    std::vector<core::DictionaryIdentity> catalog;
    std::unordered_map<std::string, std::size_t> catalog_index;
    std::uint64_t generation = 0U;
    std::uint32_t consumers = 0U;

    bool RegisterConsumer(FacadeBindingConsumer consumer) noexcept {
        const auto bit = static_cast<std::uint32_t>(consumer);
        if ((consumers & bit) != 0U)
            return false;
        consumers |= bit;
        return true;
    }

    bool Complete() const noexcept {
        return facade_owner != nullptr && facade == facade_owner.get() &&
               service != nullptr && generation != 0U &&
               catalog_index.size() == catalog.size() &&
               consumers == kCompleteFacadeBindingConsumers;
    }
};

class WidgetsFacadeBindingRegistry final {
    struct Slot;

   public:
    enum class PublicationOperation : std::uint8_t {
        kActivatePreparedBindingState,
        kPublishSlotAndGeneration,
        kRetirePreviousBindingState,
        kClosePreviousLeaseWord,
        kStorePresentationAliases,
        kStoreHostIndices,
        kPublishRelayAtomics,
        kOpenInteractionGate,
        kConsumeCandidateToken,
    };

    static constexpr std::array<PublicationOperation, 9> kPublicationOperations{
        {
            PublicationOperation::kActivatePreparedBindingState,
            PublicationOperation::kPublishSlotAndGeneration,
            PublicationOperation::kRetirePreviousBindingState,
            PublicationOperation::kClosePreviousLeaseWord,
            PublicationOperation::kStorePresentationAliases,
            PublicationOperation::kStoreHostIndices,
            PublicationOperation::kPublishRelayAtomics,
            PublicationOperation::kOpenInteractionGate,
            PublicationOperation::kConsumeCandidateToken,
        }};
    static constexpr std::uint64_t kClosed = std::uint64_t{1} << 63U;
    static constexpr std::uint64_t kReaderMask = kClosed - 1U;
    static constexpr std::uint64_t kInvalidPublication = 3U;

    enum class SlotState : std::uint8_t {
        kEmpty,
        kPreparing,
        kReady,
        kActive,
        kRetired,
        kReclaiming,
        kShutdown,
    };

    class Lease final {
       public:
        Lease() noexcept = default;

        ~Lease() { Reset(); }

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        Lease(Lease&& other) noexcept { MoveFrom(other); }

        Lease& operator=(Lease&& other) noexcept {
            if (this != &other) {
                Reset();
                MoveFrom(other);
            }
            return *this;
        }

        explicit operator bool() const noexcept {
            return descriptor_ != nullptr;
        }

        const WidgetsFacadeBindingDescriptor* operator->() const noexcept {
            return descriptor_;
        }

        const WidgetsFacadeBindingDescriptor& operator*() const noexcept {
            return *descriptor_;
        }

       private:
        Lease(Slot* slot,
              const WidgetsFacadeBindingDescriptor* descriptor) noexcept
            : slot_(slot), descriptor_(descriptor) {}

        void Reset() noexcept;

        void MoveFrom(Lease& other) noexcept {
            slot_ = std::exchange(other.slot_, nullptr);
            descriptor_ = std::exchange(other.descriptor_, nullptr);
        }

        Slot* slot_ = nullptr;
        const WidgetsFacadeBindingDescriptor* descriptor_ = nullptr;
        friend class WidgetsFacadeBindingRegistry;
    };

    WidgetsFacadeBindingRegistry() noexcept;
    ~WidgetsFacadeBindingRegistry();
    WidgetsFacadeBindingRegistry(const WidgetsFacadeBindingRegistry&) = delete;
    WidgetsFacadeBindingRegistry& operator=(
        const WidgetsFacadeBindingRegistry&) = delete;

    std::optional<std::uint8_t> Prepare(
        WidgetsFacadeBindingDescriptor descriptor);
    bool Publish(std::uint8_t prepared_slot) noexcept;
    void ClearPublished() noexcept;
    void Abandon(std::uint8_t prepared_slot) noexcept;
    Lease Acquire() const noexcept;
    void ReclaimRetired() noexcept;
    bool NeedsReclaim() const noexcept;
    void Shutdown() noexcept;

    bool Ready(std::uint8_t slot) const noexcept;
    bool AuditClosedLeaseProtocol() const noexcept;

    static constexpr bool AuditPublicationOperations() noexcept {
        return kPublicationOperations.size() == 9U;
    }

    static bool RunClosedLeaseProtocolSmokeCheck();

    static constexpr bool ReaderCountWouldOverflow(
        std::uint64_t word) noexcept {
        return (word & kReaderMask) == kReaderMask;
    }

   private:
    struct Slot final {
        std::atomic<SlotState> state = SlotState::kEmpty;
        std::atomic<std::uint64_t> generation = 0U;
        std::atomic<std::uint64_t> lease_word = kClosed;
        alignas(WidgetsFacadeBindingDescriptor)
            std::byte storage[sizeof(WidgetsFacadeBindingDescriptor)];
        bool constructed = false;

        WidgetsFacadeBindingDescriptor* Descriptor() noexcept {
            return std::launder(
                reinterpret_cast<WidgetsFacadeBindingDescriptor*>(storage));
        }

        const WidgetsFacadeBindingDescriptor* Descriptor() const noexcept {
            return std::launder(
                reinterpret_cast<const WidgetsFacadeBindingDescriptor*>(
                    storage));
        }
    };

    static std::uint64_t Pack(std::uint8_t slot,
                              std::uint64_t generation) noexcept {
        return (generation << 2U) | slot;
    }

    static std::uint8_t SlotIndex(std::uint64_t published) noexcept {
        return static_cast<std::uint8_t>(published & 3U);
    }

    static std::uint64_t PublishedGeneration(std::uint64_t published) noexcept {
        return published >> 2U;
    }

    static void Close(Slot& slot) noexcept;
    static void DestroyClosed(Slot& slot, SlotState final_state) noexcept;

    mutable std::array<Slot, 3> slots_;
    std::atomic<std::uint64_t> published_ = kInvalidPublication;
    std::atomic_bool shutdown_ = false;
    std::atomic<std::uint64_t> next_generation_ = 1U;
};

}  // namespace goldendict::widgets

#endif
