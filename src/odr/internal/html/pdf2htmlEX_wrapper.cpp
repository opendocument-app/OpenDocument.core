#include <odr/internal/html/pdf2htmlEX_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>

#include <pdf2htmlEX.h>

namespace odr::internal {

Html html::pdf2htmlEX_wrapper(const std::string &input_path,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  pdf2htmlEX::pdf2htmlEX pdf2htmlEX;

  pdf2htmlEX.setInputFilename(input_path);
  pdf2htmlEX.setDestinationDir(output_path);
  auto output_file_name = "document.html";
  pdf2htmlEX.setOutputFilename(output_file_name);

  pdf2htmlEX.setDRM(false);
  pdf2htmlEX.setProcessOutline(false);
  pdf2htmlEX.setProcessAnnotation(true);

  // @TODO:
  //  if (options.password != null) {
  //    pdf2htmlEX.setOwnerPassword(options.password).setUserPassword(options.password);
  //  }

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
