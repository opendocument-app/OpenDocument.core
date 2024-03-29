#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/util/file_util.hpp>

#include <iostream>
#include <string>

using namespace odr;

int main(int, char **argv) {
  const std::string input{argv[1]};
  const std::string diff_path{argv[2]};
  const std::string output{argv[3]};

  const DocumentFile document_file{input};

  if (document_file.password_encrypted()) {
    std::cerr << "encrypted documents are not supported" << std::endl;
    return 1;
  }

  Document document = document_file.document();

  const std::string diff = odr::internal::util::file::read(diff_path);
  html::edit(document, diff.c_str());

  document.save(output);

  return 0;
}
