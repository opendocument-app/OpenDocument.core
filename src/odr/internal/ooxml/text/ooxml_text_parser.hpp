#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {
class Document;
class Element;

Element *parse_tree(Document &document, pugi::xml_node node);

} // namespace odr::internal::ooxml::text
