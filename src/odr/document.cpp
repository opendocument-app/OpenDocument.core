#include <odr/document.hpp>

#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/util/file_util.hpp>

#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

namespace odr {

Document::Document(std::shared_ptr<internal::abstract::Document> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw NullPointerError("document is null");
  }
}

bool Document::is_editable() const noexcept { return m_impl->is_editable(); }

bool Document::is_savable(const bool encrypted) const noexcept {
  return m_impl->is_savable(encrypted);
}

// Every overload checks before it writes, so an unsavable format leaves no
// empty file behind and no half-written stream.
void Document::save(const std::string &path) const {
  if (!m_impl->is_savable(false)) {
    throw UnsupportedOperation();
  }
  std::ofstream out = internal::util::file::create(path);
  m_impl->save(out);
}

void Document::save(const std::string &path,
                    const std::string &password) const {
  if (!m_impl->is_savable(true)) {
    throw UnsupportedOperation();
  }
  std::ofstream out = internal::util::file::create(path);
  m_impl->save(out, password.c_str());
}

void Document::save(std::ostream &out) const {
  if (!m_impl->is_savable(false)) {
    throw UnsupportedOperation();
  }
  m_impl->save(out);
}

void Document::save(std::ostream &out, const std::string &password) const {
  if (!m_impl->is_savable(true)) {
    throw UnsupportedOperation();
  }
  m_impl->save(out, password.c_str());
}

File Document::save_to_memory() const {
  std::ostringstream out;
  save(out);
  return File::from_memory(std::move(out).str());
}

File Document::save_to_memory(const std::string &password) const {
  std::ostringstream out;
  save(out, password);
  return File::from_memory(std::move(out).str());
}

FileType Document::file_type() const noexcept { return m_impl->file_type(); }

DocumentType Document::document_type() const noexcept {
  return m_impl->document_type();
}

Element Document::root_element() const {
  return {m_impl->element_adapter(), m_impl->root_element()};
}

Filesystem Document::as_filesystem() const {
  if (std::shared_ptr<internal::abstract::ReadableFilesystem> files =
          m_impl->as_filesystem()) {
    return Filesystem(std::move(files));
  }
  // a document that is one file has no files of its own rather than no answer
  return Filesystem(std::make_shared<internal::VirtualFilesystem>());
}

} // namespace odr
