#ifndef ODR_TEST_TESTMETA_H
#define ODR_TEST_TESTMETA_H

#include <odr/file.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::test {

struct TestFile {
  std::string path;
  FileType type{FileType::UNKNOWN};
  bool encrypted{false};
  std::string password;

  TestFile() = default;
  TestFile(std::string path, FileType type, bool encrypted,
           std::string password);
};

class test_meta {
public:
  static test_meta &instance();

  static std::string dataDirectory();
  static std::string dataInputDirectory();

  std::vector<std::string> testFilePaths() const;
  TestFile testFile(const std::string &) const;

  test_meta(const test_meta &) = delete;
  test_meta &operator=(const test_meta &) = delete;
  test_meta(test_meta &&) = delete;
  test_meta &operator=(test_meta &&) = delete;

private:
  test_meta();

  std::unordered_map<std::string, TestFile> m_testFiles;
};

} // namespace odr::test

#endif // ODR_TEST_TESTMETA_H
