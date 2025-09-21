#include <odr/internal/common/document.hpp>

#include <odr/internal/abstract/filesystem.hpp>

#include <utility>

namespace odr::internal {

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_file_type{file_type}, m_document_type{document_type},
      m_filesystem{std::move(files)} {}

FileType Document::file_type() const noexcept { return m_file_type; }

DocumentType Document::document_type() const noexcept {
  return m_document_type;
}

std::shared_ptr<abstract::ReadableFilesystem>
Document::as_filesystem() const noexcept {
  return m_filesystem;
}

} // namespace odr::internal
