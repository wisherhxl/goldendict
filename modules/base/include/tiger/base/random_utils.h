/*
 * Copyright (c) 2023 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2023-08-21
 */

#ifndef RANDOM_UTILS_H_
#define RANDOM_UTILS_H_

#include "tiger/base.hpp"

namespace std {
class random_device;
}  // namespace std

namespace ti {
/**
 * @brief This RandomUtil class provides an interface for generating random
 * numbers or strings.
 */
class TI_EXPORTS RandomUtil {
   public:
    /**
     * @brief This function is used to generate a string of a specified length.
     * @param length Specify the string length.
     * @return String generated.
     */
    static std::string getRandomString(int length);

    /**
     * @brief This function is used to generate a uniformly distributed number
     * between min and max.
     * @param min Minimum generated value.
     * @param max Maximum generated value.
     * @return Integer generated.
     */
    static int getRandomInt(int min, int max);

    /**
     * @brief This function is used to generate a uniformly distributed
     * double-precision floating-point number between min and max.
     * @param min Minimum generated value.
     * @param max Maximum generated value.
     * @return Floating-point number generated.
     */
    static double getRandomDouble(double min, double max);
};
}  // namespace ti

#endif  // RANDOM_UTILS_H_