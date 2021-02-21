#include <iostream>
#include <nlohmann/json.hpp>
#include <odr/file.h>
#include <string>

namespace {
nlohmann::json meta_to_json(const odr::FileMeta &meta) {
  nlohmann::json result{
      {"type", meta.type_as_string()},
      {"encrypted", meta.password_encrypted},
      {"entryCount", meta.document_meta->entry_count},
      {"entries", nlohmann::json::array()},
  };

  if (!meta.document_meta->entries.empty()) {
    for (auto &&e : meta.document_meta->entries) {
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

  bool has_password = argc >= 4;
  std::string password;
  if (has_password) {
    password = argv[2];
  }

  odr::DocumentFile document_file{input};

  if (document_file.password_encrypted() && has_password) {
    if (!document_file.decrypt(password)) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  const auto json = meta_to_json(document_file.file_meta());
  std::cout << json.dump(4) << std::endl;

  return 0;
}
