// ------------------------------------------------------
//  Copyright (C) 2021 SHANGHAI INSTITUTE OF LASER TECHNOLOGY.
//                  - All Rights Reserved -
//           
//  Unauthorized copying of this file, via any medium is strictly prohibited
//  Proprietary and confidential
//  
//  Written by Xiling Huang <huangxiling@silt.top>
//  Created:     2021-03-17    16:33
// ------------------------------------------------------

#pragma once
#include <QSettings>

// ------------------------------------------------------
// Settings usage:
//     1.
namespace ti
{
    namespace settings
    {
        /**
         * \brief Where to store settings.
         */
        constexpr auto kFileName = "config/system.ini";
        /**
         * \brief Setting save type
         */
        constexpr auto kSettingType = QSettings::IniFormat;

        namespace config
        {
        }
    }
}
