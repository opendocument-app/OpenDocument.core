#include <internal/util/odr_meta_util.h>
#include <iostream>
#include <odr/file.h>
#include <string>

using namespace odr;

int main(int argc, char **argv) {
  const std::string input{argv[1]};

  bool has_password = argc >= 4;
  std::string password;
  if (has_password) {
    password = argv[2];
  }

  DocumentFile document_file{input};

  if (document_file.password_encrypted() && has_password) {
    if (!document_file.decrypt(password)) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  const auto json =
      odr::internal::util::meta::meta_to_json(document_file.file_meta());
  std::cout << json.dump(4) << std::endl;

  return 0;
}
