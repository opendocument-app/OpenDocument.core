#ifndef ODR_INTERNAL_ODF_PARSER_H
#define ODR_INTERNAL_ODF_PARSER_H

#include <memory>
#include <pugixml.hpp>
#include <utility>
#include <vector>

namespace odr::internal::abstract {
class Element;
}

namespace odr::internal::odf {

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
parse_tree(pugi::xml_node node);

}

#endif // ODR_INTERNAL_ODF_PARSER_H
