#ifndef ODR_TEST_TESTMETA_H
#define ODR_TEST_TESTMETA_H

#include <odr/Meta.h>
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

class TestMeta {
public:
  static TestMeta &instance();

  static std::string dataDirectory();
  static std::string dataInputDirectory();

  std::vector<std::string> testFilePaths() const;
  TestFile testFile(const std::string &) const;

  TestMeta(const TestMeta &) = delete;
  TestMeta &operator=(const TestMeta &) = delete;
  TestMeta(TestMeta &&) = delete;
  TestMeta &operator=(TestMeta &&) = delete;

private:
  TestMeta();

  std::unordered_map<std::string, TestFile> m_testFiles;
};

} // namespace odr::test

#endif // ODR_TEST_TESTMETA_H
