#include <fstream>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>
#include <odr/internal/common/file.h>
#include <odr/internal/crypto/crypto_util.h>
#include <odr/internal/html/common.h>
#include <odr/internal/html/image_file.h>
#include <odr/internal/svm/svm_file.h>
#include <odr/internal/svm/svm_to_svg.h>
#include <odr/internal/util/stream_util.h>
#include <sstream>

namespace odr::internal {

void html::translate_image_src(const File &file, std::ostream &out,
                               const HtmlConfig &config) {
  try {
    translate_image_src(DecodedFile(file).image_file(), out, config);
  } catch (...) {
    // TODO use stream
    std::string image_data = util::stream::read(*file.stream());
    // TODO hacky - `image/jpg` works for all common image types in chrome
    out << "data:image/jpg;base64, ";
    // TODO stream
    out << crypto::util::base64_encode(image_data);
  }
}

void html::translate_image_src(const ImageFile &image_file, std::ostream &out,
                               const HtmlConfig & /*config*/) {
  // TODO use stream
  std::string image_data;

  try {
    // try svm
    // TODO `image_file` is already an `SvmFile`
    // TODO `impl()` might be a bit dirty
    auto image_file_impl = image_file.file().impl();
    // TODO memory file might not be necessary; other istreams didn't support
    // `tellg`
    svm::SvmFile svm_file(
        std::make_shared<common::MemoryFile>(*image_file_impl));
    std::ostringstream svg_out;
    svm::Translator::svg(svm_file, svg_out);
    image_data = svg_out.str();
    out << "data:image/svg+xml;base64, ";
  } catch (...) {
    // else we guess that it is a usual image
    image_data = util::stream::read(*image_file.stream());
    // TODO hacky - `image/jpg` works for all common image types in chrome
    out << "data:image/jpg;base64, ";
  }

  // TODO stream
  out << crypto::util::base64_encode(image_data);
}

Html html::translate_image_file(const ImageFile &image_file,
                                const std::string &path,
                                const HtmlConfig &config) {
  auto output_path = path + "/image.html";
  std::ofstream out(output_path);
  if (!out.is_open()) {
    throw FileWriteError();
  }

  out << internal::html::doctype();
  out << "<html><head>";
  out << internal::html::default_headers();
  out << "<style>";
  // TODO style
  out << "</style>";
  out << "</head>";

  out << "<body " << internal::html::body_attributes(config) << ">";

  {
    out << "<img";
    out << " alt=\"Error: image not found or unsupported\"";
    out << " src=\"";

    translate_image_src(image_file, out, config);

    out << "\">";
  }

  out << "</body>";
  out << "</html>";

  return {image_file.file_type(), config, {{"image", output_path}}};
}

} // namespace odr::internal
