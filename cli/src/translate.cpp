#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace odr;

int main(const int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: translate <input> <output> [password]\n";
    return 2;
  }

  try {
    const Logger logger =
        Logger::create_stdio("odr-translate", LogLevel::verbose);

    const std::string input{argv[1]};
    const std::string output{argv[2]};

    std::optional<std::string> password;
    if (argc >= 4) {
      password = argv[3];
    }

    DecodedFile decoded_file{input};

    if (decoded_file.password_encrypted()) {
      if (!password) {
        ODR_FATAL(logger, "document encrypted but no password given");
        return 2;
      }
      try {
        decoded_file = decoded_file.decrypt(*password);
      } catch (const WrongPasswordError &) {
        ODR_FATAL(logger, "wrong password");
        return 1;
      }
    }

    HtmlConfig config;
    config.editable = true;
    config.format_html = true;

    std::filesystem::create_directories(output);
    const HtmlService service = html::translate(decoded_file, output, config);
    const Html html = service.bring_offline(output);

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
