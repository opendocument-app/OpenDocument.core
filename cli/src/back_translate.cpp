#include <iostream>
#include <odr/document.h>
#include <odr/file.h>
#include <odr/html.h>
#include <string>
#include <util/file_util.h>

int main(int, char **argv) {
  const std::string input{argv[1]};
  const std::string diffPath{argv[2]};
  const std::string output{argv[3]};

  const odr::DocumentFile documentFile{input};

  if (documentFile.passwordEncrypted()) {
    std::cerr << "encrypted documents are not supported" << std::endl;
    return 1;
  }

  odr::Document document = documentFile.document();

  const std::string diff = odr::util::file::read(diffPath);
  odr::Html::edit(document, "", diff);

  document.save(output);

  return 0;
}
