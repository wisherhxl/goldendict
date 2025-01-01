/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-06-04
 */

#ifndef FILE_UTILS_H_
#define FILE_UTILS_H_

#include <QString>
#include "tiger/base.hpp"

namespace ti {
/**
 * @brief Select images from a specified directory.
 * 
 * This function retrieves the list of image files found in a directory specified by 
 * the `dir_url` parameter and stores the list of file names in `file_list`.
 * 
 * @param dir_url The directory URL to search for image files.
 * @param file_list A reference to a QStringList where the image file names will be stored.
 */
void TI_EXPORTS GetImageFilesInDirectory(const QString& dir_url,
                                         QStringList& file_list);

/**
 * @brief Select files of a specific type from a directory.
 * 
 * This function retrieves the list of files with a given extension found in the 
 * directory specified by `dir_url` and stores the list of file names in `file_list`.
 * 
 * @param dir_url The directory URL to search for files.
 * @param file_type The file extension to filter the files, e.g., `.exe`, `.bmp`.
 * @param file_list A reference to a QStringList where the file names will be stored.
 */
void TI_EXPORTS GetFilesInDirectory(const QString& dir_url,
                                    const QString& file_type,
                                    QStringList& file_list);

/**
 * @brief Generate a file name based on the current date and time.
 * 
 * This function generates a file name by appending the current date and time (formatted
 * according to `date_format`) to the provided `prefix` and `suffix`. The `date_format`
 * parameter allows customization of the format.
 * 
 * @param prefix The prefix to be added to the file name.
 * @param suffix The suffix (including the dot) to be added to the file name.
 * @param date_format The format string for the date and time, default is "yyyy_MM_dd_hh_mm_ss_z".
 * 
 * @return A QString containing the generated file name.
 */
QString TI_EXPORTS
GenerateFileNameByTime(const QString& prefix, const QString& suffix,
                       const QString& date_format = "yyyy_MM_dd_hh_mm_ss_z");

}  // namespace ti

#endif  // FILE_UTILS_H_