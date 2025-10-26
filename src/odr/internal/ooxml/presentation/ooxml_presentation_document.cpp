#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>
#include <odr/internal/util/xml_util.hpp>

namespace odr::internal::ooxml::presentation {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::office_open_xml_presentation,
                         DocumentType::presentation, std::move(files)) {
  m_document_xml = util::xml::parse(*m_files, AbsPath("/ppt/presentation.xml"));

  for (const auto &relationships :
       parse_relationships(*m_files, AbsPath("/ppt/presentation.xml"))) {
    m_slides_xml[relationships.first] = util::xml::parse(
        *m_files, AbsPath("/ppt").join(RelPath(relationships.second)));
  }

  m_root_element =
      parse_tree(m_element_registry, m_document_xml.document_element());
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(const Path & /*path*/) const {
  throw UnsupportedOperation();
}

void Document::save(const Path & /*path*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml::presentation
