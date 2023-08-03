#include <odr/document.h>

#include <odr/document_cursor.h>
#include <odr/file.h>

#include <internal/abstract/document.h>
#include <internal/abstract/document_cursor.h>
#include <internal/common/path.h>

#include <stdexcept>
#include <utility>

namespace odr {

Document::Document(std::shared_ptr<internal::abstract::Document> document)
    : m_document{std::move(document)} {
  if (!m_document) {
    throw std::runtime_error("document is null");
  }
}

bool Document::editable() const noexcept { return m_document->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_document->savable(encrypted);
}

void Document::save(const std::string &path) const { m_document->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_document->save(path, password.c_str());
}

FileType Document::file_type() const noexcept {
  return m_document->file_type();
}

DocumentType Document::document_type() const noexcept {
  return m_document->document_type();
}

DocumentCursor Document::root_element() const {
  return {m_document, m_document->root_element()};
}

} // namespace odr
