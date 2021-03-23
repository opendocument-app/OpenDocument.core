#ifndef ODR_TEST_META_H
#define ODR_TEST_META_H

#include <nlohmann/json.hpp>
#include <odr/file.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::test {

struct TestFile {
  std::string path;
  FileType type{FileType::UNKNOWN};
  bool password_encrypted{false};
  std::string password;

  TestFile() = default;
  TestFile(std::string path, FileType type, bool password_encrypted,
           std::string password);
};

class TestData {
public:
  static std::string data_directory();
  static std::string data_input_directory();

  static std::vector<std::string> test_file_paths();
  static TestFile test_file(const std::string &path);
  static std::string test_file_path(const std::string &path);

  TestData(const TestData &) = delete;
  TestData &operator=(const TestData &) = delete;
  TestData(TestData &&) = delete;
  TestData &operator=(TestData &&) = delete;

private:
  TestData();

  static TestData &instance_();
  std::vector<std::string> test_file_paths_() const;
  TestFile test_file_(const std::string &path) const;

  std::unordered_map<std::string, TestFile> m_test_files;
};

} // namespace odr::test

#endif // ODR_TEST_META_H
