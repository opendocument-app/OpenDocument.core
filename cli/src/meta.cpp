#include <iostream>
#include <nlohmann/json.hpp>
#include <odr/file.h>
#include <string>

namespace {
nlohmann::json meta_to_json(const odr::FileMeta &meta) {
  nlohmann::json result{
      {"type", meta.typeAsString()},
      {"encrypted", meta.passwordEncrypted},
      {"entryCount", meta.documentMeta->entry_count},
      {"entries", nlohmann::json::array()},
  };

  if (!meta.documentMeta->entries.empty()) {
    for (auto &&e : meta.documentMeta->entries) {
      result["entries"].push_back({
          {"name", e.name},
          {"rowCount", e.row_count},
          {"columnCount", e.column_count},
          {"notes", e.notes},
      });
    }
  }

  return result;
}
} // namespace

int main(int argc, char **argv) {
  const std::string input{argv[1]};

  bool hasPassword = argc >= 4;
  std::string password;
  if (hasPassword)
    password = argv[2];

  odr::DocumentFile documentFile{input};

  if (documentFile.passwordEncrypted() && hasPassword) {
    if (!documentFile.decrypt(password)) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  const auto json = meta_to_json(documentFile.fileMeta());
  std::cout << json.dump(4) << std::endl;

  return 0;
}
