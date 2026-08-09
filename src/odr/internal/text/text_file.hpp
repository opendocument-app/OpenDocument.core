#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>

namespace odr::internal::text {

class TextFile final : public abstract::TextFile {
public:
  explicit TextFile(std::shared_ptr<abstract::File> file);
  TextFile(std::shared_ptr<abstract::File> file, TextEncoding encoding);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] TextEncoding encoding() const noexcept override;

  /// The file's bytes decoded to UTF-8.
  /// @throws UnsupportedTextEncoding if the encoding cannot be decoded.
  [[nodiscard]] std::string text() const;

private:
  std::shared_ptr<abstract::File> m_file;
  TextEncoding m_encoding{TextEncoding::unknown};
};

} // namespace odr::internal::text
