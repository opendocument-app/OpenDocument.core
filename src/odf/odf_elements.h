#ifndef ODR_ODF_ELEMENTS_H
#define ODR_ODF_ELEMENTS_H

#include <common/document_elements.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::odf {
struct ResolvedStyle;
class OpenDocument;

std::shared_ptr<common::Element>
factorizeRoot(std::shared_ptr<const OpenDocument> document,
              pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OpenDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node);

std::shared_ptr<common::Element>
factorizePreviousSibling(std::shared_ptr<const OpenDocument> document,
                         std::shared_ptr<const common::Element> parent,
                         pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OpenDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node);

} // namespace odr::odf

#endif // ODR_ODF_ELEMENTS_H
