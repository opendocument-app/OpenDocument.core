#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/text/text_file.hpp>

#include <memory>

namespace odr::internal::markdown {

/// Markdown is a document, not a text file: it decodes to a `TextRoot`, so
/// `FileCategory::document` and `DocumentType::text`. Nothing here rejects —
/// any UTF-8 byte sequence is some markdown — so there is no `NoMarkdownFile`;
/// only an undecodable encoding stops it. See `AGENTS.md`.
class MarkdownFile final : public abstract::DocumentFile {
public:
  explicit MarkdownFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] DocumentType document_type() const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  /// @throws UnsupportedTextEncoding if the encoding cannot be decoded.
  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

  /// The encoding the bytes were detected as.
  [[nodiscard]] TextEncoding encoding() const noexcept;

private:
  std::shared_ptr<text::TextFile> m_file;
};

} // namespace odr::internal::markdown
