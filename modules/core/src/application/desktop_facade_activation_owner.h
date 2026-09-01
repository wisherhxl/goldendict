// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_APPLICATION_DESKTOP_FACADE_ACTIVATION_OWNER_H_
#define GOLDENDICT_CORE_SRC_APPLICATION_DESKTOP_FACADE_ACTIVATION_OWNER_H_

#include <memory>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/application.h"
#include "goldendict/core/desktop_facade.h"
#include "goldendict/core/runtime_dictionary_source.h"

namespace goldendict::core::application {

struct DictionaryServiceActivationCandidate;
GOLDENDICT_EXPORTS DictionaryServiceActivationCandidate
CreateDictionaryServiceActivationCandidate(
    const CoreConfiguration& configuration,
    std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources);

class GOLDENDICT_EXPORTS ServiceStateActivationHandle final {
   public:
    ServiceStateActivationHandle() noexcept;
    ~ServiceStateActivationHandle();

    ServiceStateActivationHandle(ServiceStateActivationHandle&&) noexcept;
    ServiceStateActivationHandle& operator=(
        ServiceStateActivationHandle&&) noexcept;

    ServiceStateActivationHandle(const ServiceStateActivationHandle&) = delete;
    ServiceStateActivationHandle& operator=(
        const ServiceStateActivationHandle&) = delete;

    bool SubmitOnceWithDefaults();
    void ShutdownAndJoin() noexcept;

   private:
    class Impl;
    explicit ServiceStateActivationHandle(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;

    friend struct DictionaryServiceActivationCandidate;
    friend DictionaryServiceActivationCandidate
    CreateDictionaryServiceActivationCandidate(
        const CoreConfiguration&,
        std::vector<std::unique_ptr<RuntimeDictionarySource>>);
};

struct DictionaryServiceActivationCandidate {
    std::unique_ptr<DictionaryService> service;
    ServiceStateActivationHandle activation;
};

GOLDENDICT_EXPORTS bool IsFullTextIndexExecutorStopped(
    const DictionaryService& service) noexcept;

struct DesktopFacadeActivationCandidate {
    std::shared_ptr<DesktopFacade> facade;
    ServiceStateActivationHandle activation;
};

GOLDENDICT_EXPORTS DesktopFacadeActivationCandidate
CreateDesktopFacadeActivationCandidate(const CoreConfiguration& configuration);
GOLDENDICT_EXPORTS DesktopFacadeActivationCandidate
CreateDesktopFacadeActivationCandidate(
    const CoreConfiguration& configuration,
    std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources);

class DesktopFacadeActivationOwner;
class CoreFacadeActivationTestAccess;
class ReservedCoreFacadeCandidate;
class PublishedCoreFacadeCandidate;

class GOLDENDICT_EXPORTS PreparedCoreFacadeCandidate final {
   public:
    PreparedCoreFacadeCandidate() noexcept;
    ~PreparedCoreFacadeCandidate();

    PreparedCoreFacadeCandidate(const PreparedCoreFacadeCandidate&) = delete;
    PreparedCoreFacadeCandidate& operator=(const PreparedCoreFacadeCandidate&) =
        delete;
    PreparedCoreFacadeCandidate(PreparedCoreFacadeCandidate&&) noexcept;
    PreparedCoreFacadeCandidate& operator=(
        PreparedCoreFacadeCandidate&&) noexcept;

    explicit operator bool() const noexcept;
    void Abandon() noexcept;

   private:
    class Impl;
    explicit PreparedCoreFacadeCandidate(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;

    friend class DesktopFacadeActivationOwner;
    friend class CoreFacadeActivationTestAccess;
    friend class ReservedCoreFacadeCandidate;
};

class GOLDENDICT_EXPORTS ReservedCoreFacadeCandidate final {
   public:
    ReservedCoreFacadeCandidate() noexcept;
    ~ReservedCoreFacadeCandidate();
    ReservedCoreFacadeCandidate(const ReservedCoreFacadeCandidate&) = delete;
    ReservedCoreFacadeCandidate& operator=(const ReservedCoreFacadeCandidate&) =
        delete;
    ReservedCoreFacadeCandidate(ReservedCoreFacadeCandidate&&) noexcept;
    ReservedCoreFacadeCandidate& operator=(
        ReservedCoreFacadeCandidate&&) noexcept;

    explicit operator bool() const noexcept;
    void Abort() noexcept;

   private:
    explicit ReservedCoreFacadeCandidate(
        std::unique_ptr<PreparedCoreFacadeCandidate::Impl> impl) noexcept;
    std::unique_ptr<PreparedCoreFacadeCandidate::Impl> impl_;
    friend class DesktopFacadeActivationOwner;
};

class GOLDENDICT_EXPORTS PublishedCoreFacadeCandidate final {
   public:
    PublishedCoreFacadeCandidate() noexcept;
    ~PublishedCoreFacadeCandidate();
    PublishedCoreFacadeCandidate(const PublishedCoreFacadeCandidate&) = delete;
    PublishedCoreFacadeCandidate& operator=(
        const PublishedCoreFacadeCandidate&) = delete;
    PublishedCoreFacadeCandidate(PublishedCoreFacadeCandidate&&) noexcept;
    PublishedCoreFacadeCandidate& operator=(
        PublishedCoreFacadeCandidate&&) noexcept;

    explicit operator bool() const noexcept;

   private:
    class Impl;
    explicit PublishedCoreFacadeCandidate(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
    friend class DesktopFacadeActivationOwner;
};

class GOLDENDICT_EXPORTS DesktopFacadeActivationOwner final {
   public:
    DesktopFacadeActivationOwner();
    ~DesktopFacadeActivationOwner();

    DesktopFacadeActivationOwner(const DesktopFacadeActivationOwner&) = delete;
    DesktopFacadeActivationOwner& operator=(
        const DesktopFacadeActivationOwner&) = delete;

    PreparedCoreFacadeCandidate PrepareCandidate(
        const CoreConfiguration& configuration);
    PreparedCoreFacadeCandidate PrepareCandidate(
        const CoreConfiguration& configuration,
        std::vector<std::unique_ptr<RuntimeDictionarySource>> runtime_sources);
    std::shared_ptr<DesktopFacade> PreparedFacadeSnapshot(
        const PreparedCoreFacadeCandidate& candidate) const noexcept;
    bool Activate(PreparedCoreFacadeCandidate& candidate) noexcept;
    ReservedCoreFacadeCandidate Reserve(
        PreparedCoreFacadeCandidate& candidate) noexcept;
    void PublishReserved(ReservedCoreFacadeCandidate& reserved) noexcept;
    PublishedCoreFacadeCandidate PublishReservedOnly(
        ReservedCoreFacadeCandidate& reserved) noexcept;
    void FinishPublished(PublishedCoreFacadeCandidate& published) noexcept;
    bool Shutdown() noexcept;
    std::shared_ptr<DesktopFacade> CurrentSnapshot() const;

   private:
    friend class PreparedCoreFacadeCandidate;
    friend class ReservedCoreFacadeCandidate;
    friend class PublishedCoreFacadeCandidate;
    friend class CoreFacadeActivationTestAccess;
    class Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace goldendict::core::application

#endif  // GOLDENDICT_CORE_SRC_APPLICATION_DESKTOP_FACADE_ACTIVATION_OWNER_H_
