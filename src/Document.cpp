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

Document::Document(const std::string &path, const FileType as)
    : File(OpenStrategy::openDocument(path, as)) {}

Document::Document(File &&file) : File(std::move(file)) {
  // TODO check if document
}

common::Document & Document::impl() const noexcept {
  return static_cast<common::Document &>(*impl_);
}

DocumentType Document::documentType() const noexcept { return File::fileMeta().documentType; }

bool Document::editable() const noexcept { return impl().editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return impl().savable(encrypted);
}

void Document::save(const std::string &path) const { impl().save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  impl().save(path, password);
}

TextDocument::TextDocument(const std::string &path) : TextDocument(Document(path)) {
  // TODO
}

TextDocument::TextDocument(const std::string &path, FileType as) : TextDocument(Document(path, as)) {
  // TODO
}

TextDocument::TextDocument(Document &&document) : Document(std::move(document)) {
  // TODO
}

PageProperties TextDocument::pageProperties() const {
  return m_impl->pageProperties();
}

ElementSiblingRange TextDocument::content() const {
  return m_impl->content();
}

} // namespace odr
