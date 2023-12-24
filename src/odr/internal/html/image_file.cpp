#include <odr/internal/html/image_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_to_svg.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <fstream>
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
  std::ofstream ostream(output_path);
  if (!ostream.is_open()) {
    throw FileWriteError();
  }
  HtmlWriter out(ostream, config.format_html, config.html_indent);

  out.write_begin();
  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_target("_blank");
  out.write_header_title("odr");
  out.write_header_viewport(
      "width=device-width,initial-scale=1.0,user-scalable=yes");
  out.write_header_end();

  out.write_body_begin();

  {
    out.write_new_line();
    out.out() << "<img";
    out.out() << " alt=\"Error: image not found or unsupported\"";
    out.out() << " src=\"";

    translate_image_src(image_file, out.out(), config);

    out.out() << "\">";
  }

  out.write_body_end();
  out.write_end();

  return {image_file.file_type(), config, {{"image", output_path}}};
}

} // namespace odr::internal
