#ifndef ODR_INTERNAL_ODF_PARSER_H
#define ODR_INTERNAL_ODF_PARSER_H

#include <memory>
#include <pugixml.hpp>
#include <utility>
#include <vector>

namespace odr::internal::odf {
class Element;

std::tuple<Element *, std::vector<std::unique_ptr<Element>>>
parse_tree(pugi::xml_node node);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_PARSER_H
