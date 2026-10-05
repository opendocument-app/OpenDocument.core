#include <odr/internal/iwork/iwork_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_document.hpp>
#include <odr/internal/iwork/iwork_types.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odr::internal::iwork {

namespace {

/// Keynote-only component, verified against the Pages/Numbers/Keynote fixtures
/// from iWork 13.2 and 14.4.
constexpr std::string_view slide_component = "Slide";

/// Distinguishes Keynote and Numbers, whose root archives share type ID 1.
FileType app_by_components(const abstract::ReadableFilesystem &filesystem) {
  Package package(filesystem);
  if (package.has_component(slide_component)) {
    return FileType::iwork_keynote;
  }
  return FileType::iwork_numbers;
}

/// Detects the app from `Index/Document.iwa`, whose locator is fixed.
FileType parse_file_type(const abstract::ReadableFilesystem &filesystem) {
  const std::string data = read_iwa(filesystem, AbsPath("/Index/Document.iwa"));
  const std::vector<Object> objects = read_objects(data);
  if (objects.empty()) {
    throw NoIworkFile();
  }

  FileType file_type = FileType::unknown;
  switch (objects.front().type) {
  case archive_type::pages_document:
    file_type = FileType::iwork_pages;
    break;
  case archive_type::app_document:
    file_type = app_by_components(filesystem);
    break;
  default:
    break;
  }

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

FileMeta IworkFile::file_meta() const { return m_file_meta; }

DocumentType IworkFile::document_type() const {
  return m_file_meta.document_type;
}

bool IworkFile::is_decodable() const noexcept { return true; }

std::shared_ptr<abstract::Document> IworkFile::document() const {
  switch (file_type()) {
  case FileType::iwork_pages:
  case FileType::iwork_keynote:
  case FileType::iwork_numbers:
    return std::make_shared<Document>(file_type(), m_filesystem);
  default:
    throw UnsupportedFileType(file_type());
  }
}

} // namespace odr::internal::iwork
