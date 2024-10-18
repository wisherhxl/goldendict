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

namespace google {
namespace protobuf {
class Message;
}
}  // namespace google

class QString;

namespace ti {
/**
 * @brief This ProtobufUtil class provides an interface for safely writing Protobuf message to files.
 * @note save write procedure is provided by #QSaveFile
*/
class TI_EXPORTS ProtobufUtil {
   public:
    /**
     * @brief write protobuf message to json file. The target file will not be modified when fail.
     * @note json files are user friendly, but slower than binary files. using with configurations is recommended.
     * @param msg protobuf message to write
     * @param url file location
     * @return true if success, false otherwise.
    */
    static bool writeMessageToJson(const google::protobuf::Message& msg, const QString& url);
    /**
     * @brief read protobuf message from json file.
     * @param msg protobuf message pointer to overwrite.
     * @param url file location
     * @return true if success, false otherwise.
    */
    static bool readMessageFromJson(google::protobuf::Message* msg, const QString& url);
    /**
     * @brief write protobuf message to binary file. The target file will not be modified when fail.
     * @note binary file needs less storage, and is faster when writing and reading, but less readable than json.
     * @param msg protobuf message to write
     * @param url file location
     * @return true if success, false otherwise.
    */
    static bool writeMessageToBin(const google::protobuf::Message& msg, const QString& url);
    /**
     * @brief read protobuf message from binary file.
     * @param msg protobuf message pointer to overwrite.
     * @param url file location.
     * @return true if success, false otherwise.
    */
    static bool readMessageFromBin(google::protobuf::Message* msg, const QString& url);
};
}  // namespace ti
