// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_TESTS_SUPPORT_CONFIGURATION_FILE_PROCESS_H_
#define GOLDENDICT_CORE_TESTS_SUPPORT_CONFIGURATION_FILE_PROCESS_H_

#ifdef _WIN32
#include <windows.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include <QUuid>

namespace goldendict::core::test {

struct WindowsHandleCloser {
    void operator()(HANDLE handle) const noexcept {
        if (handle && handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};

using WindowsHandle = std::unique_ptr<void, WindowsHandleCloser>;

inline int RunConfigurationFileChild(const char* ready_name,
                                     const char* release_name) {
    WindowsHandle ready(OpenEventA(EVENT_MODIFY_STATE, FALSE, ready_name));
    WindowsHandle release(OpenEventA(SYNCHRONIZE, FALSE, release_name));
    if (!ready || !release || !SetEvent(ready.get()))
        return 2;
    return WaitForSingleObject(release.get(), 15000) == WAIT_OBJECT_0 ? 0 : 3;
}

// A real process deliberately inherits every eligible parent handle. Named
// events acknowledge readiness and end its lifetime without timing sleeps.
class ConfigurationFileChild {
   public:
    ConfigurationFileChild() {
        const auto identity = QUuid::createUuid().toString(QUuid::Id128);
        const auto ready_name =
            ("Local\\gd-config-ready-" + identity).toStdWString();
        const auto release_name =
            ("Local\\gd-config-release-" + identity).toStdWString();
        WindowsHandle ready(
            CreateEventW(nullptr, TRUE, FALSE, ready_name.c_str()));
        release_.reset(
            CreateEventW(nullptr, TRUE, FALSE, release_name.c_str()));
        if (!ready || !release_)
            throw std::runtime_error(
                "Cannot create configuration child events");
        std::wstring executable(32768, L'\0');
        const auto length = GetModuleFileNameW(
            nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0 || length >= executable.size())
            throw std::runtime_error(
                "Cannot identify configuration child executable");
        executable.resize(length);
        std::wstring command = L"\"" + executable +
                               L"\" --configuration-file-child " + ready_name +
                               L" " + release_name;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable.c_str(), command.data(), nullptr,
                            nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                            &startup, &process)) {
            throw std::system_error(static_cast<int>(GetLastError()),
                                    std::system_category(),
                                    "Cannot start configuration child");
        }
        process_.reset(process.hProcess);
        WindowsHandle thread(process.hThread);
        if (WaitForSingleObject(ready.get(), 5000) != WAIT_OBJECT_0) {
            Stop();
            throw std::runtime_error(
                "Configuration child did not become ready");
        }
    }

    ~ConfigurationFileChild() { Stop(); }

    bool IsAlive() const {
        return WaitForSingleObject(process_.get(), 0) == WAIT_TIMEOUT;
    }

   private:
    void Stop() noexcept {
        if (!process_)
            return;
        SetEvent(release_.get());
        if (WaitForSingleObject(process_.get(), 5000) != WAIT_OBJECT_0) {
            TerminateProcess(process_.get(), 4);
            WaitForSingleObject(process_.get(), 5000);
        }
        process_.reset();
    }

    WindowsHandle release_;
    WindowsHandle process_;
};

}  // namespace goldendict::core::test
#endif

#endif  // GOLDENDICT_CORE_TESTS_SUPPORT_CONFIGURATION_FILE_PROCESS_H_
