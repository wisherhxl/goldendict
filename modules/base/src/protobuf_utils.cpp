/*
 * Copyright (c) 2020-2022, Shanghai Institute of Laser Development Team
 *
 * SPDX-License-Identifier: MIT License
 *
 * Change Logs:
 * Date           Author           Notes
 * 2023-08-21     Huang Xiling     first version
 */

#include "tiger/base/protobuf_utils.h"

#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include "google/protobuf/util/json_util.h"

namespace ti {

bool ProtobufUtil::writeMessageToJson(const google::protobuf::Message& msg, const std::string& url) {
    // Create the directory if it doesn't exist
    std::filesystem::path target_path(url);
    std::filesystem::path target_dir = target_path.parent_path();
    
    if (!std::filesystem::exists(target_dir)) {
        if (!std::filesystem::create_directories(target_dir)) {
            return false;
        }
    }

    // Convert Protobuf message to JSON string
    google::protobuf::util::JsonOptions options;
    options.add_whitespace = true;
    std::string msg_json;
    google::protobuf::util::MessageToJsonString(msg, &msg_json, options);

    // Open file for writing
    std::ofstream target_file(url);
    if (!target_file.is_open()) {
        return false;
    }

    // Write JSON string to the file
    target_file << msg_json;
    if (!target_file) {
        return false;
    }

    return true;
}

bool ProtobufUtil::readMessageFromJson(google::protobuf::Message* msg, const std::string& url) {
   // Check if the file exists
    std::ifstream target(url);
    if (!target.good()) {
        return false;
    }

    // Open the file for reading
    if (!target.is_open()) {
        return false;
    }

    // Read the entire file into a string
    std::string in_str((std::istreambuf_iterator<char>(target)), std::istreambuf_iterator<char>());

    // Close the file
    target.close();

    // Parse the JSON into the Protobuf message
    google::protobuf::util::JsonParseOptions options;
    options.ignore_unknown_fields = true;

    auto status = google::protobuf::util::JsonStringToMessage(in_str, msg, options);
    if (!status.ok()) {
        return false;
    }

    return true;
}

bool ProtobufUtil::writeMessageToBin(const google::protobuf::Message& msg, const std::string& url) {
// Use std::filesystem to manage file paths and directories
    std::filesystem::path target_path(url);
    std::filesystem::path target_dir = target_path.parent_path();

    // Create directories if they don't exist
    std::error_code ec;
    if (!std::filesystem::create_directories(target_dir, ec) && ec) {
        return false;
    }

    // Open file for binary output
    std::fstream output(url, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        return false;
    }

    // Serialize the protobuf message to the file
    if (!msg.SerializeToOstream(&output)) {
        return false;
    }

    return true;
}

bool ProtobufUtil::readMessageFromBin(google::protobuf::Message* msg, const std::string& url) {
    std::fstream input(url, std::ios::in | std::ios::binary);

    if (!input) {
        return false;
    }

    if (!msg->ParseFromIstream(&input)) {
        return false;
    }

    return true;
}

}  // namespace ti
