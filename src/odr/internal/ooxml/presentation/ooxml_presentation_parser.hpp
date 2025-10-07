#pragma once

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::ooxml::presentation {
class Document;
class Element;

Element *parse_tree(Document &document, pugi::xml_node node);

} // namespace odr::internal::ooxml::presentation
