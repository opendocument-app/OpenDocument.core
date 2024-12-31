#include <odr/document.hpp>

#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/path.hpp>

#include <stdexcept>

namespace odr {

Document::Document(std::shared_ptr<internal::abstract::Document> impl)
    : m_impl{std::move(impl)} {
  if (!m_impl) {
    throw std::runtime_error("document is null");
  }
}

bool Document::editable() const noexcept { return m_impl->is_editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_impl->is_savable(encrypted);
}

void Document::save(const std::string &path) const {
  m_impl->save(internal::common::Path(path));
}

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_impl->save(internal::common::Path(path), password.c_str());
}

FileType Document::file_type() const noexcept { return m_impl->file_type(); }

DocumentType Document::document_type() const noexcept {
  return m_impl->document_type();
}

Element Document::root_element() const {
  return {m_impl.get(), m_impl->root_element()};
}

Filesystem Document::files() const { return Filesystem(m_impl->files()); }

} // namespace odr
