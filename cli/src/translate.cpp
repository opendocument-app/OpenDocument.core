#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace odr;

int main(const int argc, char **argv) {
  const std::string input{argv[1]};
  const std::string output{argv[2]};

  std::optional<std::string> password;
  if (argc >= 4) {
    password = argv[3];
  }

  DecodedFile decoded_file{input};

  if (decoded_file.password_encrypted() && !password) {
    std::cerr << "document encrypted but no password given" << std::endl;
    return 2;
  }
  if (decoded_file.password_encrypted()) {
    try {
      decoded_file = decoded_file.decrypt(*password);
    } catch (const WrongPasswordError &) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  HtmlConfig config;
  config.editable = true;

  const std::string output_tmp = output + "/tmp";
  std::filesystem::create_directories(output_tmp);
  const HtmlService service = html::translate(decoded_file, output, config);
  Html html = service.bring_offline(output);
  std::filesystem::remove_all(output_tmp);

  return 0;
}
