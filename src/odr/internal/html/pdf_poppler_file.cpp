#include <odr/internal/html/pdf_poppler_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>
#include <odr/internal/project_info.hpp>

#include <pdf2htmlEX/HTMLRenderer/HTMLRenderer.h>
#include <pdf2htmlEX/Param.h>

#include <cstring>

namespace odr::internal {

Html html::translate_pdf_poppler_file(const PopplerPdfFile &pdf_file,
                                      const std::string &output_path,
                                      const HtmlConfig &config) {
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

  Param param;

  HTMLRenderer(nullptr, param).process(&pdf_file.pdf_doc());

  return {FileType::portable_document_format,
          config,
          {{"document", output_path + "/document.html"}}};
}

} // namespace odr::internal
