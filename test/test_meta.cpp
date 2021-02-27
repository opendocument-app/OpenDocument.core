#include <common/path.h>
#include <csv.hpp>
#include <filesystem>
#include <test/test_meta.h>

namespace fs = std::filesystem;

namespace odr::test {

namespace {
TestFile get_test_file(std::string input) {
  const FileType type =
      FileMeta::type_by_extension(common::Path(input).extension());
  const std::string fileName = fs::path(input).filename();
  std::string password;
  if (const auto left = fileName.find('$'), right = fileName.rfind('$');
      (left != std::string::npos) && (left != right))
    password = fileName.substr(left, right);
  const bool encrypted = !password.empty();

  return {std::move(input), type, encrypted, std::move(password)};
}

std::vector<TestFile> get_test_files(const std::string &input) {
  if (fs::is_regular_file(input))
    return {get_test_file(input)};
  if (!fs::is_directory(input))
    return {};

  std::vector<TestFile> result;

  if (const std::string index = input + "/index.csv";
      fs::is_regular_file(index)) {
    for (auto &&row : csv::CSVReader(index)) {
      const std::string path = input + "/" + row["path"].get<>();
      const FileType type = FileMeta::type_by_extension(row["type"].get<>());
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

    const auto file = get_test_file(path);

    if (file.type == FileType::UNKNOWN)
      continue;
    result.push_back(file);
  }

  return result;
}

std::unordered_map<std::string, TestFile> get_test_files() {
  std::unordered_map<std::string, TestFile> result;

  for (const auto &e :
       fs::directory_iterator(test::TestMeta::data_input_directory())) {
    const auto files = get_test_files(e.path().string());
    for (auto &&file : files) {
      std::string testPath =
          file.path.substr(TestMeta::data_input_directory().length() + 1);
      result[testPath] = file;
    }
  }

  return result;
}
} // namespace

TestFile::TestFile(std::string path, const FileType type,
                   const bool password_encrypted, std::string password)
    : path{std::move(path)}, type{type},
      password_encrypted{password_encrypted}, password{std::move(password)} {}

TestMeta::TestMeta() { m_test_files = get_test_files(); }

TestMeta &TestMeta::instance() {
  static TestMeta instance;
  return instance;
}

std::vector<std::string> TestMeta::test_file_paths() const {
  std::vector<std::string> result;
  for (auto &&file : m_test_files) {
    result.push_back(file.first);
  }
  std::sort(std::begin(result), std::end(result));
  return result;
}

TestFile TestMeta::test_file(const std::string &path) const {
  return m_test_files.at(path);
}

} // namespace odr::test
