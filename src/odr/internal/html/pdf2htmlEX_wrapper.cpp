#include <odr/internal/html/pdf2htmlEX_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>


#include <fstream>

#include <pdf2htmlEX.h>

namespace odr::internal {

Html html::pdf2htmlEX_wrapper(const PdfFile &pdf_file,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  pdf2htmlEX::pdf2htmlEX pdf2htmlEX;

  auto disk_path = pdf_file.file().disk_path();
  if (!disk_path.has_value()) {
    throw FileNotFound();
  }
  pdf2htmlEX.setInputFilename(disk_path.value());
  pdf2htmlEX.setDestinationDir(output_path);
  auto output_file_name = "document.html";
  pdf2htmlEX.setOutputFilename(output_file_name);

  try {
    pdf2htmlEX.convert();
  } catch (const pdf2htmlEX::EncryptionPasswordException & e) {
    throw WrongPassword();
  } catch (const pdf2htmlEX::DocumentCopyProtectedException & e) {
    throw std::runtime_error("document is copy protected");
  } catch (const pdf2htmlEX::ConversionFailedException & e) {
    throw std::runtime_error(std::string("conversion error ") + e.what());
  }

  return {FileType::portable_document_format,
          config,
          {{"document", output_path + "/" + output_file_name}}};
}

} // namespace odr::internal
