// ------------------------------------------------------
//  Copyright (C) 2021 SHANGHAI INSTITUTE OF LASER TECHNOLOGY.
//                  - All Rights Reserved -
//           
//  Unauthorized copying of this file, via any medium is strictly prohibited
//  Proprietary and confidential
//  
//  Written by Xiling Huang <huangxiling@silt.top>
//  Created:     2021-03-17    16:20
// ------------------------------------------------------

#pragma once

#include "Windows.h"
#include <memory>

namespace ti
{
    class SingleApplication
    {
    public:
        explicit SingleApplication();
        ~SingleApplication();
        SingleApplication(SingleApplication&) = delete;
        SingleApplication(SingleApplication&&) = delete;
        SingleApplication& operator=(SingleApplication&) = delete;
        SingleApplication& operator=(SingleApplication&&) = delete;

        bool isValid() const;
    private:
        std::unique_ptr<HANDLE> global_mutex_;
        bool valid_;
    };
}
