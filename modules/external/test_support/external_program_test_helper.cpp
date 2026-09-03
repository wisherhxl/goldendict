// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argc < 2)
        return 64;

    const std::string mode = argv[1];
    if (mode == "arg" || mode == "argv") {
        if (argc < 3)
            return 64;
        std::cout << argv[2];
    } else if (mode == "stdin") {
        std::cout << std::string(std::istreambuf_iterator<char>(std::cin),
                                 std::istreambuf_iterator<char>());
    } else if (mode == "utf16") {
        constexpr char output[] = {'\xff', '\xfe', 'H', '\0', 'i', '\0'};
        std::cout.write(output, sizeof(output));
    } else if (mode == "stderr") {
        std::cerr << "failure";
        return 7;
    } else if (mode == "slow") {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    } else if (mode == "large") {
        std::cout << "0123456789";
    } else if (mode == "html") {
        std::cout << "<p>safe<script>bad()</script></p>";
    } else if (mode == "pwd") {
        std::cout << std::filesystem::current_path().string();
    } else if (mode == "prefix") {
        std::cout << "Alpha\r\n\r\nAlpine\nAlbatross\r";
    } else if (mode == "invalid") {
        constexpr char invalid = '\xff';
        std::cout.write(&invalid, 1);
    } else if (mode == "fail") {
        return 7;
    } else {
        return 64;
    }
    return std::cout ? 0 : 74;
}
