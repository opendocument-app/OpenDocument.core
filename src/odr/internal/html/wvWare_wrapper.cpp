#include <odr/internal/html/wvWare_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/project_info.hpp>

#include <wv/wv.h>

// TODO remove this
#include <unistd.h>

namespace odr::internal::html {

Html wvWare_wrapper(const std::string &input_path,
                    const std::string &output_path, const HtmlConfig &config,
                    std::optional<std::string> &password) {
  if (nullptr == g_wvDataDir) {
    g_wvDataDir = WVDATADIR;
  }

  auto output_file_path = output_path + "/document.html";

  char *input_file_path = strdup(input_path.c_str());
  char *output_dir = strdup(output_path.c_str());

  g_htmlOutputFileHandle = fopen(output_file_path.c_str(), "w");

  std::string pw;
  if (password.has_value()) {
    pw = password.value();
  }
  int retVal = wvHtml_convert(input_file_path, output_dir, pw.c_str());
  free(output_dir);
  free(input_file_path);
  fclose(g_htmlOutputFileHandle);
  g_htmlOutputFileHandle = nullptr;

  if (0 != retVal) {
    unlink(output_file_path.c_str());

    switch (retVal) {
    case 100: // PasswordRequired
    case 101: // Wrong Password
      throw WrongPassword();
    default:
      throw std::runtime_error("Conversion error");
    }
  }

  return {
      FileType::legacy_word_document, config, {{"document", output_file_path}}};
}

} // namespace odr::internal::html
