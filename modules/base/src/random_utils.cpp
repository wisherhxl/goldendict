/*
 * Copyright (c) 2020-2022, Shanghai Institute of Laser Development Team
 *
 * SPDX-License-Identifier: MIT License
 *
 * Change Logs:
 * Date           Author           Notes
 * 2023-08-21     Huang Xiling     first version
 */

#include "tiger/base/random_utils.h"

#include <random>
#include <string>

namespace ti {

std::string RandomUtil::getRandomString(int length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

    std::string res;
    res.reserve(length);
    for (int i = 0; i < length; ++i) {
        res += charset[getRandomInt(0, sizeof(charset) - 2)];
    }

    return res;
}

int RandomUtil::getRandomInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(min, max);

    return distribution(generator);
}

double RandomUtil::getRandomDouble(double min, double max) {
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_real_distribution<double> distribution(min, max);

    return distribution(generator);
}

}  // namespace ti
