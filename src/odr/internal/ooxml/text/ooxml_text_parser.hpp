#ifndef ODR_INTERNAL_OOXML_TEXT_PARSER_HPP
#define ODR_INTERNAL_OOXML_TEXT_PARSER_HPP

#include <memory>
#include <utility>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {
class Document;
class Element;

Element *parse_tree(Document &document, pugi::xml_node node);

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_PARSER_HPP
