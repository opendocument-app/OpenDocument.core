#ifndef ODR_INTERNAL_OOXML_TEXT_PARSER_HPP
#define ODR_INTERNAL_OOXML_TEXT_PARSER_HPP

#include <memory>
#include <pugixml.hpp>
#include <utility>
#include <vector>

namespace odr::internal::ooxml::text {
class Element;

std::tuple<Element *, std::vector<std::unique_ptr<Element>>>
parse_tree(pugi::xml_node node);

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_PARSER_HPP
