#include <odr/internal/html/image_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_to_svg.hpp>

#include <sstream>

namespace odr::internal::html {
namespace {

class HtmlServiceImpl : public HtmlService {
public:
  HtmlServiceImpl(ImageFile image_file, HtmlConfig config,
                  HtmlResourceLocator resource_locator)
      : HtmlService(std::move(config), std::move(resource_locator)),
        m_image_file{std::move(image_file)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "image", "image.html"));
  }

  void warmup() const final {}

  [[nodiscard]] const HtmlViews &list_views() const final { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const final {
    if (path == "image.html") {
      return true;
    }

    return false;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const final {
    if (path == "image.html") {
      return "text/html";
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const final {
    if (path == "image.html") {
      HtmlWriter writer(out, config());
      write_image(writer);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           html::HtmlWriter &out) const final {
    if (path == "image.html") {
      return write_image(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_image(HtmlWriter &out) const {
    HtmlResources resources;

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

      translate_image_src(m_image_file, out.out(), config());

      out.out() << "\">";
    }

    out.write_body_end();
    out.write_end();

    return resources;
  }

protected:
  ImageFile m_image_file;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

void html::translate_image_src(const File &file, std::ostream &out,
                               const HtmlConfig &config) {
  try {
    translate_image_src(DecodedFile(file).image_file(), out, config);
  } catch (...) {
    // TODO hacky - `image/jpg` works for all common image types in chrome
    // TODO use stream
    out << file_to_url(*file.stream(), "image/jpg");
  }
}

void html::translate_image_src(const ImageFile &image_file, std::ostream &out,
                               const HtmlConfig & /*config*/) {
  // try svm
  try {
    // TODO `image_file` is already an `SvmFile`
    // TODO `impl()` might be a bit dirty
    auto image_file_impl = image_file.file().impl();
    // TODO memory file might not be necessary; other istreams didn't support
    // `tellg`
    svm::SvmFile svm_file(
        std::make_shared<common::MemoryFile>(*image_file_impl));
    std::ostringstream svg_out;
    svm::Translator::svg(svm_file, svg_out);
    // TODO use stream
    out << file_to_url(svg_out.str(), "image/svg+xml");
  } catch (...) {
    // else we guess that it is a usual image
    // TODO hacky - `image/jpg` works for all common image types in chrome
    // TODO use stream
    out << file_to_url(*image_file.stream(), "image/jpg");
  }
}

odr::HtmlService html::create_image_service(const ImageFile &image_file,
                                            const std::string &output_path,
                                            const HtmlConfig &config) {
  HtmlResourceLocator resource_locator =
      local_resource_locator(output_path, config);

  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(image_file, config, resource_locator));
}

} // namespace odr::internal
