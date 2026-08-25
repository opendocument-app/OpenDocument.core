#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/text/text_file.hpp>

#include <memory>

namespace odr::internal::markdown {

class MarkdownFile final : public abstract::MarkdownFile {
public:
  explicit MarkdownFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  /// @throws UnsupportedTextEncoding if the encoding cannot be decoded.
  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

  [[nodiscard]] TextEncoding encoding() const noexcept override;

private:
  std::shared_ptr<text::TextFile> m_file;
};

} // namespace odr::internal::markdown
