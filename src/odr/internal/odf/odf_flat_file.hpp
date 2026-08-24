#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>
#include <string>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal::text {
class TextFile;
} // namespace odr::internal::text

namespace odr::internal::xml {
class XmlFile;
} // namespace odr::internal::xml

namespace odr::internal::odf {

/// Whether @p file has an `office:document` root whose `office:mimetype` names
/// a document type we decode, which is all that tells a flat OpenDocument from
/// any other xml.
[[nodiscard]] bool is_flat_opendocument_file(const xml::XmlFile &file);

/// An OpenDocument written as one xml file rather than a zip package
/// (`.fodt`, `.fodp`, `.fods`, `.fodg`), decoding to the same @ref Document as
/// the packaged form. It has no filesystem and is never encrypted.
class FlatOpenDocumentFile final : public virtual abstract::DocumentFile {
public:
  /// @throws NoOpenDocumentFile if @p file is not a flat OpenDocument.
  explicit FlatOpenDocumentFile(const xml::XmlFile &file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] DocumentType document_type() const override;

  [[nodiscard]] bool password_encrypted() const noexcept override;
  [[nodiscard]] EncryptionState encryption_state() const noexcept override;
  [[nodiscard]] std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

private:
  std::shared_ptr<text::TextFile> m_file;
  FileMeta m_file_meta;
};

} // namespace odr::internal::odf
