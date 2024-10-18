/*
 * Copyright (c) 2020-2022, Shanghai Institute of Laser Development Team
 *
 * SPDX-License-Identifier: MIT License
 *
 * Change Logs:
 * Date           Author           Notes
 * 2023-08-21     Huang Xiling     first version
 */

#pragma once

#include "tiger/base.hpp"

namespace std {
class random_device;
}  // namespace std

namespace ti {
/**
 * \brief This RandomUtil class provides an interface for generating random numbers or strings.
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
     * @brief This function is used to generate a uniformly distributed integer between min and max.
     * @param min Minimum generated value.
     * @param max Maximum generated value.
     * @return Integer generated.
    */
    static int getRandomInt(int min, int max);
    /**
     * @brief This function is used to generate a uniformly distributed double-precision floating-point
     * number between min and max.
     * @param min Minimum generated value.
     * @param max Maximum generated value.
     * @return Floating-point number generated.
    */
    static double getRandomDouble(double min, double max);
};
}  // namespace ti
