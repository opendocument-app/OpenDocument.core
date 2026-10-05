#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/util/file_util.hpp>

#include <iostream>
#include <string>

using namespace odr;

int main(const int argc, char **argv) {
  if (argc < 4) {
    std::cerr << "usage: back_translate <input> <diff> <output>\n";
    return 2;
  }

  try {
    const Logger logger =
        Logger::create_stdio("odr-back-translate", LogLevel::verbose);

    const std::string input{argv[1]};
    const std::string diff_path{argv[2]};
    const std::string output{argv[3]};

    const DecodedFile file = open(input);

    if (file.password_encrypted()) {
      ODR_FATAL(logger, "encrypted documents are not supported");
      return 1;
    }

    const std::string diff = internal::util::file::read(diff_path);
    if (file.is_text_file()) {
      const TextFile text = file.as_text_file();
      text.edit(diff, logger);
      text.save(output);
    } else {
      const Document document = file.is_csv_file()
                                    ? file.as_csv_file().document()
                                    : file.as_document_file().document();
      document.edit(diff, logger);
      document.save(output);
    }

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
