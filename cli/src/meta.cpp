#include <odr/file.hpp>

#include <odr/internal/util/odr_meta_util.hpp>

#include <odr/exceptions.hpp>

#include <iostream>
#include <string>

using namespace odr;

int main(const int argc, char **argv) {
  try {
    const std::shared_ptr logger =
        Logger::create_stdio("odr-meta", LogLevel::verbose);

    const std::string input{argv[1]};

    std::optional<std::string> password;
    if (argc >= 3) {
      password = argv[2];
    }

    DocumentFile document_file{input};

    if (document_file.password_encrypted()) {
      if (!password) {
        ODR_FATAL(*logger, "document encrypted but no password given");
        return 2;
      }
      try {
        document_file = document_file.decrypt(*password);
      } catch (const WrongPasswordError &) {
        ODR_FATAL(*logger, "wrong password");
        return 1;
      }
    }

    const auto json =
        internal::util::meta::meta_to_json(document_file.file_meta());
    std::cout << json.dump(4) << '\n';

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
