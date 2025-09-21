#include <odr/file.hpp>

#include <odr/internal/util/odr_meta_util.hpp>

#include <odr/exceptions.hpp>

#include <iostream>
#include <string>

using namespace odr;

int main(const int argc, char **argv) {
  const std::string input{argv[1]};

  const bool has_password = argc >= 4;
  std::string password;
  if (has_password) {
    password = argv[2];
  }

  DocumentFile document_file{input};

  if (document_file.password_encrypted() && has_password) {
    try {
      document_file = document_file.decrypt(password);
    } catch (const WrongPasswordError &) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  const auto json =
      internal::util::meta::meta_to_json(document_file.file_meta());
  std::cout << json.dump(4) << std::endl;

  return 0;
}
