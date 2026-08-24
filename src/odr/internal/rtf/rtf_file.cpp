#include <odr/internal/rtf/rtf_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/magic.hpp>
#include <odr/internal/rtf/rtf_document.hpp>

#include <utility>

namespace odr::internal::rtf {

RtfFile::RtfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (magic::file_type(*m_file) != FileType::rich_text_format) {
    throw NoRtfFile();
  }
}

std::shared_ptr<abstract::File> RtfFile::file() const noexcept {
  return m_file;
}

FileType RtfFile::file_type() const noexcept {
  return FileType::rich_text_format;
}

std::string_view RtfFile::mimetype() const noexcept {
  return "application/rtf";
}

FileMeta RtfFile::file_meta() const noexcept {
  FileMeta result;
  result.type = file_type();
  result.mimetype = mimetype();
  result.document_type = document_type();
  return result;
}

DocumentType RtfFile::document_type() const { return DocumentType::text; }

bool RtfFile::is_decodable() const noexcept { return true; }

std::shared_ptr<abstract::Document> RtfFile::document() const {
  return std::make_shared<Document>(*m_file);
}

} // namespace odr::internal::rtf
