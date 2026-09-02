#include <odr/internal/common/document.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>

namespace odr::internal {

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_file_type{file_type}, m_document_type{document_type},
      m_files{std::move(files)} {}

Document::~Document() = default;

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
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

} // namespace odr::internal
