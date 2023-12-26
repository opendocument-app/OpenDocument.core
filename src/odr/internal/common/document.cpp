#include <odr/internal/common/document.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>

#include <utility>

namespace odr::internal::common {

Document::Document(FileType file_type, DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_file_type{file_type}, m_document_type{document_type},
      m_filesystem{std::move(filesystem)} {}

FileType Document::file_type() const noexcept { return m_file_type; }

DocumentType Document::document_type() const noexcept {
  return m_document_type;
}

std::shared_ptr<abstract::ReadableFilesystem> Document::files() const noexcept {
  return m_filesystem;
}

} // namespace odr::internal::common
