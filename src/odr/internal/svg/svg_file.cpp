#include <odr/internal/svg/svg_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/svg/svg_util.hpp>

#include <utility>

namespace odr::internal {

svg::SvgFile::SvgFile(std::shared_ptr<xml::XmlFile> file)
    : m_file{std::move(file)} {
  check_svg_file(*m_file);
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

std::string svg::SvgFile::text() const { return m_file->text(); }

} // namespace odr::internal
