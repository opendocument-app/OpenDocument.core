#include <OpenStrategy.h>
#include <access/Path.h>
#include <common/Document.h>
#include <common/DocumentElements.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr {

FileType Document::type(const std::string &path) {
  return Document(path).fileType();
}

FileMeta Document::meta(const std::string &path) {
  return Document(path).fileMeta();
}

Document::Document(const std::string &path) : File(OpenStrategy::openDocument(path)) {}

DocumentType Document::documentType() const noexcept { return File::fileMeta().documentType; }

bool Document::editable() const noexcept { return m_document->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_document->savable(encrypted);
}

void Document::save(const std::string &path) const { m_document->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_document->save(path, password);
}

PageProperties TextDocument::pageProperties() const {
  return m_text_document->pageProperties();
}

ElementSiblingRange TextDocument::content() const {
  return m_text_document->content();
}

} // namespace odr
