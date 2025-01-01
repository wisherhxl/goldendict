/*
 * Copyright (c) 2023 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2023-08-12
 */

#ifndef PROTOBUF_UTILS_H_
#define PROTOBUF_UTILS_H_

#include "tiger/base.hpp"

namespace google {
namespace protobuf {
class Message;
}
}  // namespace google

class QString;

namespace ti {
/**
 * @brief This ProtobufUtil class provides an interface for safely writing
 * Protobuf message to files.
 * @note save write procedure is provided by #QSaveFile
 */
class TI_EXPORTS ProtobufUtil {
   public:
    /**
     * @brief write protobuf message to json file. The target file will not be
     * modified when fail.
     * @note json files are user friendly, but slower than binary files. using
     * with configurations is recommended.
     * @param msg protobuf message to write
     * @param url file location
     * @return true if success, false otherwise.
     */
    static bool writeMessageToJson(const google::protobuf::Message& msg,
                                   const QString& url);
    /**
     * @brief read protobuf message from json file.
     * @param msg protobuf message pointer to overwrite.
     * @param url file location
     * @return true if success, false otherwise.
     */
    static bool readMessageFromJson(google::protobuf::Message* msg,
                                    const QString& url);
    /**
     * @brief write protobuf message to binary file. The target file will not be
     * modified when fail.
     * @note binary file needs less storage, and is faster when writing and
     * reading, but less readable than json.
     * @param msg protobuf message to write
     * @param url file location
     * @return true if success, false otherwise.
     */
    static bool writeMessageToBin(const google::protobuf::Message& msg,
                                  const QString& url);
    /**
     * @brief read protobuf message from binary file.
     * @param msg protobuf message pointer to overwrite.
     * @param url file location.
     * @return true if success, false otherwise.
     */
    static bool readMessageFromBin(google::protobuf::Message* msg,
                                   const QString& url);
};
}  // namespace ti

#endif  // PROTOBUF_UTILS_H_