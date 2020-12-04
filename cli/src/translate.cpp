#include <iostream>
#include <odr/File.h>
#include <odr/Document.h>
#include <odr/Html.h>
#include <string>

int main(int argc, char **argv) {
  const std::string input{argv[1]};
  const std::string output{argv[2]};

  std::optional<std::string> password;
  if (argc >= 4)
    password = argv[3];

  odr::DocumentFile documentFile{input};

  if (documentFile.passwordEncrypted()) {
    if (password) {
      if (!documentFile.decrypt(*password)) {
        std::cerr << "wrong password" << std::endl;
        return 1;
      }
    } else {
      std::cerr << "document encrypted but no password given" << std::endl;
      return 2;
    }
  }

  auto document = documentFile.document();

  odr::HtmlConfig config;
  odr::Html::translate(document, output, config);

  return 0;
}
