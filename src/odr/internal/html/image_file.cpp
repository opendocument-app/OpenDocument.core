#include <odr/internal/html/image_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_to_svg.hpp>

#include <span>
#include <sstream>
#include <string_view>

namespace odr::internal::html {
namespace {

/// A starview metafile is converted to svg; anything else goes out as its own
/// bytes labelled @p mime_type.
void write_image_src(const ImageFile &image_file, std::ostream &out,
                     const std::string &mime_type) {
  // try svm
  try {
    // TODO `image_file` is already an `SvmFile`
    // TODO `impl()` might be a bit dirty
    const std::shared_ptr<abstract::File> image_file_impl =
        image_file.file().impl();
    // TODO memory file might not be necessary; other istreams didn't support
    // `tellg`
    const svm::SvmFile svm_file(std::make_shared<MemoryFile>(*image_file_impl));
    std::ostringstream svg_out;
    svm::Translator::svg(svm_file, svg_out);
    // TODO use stream
    out << file_to_url(svg_out.str(), "image/svg+xml");
  } catch (...) {
    // else it is a usual image and goes out as it came in
    // TODO use stream
    out << file_to_url(*image_file.stream(), mime_type);
  }
}

/// An image file knows exactly which format it is holding, so the data url
/// says so. The table's first mime type is the canonical one.
std::string image_mime_type(const ImageFile &image_file) {
  const std::span<const std::string_view> mimetypes =
      mimetypes_by_file_type(image_file.file_type());
  return mimetypes.empty() ? "image/jpg" : std::string(mimetypes.front());
}

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(ImageFile image_file, HtmlConfig config, const Logger &logger)
      : HtmlService(std::move(config), logger),
        m_image_file{std::move(image_file)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "image", 0, "image.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    return path == "image.html";
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "image.html") {
      return "text/html";
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "image.html") {
      HtmlWriter writer(out, config());
      write_image(writer);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
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
    write_viewport_meta(out, config(), true);
    out.write_header_end();

    out.write_body_begin();

    {
      out.write_new_line();
      out.out() << "<img";
      out.out() << " alt=\"Error: image not found or unsupported\"";
      out.out() << " src=\"";

      write_image_src(m_image_file, out.out(), image_mime_type(m_image_file));

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
    translate_image_src(DecodedFile(file).as_image_file(), out, config);
  } catch (...) {
    // nothing named it, so the label is a guess - browsers sniff `<img>` and
    // `image/jpg` is what they have been handed here for years
    // TODO use stream
    out << file_to_url(*file.stream(), "image/jpg");
  }
}

void html::translate_image_src(const ImageFile &image_file, std::ostream &out,
                               const HtmlConfig & /*config*/) {
  write_image_src(image_file, out, image_mime_type(image_file));
}

HtmlService html::create_image_service(const ImageFile &image_file,
                                       HtmlConfig config,
                                       const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(image_file, std::move(config), logger));
}

} // namespace odr::internal
