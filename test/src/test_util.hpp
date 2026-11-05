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
  /// The message the file must fail with: `index.csv`'s `error` column.
  std::optional<std::string> error;

  TestFile() = default;
  TestFile(std::string absolute_path, std::string short_path, FileType type,
           std::optional<std::string> password,
           std::optional<std::string> error = std::nullopt);
};

class TestData {
public:
  static std::string test_input_directory();

  /// The files that open; a file that must fail is in `malformed_test_files`.
  static std::vector<TestFile> test_files();
  static std::vector<TestFile> test_files(FileType);
  static std::vector<TestFile> malformed_test_files();

  static TestFile test_file(const std::string &short_path);
  static std::string test_file_path(const std::string &short_path);

  TestData(const TestData &) = delete;
  TestData &operator=(const TestData &) = delete;
  TestData(TestData &&) = delete;
  TestData &operator=(TestData &&) = delete;

private:
  TestData();

  [[nodiscard]] static TestData &instance_();

  [[nodiscard]] std::vector<TestFile> test_files_(FileType file_type) const;

  std::vector<TestFile> m_test_files;
};

} // namespace odr::test
