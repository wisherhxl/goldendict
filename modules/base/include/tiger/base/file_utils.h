// ------------------------------------------------------
//  Copyright (C) 2021 SHANGHAI INSTITUTE OF LASER TECHNOLOGY.
//                  - All Rights Reserved -
//           
//  Unauthorized copying of this file, via any medium is strictly prohibited
//  Proprietary and confidential
//  
//  Written by Xiling Huang <huangxiling@silt.top>
//  Created:     2021-06-04    13:32
// ------------------------------------------------------

#pragma once

#include <QString>
#include "tiger/base.hpp"

namespace ti
{
    /**
     * \brief Select images from one directory
     * \param dir_url directory url
     * \param file_list list of images file names found in dir_url
     */
    void TI_EXPORTS GetImageFilesInDirectory(const QString& dir_url, QStringList& file_list);
    /**
     * \brief Select file_type from one directory
     * \param dir_url directory url
     * \param file_type extension of the file for example: .exe, .bmp etc.
     * \param file_list list of images file names found in dir_url
     */
    void TI_EXPORTS GetFilesInDirectory(const QString& dir_url, const QString& file_type, QStringList& file_list);
    /**
     * \brief generate file name by current date and time
     * \param prefix file name prefix
     * \param suffix file name suffix "." should be included
     * \param date_format format of date 
     * \return filename
     */
    QString TI_EXPORTS GenerateFileNameByTime(const QString& prefix, const QString& suffix,
                                              const QString& date_format = "yyyy_MM_dd_hh_mm_ss_z");
}
