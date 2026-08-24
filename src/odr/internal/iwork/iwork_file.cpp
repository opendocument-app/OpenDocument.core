#include <odr/internal/iwork/iwork_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_document.hpp>
#include <odr/internal/iwork/iwork_types.hpp>

#include <string>
#include <utility>
#include <vector>

namespace odr::internal::iwork {

namespace {

/// The type of the root archive says which app wrote the package. Only
/// `.pages` is pinned — a `.numbers` or `.key` fixture would be needed to read
/// theirs off, and the extension is not an answer.
FileType file_type_by_archive_type(const std::uint32_t type) {
  switch (type) {
  case archive_type::pages_document:
    return FileType::iwork_pages;
  default:
    return FileType::unknown;
  }
}

/// Reads the root archive of the package's `Document` component. The component
/// list in `Index/Metadata.iwa` is not consulted: this runs on every zip a
/// caller opens, and the `Document` component is the one whose file name never
/// carries an identifier suffix.
FileType parse_file_type(const abstract::ReadableFilesystem &filesystem) {
  const std::string data = read_iwa(filesystem, AbsPath("/Index/Document.iwa"));
  const std::vector<Object> objects = read_objects(data);
  if (objects.empty()) {
    throw NoIworkFile();
  }

  const FileType file_type = file_type_by_archive_type(objects.front().type);
  if (file_type == FileType::unknown) {
    throw NoIworkFile();
  }
  return file_type;
}

} // namespace

IworkFile::IworkFile(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  if (!m_filesystem->is_file(AbsPath("/Index/Document.iwa"))) {
    throw NoIworkFile();
  }

  m_file_meta.type = parse_file_type(*m_filesystem);
  m_file_meta.mimetype = mimetype_by_file_type(m_file_meta.type);
  m_file_meta.document_type = document_type_by_file_type(m_file_meta.type);
}

std::shared_ptr<abstract::File> IworkFile::file() const noexcept { return {}; }

FileType IworkFile::file_type() const noexcept { return m_file_meta.type; }

std::string_view IworkFile::mimetype() const noexcept {
  return m_file_meta.mimetype;
}

FileMeta IworkFile::file_meta() const noexcept { return m_file_meta; }

DocumentType IworkFile::document_type() const {
  return m_file_meta.document_type;
}

bool IworkFile::is_decodable() const noexcept { return true; }

std::shared_ptr<abstract::Document> IworkFile::document() const {
  switch (file_type()) {
  case FileType::iwork_pages:
    return std::make_shared<Document>(m_filesystem);
  default:
    throw UnsupportedFileType(file_type());
  }
}

} // namespace odr::internal::iwork
