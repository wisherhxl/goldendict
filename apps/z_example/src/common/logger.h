// ------------------------------------------------------
//  Copyright (C) 2021 SHANGHAI INSTITUTE OF LASER TECHNOLOGY.
//                  - All Rights Reserved -
//           
//  Unauthorized copying of this file, via any medium is strictly prohibited
//  Proprietary and confidential
//  
//  Written by Xiling Huang <huangxiling@silt.top>
//  Created:     2021-03-17    16:39
// ------------------------------------------------------

#pragma once

#include <QDir>
#include <log4cplus/logger.h>

namespace ti
{
    /**
     * \brief 
     * \param type 
     * \param context 
     * \param msg 
     */
    void laserLogOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg);
}
