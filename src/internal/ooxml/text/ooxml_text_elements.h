#ifndef ODR_INTERNAL_OOXML_TEXT_ELEMENTS_H
#define ODR_INTERNAL_OOXML_TEXT_ELEMENTS_H

#include <internal/abstract/document_elements.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::internal::ooxml {
class OfficeOpenXmlTextDocument;
}

namespace odr::internal::ooxml::text {

std::shared_ptr<abstract::Element>
factorize_root(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
               pugi::xml_node node);

std::shared_ptr<abstract::Element>
factorize_element(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                  std::shared_ptr<const abstract::Element> parent,
                  pugi::xml_node node);

std::shared_ptr<abstract::Element>
factorize_first_child(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                      std::shared_ptr<const abstract::Element> parent,
                      pugi::xml_node node);

std::shared_ptr<abstract::Element> factorize_previous_sibling(
    std::shared_ptr<const OfficeOpenXmlTextDocument> document,
    std::shared_ptr<const abstract::Element> parent, pugi::xml_node node);

std::shared_ptr<abstract::Element> factorize_next_sibling(
    std::shared_ptr<const OfficeOpenXmlTextDocument> document,
    std::shared_ptr<const abstract::Element> parent, pugi::xml_node node);

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_ELEMENTS_H