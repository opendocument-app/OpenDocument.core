#include <odr/internal/svg/svg_file.hpp>

#include <odr/exceptions.hpp>

#include <string_view>
#include <utility>

namespace odr::internal {

namespace {

/// pugixml does not process namespaces, so the prefix comes off by hand.
std::string_view local_name(std::string_view name) {
  if (const std::size_t colon = name.find(':');
      colon != std::string_view::npos) {
    name.remove_prefix(colon + 1);
  }
  return name;
}

} // namespace

bool svg::is_svg_file(const xml::XmlFile &file) {
  return local_name(file.root_name()) == "svg";
}

svg::SvgFile::SvgFile(std::shared_ptr<xml::XmlFile> file)
    : m_file{std::move(file)} {
  if (!is_svg_file(*m_file)) {
    throw NoSvgFile();
  }
}

std::shared_ptr<abstract::File> svg::SvgFile::file() const noexcept {
  return m_file->file();
}

FileType svg::SvgFile::file_type() const noexcept {
  return FileType::scalable_vector_graphics;
}

std::string_view svg::SvgFile::mimetype() const noexcept {
  return "image/svg+xml";
}

FileMeta svg::SvgFile::file_meta() const noexcept {
  FileMeta result;
  result.type = file_type();
  result.mimetype = mimetype();
  return result;
}

bool svg::SvgFile::is_decodable() const noexcept { return false; }

std::shared_ptr<abstract::Image> svg::SvgFile::image() const {
  throw UnsupportedFileEncoding("generally unsupported");
}

std::shared_ptr<xml::XmlFile> svg::SvgFile::xml_file() const noexcept {
  return m_file;
}

std::shared_ptr<text::TextFile> svg::SvgFile::text_file() const noexcept {
  return m_file->text_file();
}

const pugi::xml_document &svg::SvgFile::document() const noexcept {
  return m_file->document();
}

std::string svg::SvgFile::text() const { return m_file->text(); }

} // namespace odr::internal
