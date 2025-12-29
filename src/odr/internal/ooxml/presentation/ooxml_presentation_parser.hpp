#pragma once

#include <odr/definitions.hpp>

#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {
class ElementRegistry;

class ParseContext {
public:
  explicit ParseContext(
      const std::unordered_map<std::string, pugi::xml_document> &slides_xml)
      : m_slides_xml(&slides_xml) {}

  [[nodiscard]] const std::unordered_map<std::string, pugi::xml_document> &
  slides_xml() const {
    return *m_slides_xml;
  }

private:
  const std::unordered_map<std::string, pugi::xml_document> *m_slides_xml{
      nullptr};
};

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const ParseContext &context, pugi::xml_node node);

} // namespace odr::internal::ooxml::presentation
