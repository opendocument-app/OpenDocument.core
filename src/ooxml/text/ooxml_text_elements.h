#ifndef ODR_OOXML_TEXT_ELEMENTS_H
#define ODR_OOXML_TEXT_ELEMENTS_H

#include <common/document_elements.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::ooxml {
class OfficeOpenXmlTextDocument;
}

namespace odr::ooxml::text {

std::shared_ptr<common::Element>
factorizeRoot(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
              pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node);

std::shared_ptr<common::Element> factorizePreviousSibling(
    std::shared_ptr<const OfficeOpenXmlTextDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node);

} // namespace odr::ooxml::text

#endif // ODR_OOXML_TEXT_ELEMENTS_H
