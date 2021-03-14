#include <internal/util/file_util.h>
#include <iostream>
#include <odr/document.h>
#include <odr/html_config.h>
#include <string>

using namespace odr;

int main(int, char **argv) {
  const std::string input{argv[1]};
  const std::string diff_path{argv[2]};
  const std::string output{argv[3]};

  HtmlConfig config;
  config.entry_offset = 0;
  config.entry_count = 0;
  config.editable = true;

  const Document document{input};

  if (document.encrypted()) {
    std::cerr << "encrypted documents are not supported" << std::endl;
    return 1;
  }

  document.translate(output, config);

  const std::string back_diff = internal::util::file::read(diff_path);
  document.edit(back_diff);
  document.save(output);

  return 0;
}
