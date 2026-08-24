#include <odr/internal/odf/odf_flat_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/odf/odf_meta.hpp>
#include <odr/internal/text/text_file.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/internal/xml/xml_file.hpp>

#include <pugixml.hpp>

namespace odr::internal::odf {

bool is_flat_opendocument_file(const xml::XmlFile &file) {
  if (file.root_name() != "office:document") {
    return false;
  }
  try {
    parse_flat_file_meta(file.document().document_element());
  } catch (...) {
    return false;
  }
  return true;
}

FlatOpenDocumentFile::FlatOpenDocumentFile(const xml::XmlFile &file)
    : m_file{std::make_shared<text::TextFile>(file.file(), file.encoding())},
      m_file_meta{parse_flat_file_meta(file.document().document_element())} {}

std::shared_ptr<abstract::File> FlatOpenDocumentFile::file() const noexcept {
  return m_file->file();
}

FileType FlatOpenDocumentFile::file_type() const noexcept {
  return m_file_meta.type;
}

std::string_view FlatOpenDocumentFile::mimetype() const noexcept {
  return m_file_meta.mimetype;
}

FileMeta FlatOpenDocumentFile::file_meta() const noexcept {
  return m_file_meta;
}

DocumentType FlatOpenDocumentFile::document_type() const {
  return m_file_meta.document_type;
}

bool FlatOpenDocumentFile::password_encrypted() const noexcept { return false; }

EncryptionState FlatOpenDocumentFile::encryption_state() const noexcept {
  return EncryptionState::not_encrypted;
}

std::shared_ptr<abstract::DecodedFile>
FlatOpenDocumentFile::decrypt(const std::string & /*password*/) const {
  throw NotEncryptedError();
}

bool FlatOpenDocumentFile::is_decodable() const noexcept { return true; }

std::shared_ptr<abstract::Document> FlatOpenDocumentFile::document() const {
  // the recogniser's tree is parsed for a source view; the document model
  // needs its own parse
  return std::make_shared<Document>(m_file_meta.type, m_file_meta.document_type,
                                    util::xml::parse(m_file->text()));
}

} // namespace odr::internal::odf
