#ifndef ODR_INTERNAL_ODF_ELEMENTS_H
#define ODR_INTERNAL_ODF_ELEMENTS_H

#include <internal/abstract/document_elements.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::internal::odf {
struct ResolvedStyle;
class OpenDocument;

std::shared_ptr<abstract::Element>
factorize_root(std::shared_ptr<const OpenDocument> document,
               pugi::xml_node node);

std::shared_ptr<abstract::Element>
factorize_element(std::shared_ptr<const OpenDocument> document,
                  std::shared_ptr<const abstract::Element> parent,
                  pugi::xml_node node);

std::shared_ptr<abstract::Element>
factorize_first_child(std::shared_ptr<const OpenDocument> document,
                      std::shared_ptr<const abstract::Element> parent,
                      pugi::xml_node node);

std::shared_ptr<abstract::Element>
factorize_previous_sibling(std::shared_ptr<const OpenDocument> document,
                           std::shared_ptr<const abstract::Element> parent,
                           pugi::xml_node node);

std::shared_ptr<abstract::Element>
factorize_next_sibling(std::shared_ptr<const OpenDocument> document,
                       std::shared_ptr<const abstract::Element> parent,
                       pugi::xml_node node);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENTS_H
