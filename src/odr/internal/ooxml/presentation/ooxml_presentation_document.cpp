#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>
#include <odr/internal/util/xml_util.hpp>

namespace odr::internal::ooxml::presentation {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : common::TemplateDocument<Element>(FileType::office_open_xml_presentation,
                                        DocumentType::presentation,
                                        std::move(filesystem)) {
  m_document_xml = util::xml::parse(*m_filesystem, "ppt/presentation.xml");

  for (auto relationships :
       parse_relationships(*m_filesystem, "ppt/presentation.xml")) {
    m_slides_xml[relationships.first] = util::xml::parse(
        *m_filesystem, common::Path("ppt").join(relationships.second));
  }

  m_root_element = parse_tree(*this, m_document_xml.document_element());
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void Document::save(const common::Path & /*path*/,
                    const char * /*password*/) const {
  throw UnsupportedOperation();
}

pugi::xml_node Document::get_slide_root(const std::string &ref) const {
  if (auto it = m_slides_xml.find(ref); it != std::end(m_slides_xml)) {
    return it->second.document_element();
  }
  return {};
}

} // namespace odr::internal::ooxml::presentation
