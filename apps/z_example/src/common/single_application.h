/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-03-17
 */

#ifndef SINGLE_APPLICATION_H_
#define SINGLE_APPLICATION_H_

#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace ti {

/**
 * @brief A class to ensure a single instance of the application
 * runs.
 *
 * This implementation works on Windows, Linux, and macOS using
 * platform-specific synchronization primitives.
 */
class SingleApplication {
   public:
    /**
     * @brief Constructs the SingleApplication instance.
     *
     * On Windows, it uses a named mutex. On Linux and macOS, it uses a lock
     * file.
     */
    explicit SingleApplication();

    /**
     * @brief Destroys the SingleApplication instance and releases any held
     * resources.
     */
    ~SingleApplication();

    SingleApplication(SingleApplication&) = delete;
    SingleApplication(SingleApplication&&) = delete;
    SingleApplication& operator=(SingleApplication&) = delete;
    SingleApplication& operator=(SingleApplication&&) = delete;

    /**
     * @brief Checks if this instance of the application is valid (i.e.,
     * unique).
     *
     * @return True if this is the only running instance, false otherwise.
     */
    bool isValid() const;

   private:
#ifdef _WIN32
    HANDLE global_mutex_;
#else
    int lock_file_fd_;
#endif
    bool valid_;
};

}  // namespace ti

#endif  // SINGLE_APPLICATION_H_