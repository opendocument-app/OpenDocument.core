#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <iostream>
#include <string>

using namespace odr;

int main(int argc, char **argv) {
  std::string input{argv[1]};
  std::string output{argv[2]};

  std::optional<std::string> password;
  if (argc >= 4) {
    password = argv[3];
  }

  File file{input};

  HtmlConfig config;
  config.editable = true;

  PasswordCallback password_callback = [&]() {
    if (!password) {
      std::cerr << "document encrypted but no password given" << std::endl;
      std::exit(2);
    }
    return *password;
  };

  try {
    html::translate(file, output, config, password_callback);
  } catch (const WrongPassword &e) {
    std::cerr << "wrong password" << std::endl;
    return 1;
  }

  return 0;
}
