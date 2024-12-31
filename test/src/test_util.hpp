#pragma once

#include <odr/file.hpp>

#include <string>
#include <vector>

namespace odr::test {

struct TestFile {
  std::string absolute_path;
  std::string short_path;
  FileType type{FileType::unknown};
  std::optional<std::string> password;

  TestFile() = default;
  TestFile(std::string absolute_path, std::string short_path, FileType type,
           std::optional<std::string> password);
};

class TestData {
public:
  static std::string data_directory();
  static std::string data_input_directory();

  static std::vector<TestFile> test_files();
  static std::vector<TestFile> test_files(FileType);

  static TestFile test_file(const std::string &short_path);
  static std::string test_file_path(const std::string &short_path);

  TestData(const TestData &) = delete;
  TestData &operator=(const TestData &) = delete;
  TestData(TestData &&) = delete;
  TestData &operator=(TestData &&) = delete;

private:
  TestData();

  static TestData &instance_();
  [[nodiscard]] std::vector<TestFile> test_files_(FileType) const;

  std::vector<TestFile> m_test_files;
};

} // namespace odr::test
