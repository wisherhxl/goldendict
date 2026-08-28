#include <opencc/SimpleConverter.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void ExpectConversion(const std::string& config_name, const std::string& input,
                      const std::string& expected) {
  const std::filesystem::path config_path =
      std::filesystem::path(OPENCC_TEST_DATA_DIR) / config_name;
  if (!std::filesystem::is_regular_file(config_path)) {
    throw std::runtime_error("Missing packaged OpenCC config: " +
                             config_path.string());
  }

  opencc::SimpleConverter converter(config_path.string());
  const std::string actual = converter.Convert(input);
  if (actual != expected) {
    throw std::runtime_error(config_name + " converted '" + input + "' to '" +
                             actual + "', expected '" + expected + "'");
  }
}

}  // namespace

int main() {
  try {
    ExpectConversion("s2tw.json", "鼠标里面", "鼠標裡面");
    ExpectConversion("s2hk.json", "鼠标里面", "鼠標裏面");
    ExpectConversion("t2s.json", "繁體中文", "繁体中文");
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
