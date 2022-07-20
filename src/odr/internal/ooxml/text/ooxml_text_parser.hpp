#ifndef ODR_INTERNAL_OOXML_TEXT_PARSER_HPP
#define ODR_INTERNAL_OOXML_TEXT_PARSER_HPP

#include <memory>
#include <pugixml.hpp>
#include <utility>
#include <vector>

namespace odr::internal::abstract {
class Element;
}

namespace odr::internal::ooxml::text {

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
parse_tree(pugi::xml_node node);

}

#endif // ODR_INTERNAL_OOXML_TEXT_PARSER_HPP
