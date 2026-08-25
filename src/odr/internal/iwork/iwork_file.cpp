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

/// The component Keynote writes one of per slide. A `.numbers` and a `.pages`
/// package hold none — `empty.key` against `empty.numbers` and both `.pages`
/// fixtures (iWork 14.4 / 13.2).
constexpr std::string_view slide_component = "Slide";

/// Which app wrote a package whose root archive is @ref
/// archive_type::app_document. Pages has a type id of its own, but Keynote and
/// Numbers both number their root archive 1 — the id space is per app — so the
/// two are told apart by the components the package holds.
FileType app_by_components(const abstract::ReadableFilesystem &filesystem) {
  Package package(filesystem);
  if (package.has_component(slide_component)) {
    return FileType::iwork_keynote;
  }
  return FileType::iwork_numbers;
}

/// Reads the root archive of the package's `Document` component. The component
/// list in `Index/Metadata.iwa` is not consulted for the root archive itself:
/// this runs on every zip a caller opens, and the `Document` component is the
/// one whose file name never carries an identifier suffix.
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

FileMeta IworkFile::file_meta() const noexcept { return m_file_meta; }

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
