#pragma once

#include <odr/file.hpp>
#include <odr/internal/abstract/file.hpp>

#include <memory>

namespace odr::internal::abstract {
class Font;
}

namespace odr::internal::font {

/// A standalone font file (`.ttf`/`.otf`) as a `DecodedFile`
class FontFile final : public abstract::FontFile {
public:
  FontFile(std::shared_ptr<abstract::File> file, FileType file_type);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;
  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Font> font() const override;

private:
  std::shared_ptr<abstract::File> m_file;
  FileType m_file_type;
  std::shared_ptr<abstract::Font> m_font;
};

} // namespace odr::internal::font
