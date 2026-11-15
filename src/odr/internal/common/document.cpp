#include <odr/internal/common/document.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/zip/zip_util.hpp>

namespace odr::internal {

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files,
                   const EncryptionState encryption_state,
                   std::shared_ptr<zip::util::Archive> archive)
    : m_file_type{file_type}, m_document_type{document_type},
      m_encryption_state{encryption_state}, m_files{std::move(files)},
      m_archive{std::move(archive)} {}

Document::~Document() = default;

bool Document::is_editable() const noexcept { return false; }

std::optional<std::string> Document::locale() const { return std::nullopt; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::release_source(const AbsPath &path) const {
  if (m_archive != nullptr) {
    m_archive->release_source(path);
  }
}

void Document::save(std::ostream & /*out*/) const {
  throw UnsupportedOperation();
}

void Document::save(std::ostream & /*out*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

FileType Document::file_type() const noexcept { return m_file_type; }

DocumentType Document::document_type() const noexcept {
  return m_document_type;
}

std::shared_ptr<abstract::ReadableFilesystem>
Document::as_filesystem() const noexcept {
  return m_files;
}

ElementIdentifier Document::root_element() const { return m_root_element; }

const abstract::ElementAdapter *Document::element_adapter() const {
  return m_element_adapter.get();
}

void Document::set_adapters_(EngineAdapters adapters) {
  m_element_adapter = std::move(adapters.element);
  m_spreadsheet_adapter = m_document_type == DocumentType::spreadsheet
                              ? adapters.spreadsheet
                              : nullptr;
  m_presentation_adapter = m_document_type == DocumentType::presentation
                               ? adapters.presentation
                               : nullptr;
  m_drawing_adapter =
      m_document_type == DocumentType::drawing ? adapters.drawing : nullptr;
}

const abstract::SpreadsheetAdapter *Document::spreadsheet_adapter() const {
  return m_spreadsheet_adapter;
}

const abstract::PresentationAdapter *Document::presentation_adapter() const {
  return m_presentation_adapter;
}

const abstract::DrawingAdapter *Document::drawing_adapter() const {
  return m_drawing_adapter;
}

bool Document::is_decrypted() const noexcept {
  return m_encryption_state == EncryptionState::decrypted;
}

} // namespace odr::internal
