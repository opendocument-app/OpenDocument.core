#include <odr/internal/html/pdf2htmlEX_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>

#include <pdf2htmlEX.h>

#include <odr/internal/project_info.hpp>

namespace odr::internal {

static void ensure_env_vars() {
  static const char *pdf2htmlEX_data_dir = getenv("PDF2HTMLEX_DATA_DIR");
  if (nullptr == pdf2htmlEX_data_dir) {
    pdf2htmlEX_data_dir = PDF2HTMLEX_DATA_DIR;
    setenv("PDF2HTMLEX_DATA_DIR", pdf2htmlEX_data_dir, 0);
    std::cout << "PDF2HTMLEX_DATA_DIR set to " << getenv("PDF2HTMLEX_DATA_DIR")
              << std::endl;
  }

  static const char *poppler_data_dir = getenv("POPPLER_DATA_DIR");
  if (nullptr == poppler_data_dir) {
    poppler_data_dir = POPPLER_DATA_DIR;
    setenv("POPPLER_DATA_DIR", poppler_data_dir, 0);
    std::cout << "POPPLER_DATA_DIR set to " << getenv("POPPLER_DATA_DIR")
              << std::endl;
  }

  static const char *fontconfig_path = getenv("FONTCONFIG_PATH");
  if (nullptr == fontconfig_path) {
    fontconfig_path = FONTCONFIG_PATH;
    setenv("FONTCONFIG_PATH", fontconfig_path, 0);
    std::cout << "FONTCONFIG_PATH set to " << getenv("FONTCONFIG_PATH")
              << std::endl;
  }
}

Html html::pdf2htmlEX_wrapper(const std::string &input_path,
                              const std::string &output_path,
                              const HtmlConfig &config,
                              std::optional<std::string> &password) {
  ensure_env_vars();

  pdf2htmlEX::pdf2htmlEX pdf2htmlEX;

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
