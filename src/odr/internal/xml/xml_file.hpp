#pragma once

#include <odr/file.hpp>

#include <odr/internal/text/text_file.hpp>

#include <memory>
#include <string>

namespace pugi {
class xml_document;
} // namespace pugi

namespace odr::internal::xml {

/// Parses @p text, which has to be utf-8, keeping what a source view has to
/// show: the declaration, the doctype, processing instructions, comments, and
/// whitespace-only text where it is an element's only child.
/// @throws NoXmlFile if @p text is not a well formed xml document.
[[nodiscard]] pugi::xml_document parse_source(const std::string &text);

/// An xml file. Nothing is decoded beyond the parse that recognises it: it
/// renders as a source view, not as a document.
class XmlFile final : public abstract::TextFile {
public:
  /// @throws NoXmlFile if @p file is not a well formed xml document.
  /// @throws UnsupportedTextEncoding if its encoding cannot be decoded.
  explicit XmlFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  /// The declaration's `encoding` where it names one we know, else what the
  /// bytes were detected as.
  [[nodiscard]] TextEncoding encoding() const noexcept override;

  /// The file's bytes decoded to utf-8.
  /// @throws UnsupportedTextEncoding if @ref encoding cannot be decoded.
  [[nodiscard]] std::string text() const;

private:
  std::shared_ptr<text::TextFile> m_file;
  TextEncoding m_encoding{TextEncoding::unknown};
};

} // namespace odr::internal::xml
