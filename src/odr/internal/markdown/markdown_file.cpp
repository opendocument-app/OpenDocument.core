#include <odr/internal/markdown/markdown_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/markdown/markdown_document.hpp>

#include <string>
#include <utility>

namespace odr::internal::markdown {

MarkdownFile::MarkdownFile(std::shared_ptr<text::TextFile> file)
    : m_file{std::move(file)} {}

std::shared_ptr<abstract::File> MarkdownFile::file() const noexcept {
  return m_file->file();
}

FileType MarkdownFile::file_type() const noexcept { return FileType::markdown; }

std::string_view MarkdownFile::mimetype() const noexcept {
  return "text/markdown";
}

FileMeta MarkdownFile::file_meta() const noexcept {
  FileMeta result;
  result.type = file_type();
  result.mimetype = mimetype();
  result.document_type = DocumentType::text;
  return result;
}

bool MarkdownFile::is_decodable() const noexcept {
  return text_encoding_is_decodable(encoding());
}

std::shared_ptr<abstract::Document> MarkdownFile::document() const {
  // `Text::content()` is UTF-8 to every binding, so bytes we cannot decode
  // have no document at all — the text rendering path stays open to them.
  if (!is_decodable()) {
    throw UnsupportedTextEncoding(encoding());
  }
  const std::string text = m_file->text();
  return std::make_shared<Document>(text);
}

TextEncoding MarkdownFile::encoding() const noexcept {
  return m_file->encoding();
}

} // namespace odr::internal::markdown
