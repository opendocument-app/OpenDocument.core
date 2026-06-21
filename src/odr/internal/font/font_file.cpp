#include <odr/internal/font/font_file.hpp>

#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/sfnt_font.hpp>

#include <utility>

namespace odr::internal::font {

FontFile::FontFile(std::shared_ptr<abstract::File> file,
                   const FileType file_type)
    : m_file{std::move(file)}, m_file_type{file_type} {
  // Parse eagerly: a parse failure is how detection rejects a non-font, so the
  // open-strategy try/catch can fall through.
  m_program = std::make_shared<sfnt::SfntFont>(m_file->stream());
}

std::shared_ptr<abstract::File> FontFile::file() const noexcept {
  return m_file;
}

DecoderEngine FontFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

FileType FontFile::file_type() const noexcept { return m_file_type; }

std::string_view FontFile::mimetype() const noexcept {
  return m_file_type == FileType::opentype_font ? "font/otf" : "font/ttf";
}

FileMeta FontFile::file_meta() const noexcept {
  return {file_type(), mimetype(), false, std::nullopt};
}

bool FontFile::is_decodable() const noexcept { return true; }

std::shared_ptr<abstract::Font> FontFile::font_program() const {
  return m_program;
}

} // namespace odr::internal::font
