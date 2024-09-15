#include <odr/internal/html/pdf2htmlEX_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/project_info.hpp>

#include <pdf2htmlEX.h>

#include <cstring>

namespace odr::internal {

Html html::pdf2htmlEX_wrapper(const std::string &input_path,
                              const std::string &output_path,
                              const HtmlConfig &config,
                              std::optional<std::string> &password) {
  static const char *fontconfig_path = getenv("FONTCONFIG_PATH");
  if (nullptr == fontconfig_path) {
    // Storage is allocated and after successful putenv, it will never be freed.
    // This is the way of putenv.
    char *storage = strdup("FONTCONFIG_PATH=" FONTCONFIG_PATH);
    if (0 != putenv(storage)) {
      free(storage);
    }
    fontconfig_path = getenv("FONTCONFIG_PATH");
  }

  pdf2htmlEX::pdf2htmlEX pdf2htmlEX;
  pdf2htmlEX.setDataDir(PDF2HTMLEX_DATA_DIR);
  pdf2htmlEX.setPopplerDataDir(POPPLER_DATA_DIR);

  pdf2htmlEX.setInputFilename(input_path);
  pdf2htmlEX.setDestinationDir(output_path);
  auto output_file_name = "document.html";
  pdf2htmlEX.setOutputFilename(output_file_name);

  pdf2htmlEX.setDRM(false);
  pdf2htmlEX.setProcessOutline(false);
  pdf2htmlEX.setProcessAnnotation(true);

  if (password.has_value()) {
    pdf2htmlEX.setOwnerPassword(password.value());
    pdf2htmlEX.setUserPassword(password.value());
  }

  try {
    pdf2htmlEX.convert();
  } catch (const pdf2htmlEX::EncryptionPasswordException &e) {
    throw WrongPassword();
  } catch (const pdf2htmlEX::DocumentCopyProtectedException &e) {
    throw std::runtime_error("document is copy protected");
  } catch (const pdf2htmlEX::ConversionFailedException &e) {
    throw std::runtime_error(std::string("conversion error ") + e.what());
  }

  return {FileType::portable_document_format,
          config,
          {{"document", output_path + "/" + output_file_name}}};
}

} // namespace odr::internal
