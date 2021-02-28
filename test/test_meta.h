#ifndef ODR_TESTMETA_H
#define ODR_TESTMETA_H

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

class TestMeta {
public:
  static std::string data_directory();
  static std::string data_input_directory();

  static std::vector<std::string> test_file_paths();
  static TestFile test_file(const std::string &path);
  static std::string test_file_path(const std::string &path);

  TestMeta(const TestMeta &) = delete;
  TestMeta &operator=(const TestMeta &) = delete;
  TestMeta(TestMeta &&) = delete;
  TestMeta &operator=(TestMeta &&) = delete;

private:
  TestMeta();

  static TestMeta &instance_();
  std::vector<std::string> test_file_paths_() const;
  TestFile test_file_(const std::string &path) const;

  std::unordered_map<std::string, TestFile> m_test_files;
};

} // namespace odr::test

#endif // ODR_TESTMETA_H
