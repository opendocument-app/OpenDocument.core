#include <access/Path.h>
#include <csv.hpp>
#include <filesystem>
#include <test/TestMeta.h>

namespace fs = std::filesystem;

namespace odr::test {

namespace {
TestFile getTestFile(std::string input) {
  const FileType type =
      FileMeta::typeByExtension(access::Path(input).extension());
  const std::string fileName = fs::path(input).filename();
  std::string password;
  if (const auto left = fileName.find('$'), right = fileName.rfind('$');
      (left != std::string::npos) && (left != right))
    password = fileName.substr(left, right);
  const bool encrypted = !password.empty();

  return {std::move(input), type, encrypted, std::move(password)};
}

std::vector<TestFile> getTestFiles(const std::string &input) {
  if (fs::is_regular_file(input))
    return {getTestFile(input)};
  if (!fs::is_directory(input))
    return {};

  std::vector<TestFile> result;

  if (const std::string index = input + "/index.csv";
      fs::is_regular_file(index)) {
    for (auto &&row : csv::CSVReader(index)) {
      const std::string path = input + "/" + row["path"].get<>();
      const FileType type = FileMeta::typeByExtension(row["type"].get<>());
      std::string password = row["password"].get<>();
      const bool encrypted = !password.empty();
      const std::string fileName = fs::path(path).filename();

      if (type == FileType::UNKNOWN)
        continue;
      result.emplace_back(path, type, encrypted, std::move(password));
    }
  }

  // TODO this will also recurse `.git`
  for (auto &&p : fs::recursive_directory_iterator(input)) {
    if (!p.is_regular_file())
      continue;
    const std::string path = p.path().string();
    if (const auto it =
            std::find_if(std::begin(result), std::end(result),
                         [&](auto &&file) { return file.path == path; });
        it != std::end(result))
      continue;

    const auto file = getTestFile(path);

    if (file.type == FileType::UNKNOWN)
      continue;
    result.push_back(file);
  }

  return result;
}

std::unordered_map<std::string, TestFile> getTestFiles() {
  std::unordered_map<std::string, TestFile> result;

  for (const auto &e :
       fs::directory_iterator(test::TestMeta::dataInputDirectory())) {
    const auto files = getTestFiles(e.path().string());
    for (auto &&file : files) {
      std::string name =
          file.path.substr(TestMeta::dataInputDirectory().length() + 1);
      result[name] = file;
    }
  }

  return result;
}
} // namespace

TestFile::TestFile(std::string path, const FileType type, const bool encrypted,
                   std::string password)
    : path{std::move(path)}, type{type}, encrypted{encrypted},
      password{std::move(password)} {}

TestMeta::TestMeta() { m_testFiles = getTestFiles(); }

TestMeta &TestMeta::instance() {
  static TestMeta instance;
  return instance;
}

std::vector<std::string> TestMeta::testFiles() const {
  std::vector<std::string> result;
  for (auto &&file : m_testFiles) {
    result.push_back(file.first);
  }
  std::sort(std::begin(result), std::end(result));
  return result;
}

TestFile TestMeta::testFile(const std::string &path) const {
  return m_testFiles.at(path);
}

} // namespace odr::test
