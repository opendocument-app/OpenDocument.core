#pragma once

#include <odr/file.hpp>

#include <odr/internal/text/text_file.hpp>

#include <memory>
#include <string>

namespace pugi {
class xml_document;
} // namespace pugi

namespace odr::internal::xml {

/// An xml file. Nothing is decoded beyond the parse that recognises it: it
/// renders as a source view, not as a document.
class XmlFile final : public abstract::TextFile {
public:
  /// @throws NoXmlFile if @p file is not a well formed xml document.
  /// @throws UnsupportedTextEncoding if its encoding cannot be decoded.
  explicit XmlFile(std::shared_ptr<text::TextFile> file);
  ~XmlFile() override;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  /// The declaration's `encoding` where it names one we know, else what the
  /// bytes were detected as.
  [[nodiscard]] TextEncoding encoding() const noexcept override;

  /// The text file the bytes came from. Its `text()` decodes with the encoding
  /// detected over the bytes; @ref text uses the one the declaration names.
  [[nodiscard]] std::shared_ptr<text::TextFile> text_file() const noexcept;

  /// The file's bytes decoded to utf-8.
  /// @throws UnsupportedTextEncoding if @ref encoding cannot be decoded.
  [[nodiscard]] std::string text() const;

  /// Cached source DOM, including declarations, comments and significant
  /// whitespace.
  [[nodiscard]] const pugi::xml_document &document() const noexcept;

  /// The document element's name, prefix and all, read off @ref document so
  /// telling a dialect apart costs no parse of its own.
  [[nodiscard]] std::string_view root_name() const noexcept;

private:
  std::shared_ptr<text::TextFile> m_file;
  TextEncoding m_encoding{TextEncoding::unknown};
  std::unique_ptr<pugi::xml_document> m_document;
};

} // namespace odr::internal::xml
