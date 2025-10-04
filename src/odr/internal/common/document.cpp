#include <odr/internal/common/document.hpp>

#include <odr/document_element_identifier.hpp>
#include <odr/internal/abstract/filesystem.hpp>

namespace odr::internal {

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_file_type{file_type}, m_document_type{document_type},
      m_files{std::move(files)} {}

FileType Document::file_type() const noexcept { return m_file_type; }

DocumentType Document::document_type() const noexcept {
  return m_document_type;
}

std::shared_ptr<abstract::ReadableFilesystem>
Document::as_filesystem() const noexcept {
  return m_files;
}

ExtendedElementIdentifier Document::root_element() const {
  return m_root_element;
}

const abstract::ElementAdapter *
Document::element_adapter(ExtendedElementIdentifier) const {
  return m_element_adapter;
}

} // namespace odr::internal
