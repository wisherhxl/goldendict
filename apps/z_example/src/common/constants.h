/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-03-17
 */

#pragma once
#include <QSettings>

// ------------------------------------------------------
// Settings usage:
//     1.
namespace ti {
namespace settings {
/**
 * \brief Where to store settings.
 */
constexpr auto kFileName = "config/system.ini";
/**
 * \brief Setting save type
 */
constexpr auto kSettingType = QSettings::IniFormat;

namespace config {}
}  // namespace settings
}  // namespace ti
