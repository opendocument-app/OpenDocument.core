#include <internal/util/file_util.h>
#include <iostream>
#include <odr/document.h>
#include <odr/file.h>
#include <odr/html.h>
#include <string>

int main(int, char **argv) {
  const std::string input{argv[1]};
  const std::string diff_path{argv[2]};
  const std::string output{argv[3]};

  const odr::DocumentFile document_file{input};

  if (document_file.password_encrypted()) {
    std::cerr << "encrypted documents are not supported" << std::endl;
    return 1;
  }

  odr::Document document = document_file.document();

  const std::string diff = odr::util::file::read(diff_path);
  odr::Html::edit(document, "", diff);

  document.save(output);

  return 0;
}
