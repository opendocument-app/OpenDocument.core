#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>

#include <csv.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <test_util.hpp>
#include <unordered_map>
#include <utility>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::common;
namespace fs = std::filesystem;

namespace odr::test {

namespace {

TestFile get_test_file(const std::string &root_path,
                       std::string absolute_path) {
  const FileType type = odr::get_file_type_by_file_extension(
      common::Path(absolute_path).extension());

  std::string short_path = absolute_path.substr(root_path.size() + 1);

  std::optional<std::string> password;
  const std::string filename = fs::path(absolute_path).filename().string();
  if (const auto left = filename.find('$'), right = filename.rfind('$');
      (left != std::string::npos) && (left != right)) {
    password = filename.substr(left, right);
  }

  return {std::move(absolute_path), std::move(short_path), type,
          std::move(password)};
}

std::vector<TestFile> get_test_files(const std::string &root_path,
                                     const std::string &input_path) {
  if (fs::is_regular_file(input_path)) {
    return {get_test_file(root_path, input_path)};
  }
  if (!fs::is_directory(input_path)) {
    return {};
  }

  std::vector<TestFile> result;

  const std::string index_path = input_path + "/index.csv";
  if (fs::is_regular_file(index_path)) {
    for (const auto &row : csv::CSVReader(index_path)) {
      std::string absolute_path = input_path + "/" + row["path"].get<>();
      std::string short_path = absolute_path.substr(root_path.size() + 1);
      FileType type = odr::get_file_type_by_file_extension(row["type"].get<>());
      std::optional<std::string> password = row["encrypted"].get<>() == "yes"
                                                ? row["password"].get<>()
                                                : std::optional<std::string>();

      if (type == FileType::unknown) {
        continue;
      }
      result.emplace_back(std::move(absolute_path), std::move(short_path), type,
                          std::move(password));
    }
  }

  for (auto &&p : fs::recursive_directory_iterator(input_path)) {
    if (!p.is_regular_file()) {
      continue;
    }
    const std::string path = p.path().string();
    if (path == index_path) {
      continue;
    }
    if (p.path().filename().string().starts_with(".")) {
      continue;
    }
    if (const auto it = std::find_if(
            std::begin(result), std::end(result),
            [&](auto &&file) { return file.absolute_path == path; });
        it != std::end(result)) {
      continue;
    }

    const auto file = get_test_file(root_path, path);

    if (file.type == FileType::unknown) {
      continue;
    }
    result.push_back(file);
  }

  return result;
}

std::vector<TestFile> get_test_files() {
  std::vector<TestFile> result;

  std::string root = TestData::data_input_directory();

  for (const auto &e : fs::directory_iterator(root)) {
    const auto files = get_test_files(root, e.path().string());
    result.insert(std::end(result), std::begin(files), std::end(files));
  }

  std::sort(std::begin(result), std::end(result),
            [](const auto &lhs, const auto &rhs) {
              return lhs.short_path < rhs.short_path;
            });

  return result;
}

} // namespace

TestFile::TestFile(std::string absolute_path, std::string short_path,
                   const FileType type, std::optional<std::string> password)
    : absolute_path{std::move(absolute_path)},
      short_path{std::move(short_path)}, type{type},
      password{std::move(password)} {}

std::string TestData::data_input_directory() {
  return common::Path(TestData::data_directory()).join(Path("input")).string();
}

TestData &TestData::instance_() {
  static TestData instance;
  return instance;
}

std::vector<TestFile> TestData::test_files() {
  return instance_().m_test_files;
}

std::vector<TestFile> TestData::test_files(FileType fileType) {
  return instance_().test_files_(fileType);
}

TestFile TestData::test_file(const std::string &short_path) {
  const auto &files = instance_().m_test_files;
  const auto it =
      std::find_if(std::begin(files), std::end(files), [&](const auto &file) {
        return file.short_path == short_path;
      });
  if (it == std::end(files)) {
    throw std::runtime_error("Test file not found: " + short_path);
  }
  return *it;
}

std::string TestData::test_file_path(const std::string &short_path) {
  return test_file(short_path).absolute_path;
}

TestData::TestData() : m_test_files{get_test_files()} {}

std::vector<TestFile> TestData::test_files_(const FileType fileType) const {
  std::vector<TestFile> result;
  result.reserve(m_test_files.size());
  for (auto &&file : m_test_files) {
    if (file.type == fileType) {
      result.push_back(file);
    }
  }
  return result;
}

} // namespace odr::test
