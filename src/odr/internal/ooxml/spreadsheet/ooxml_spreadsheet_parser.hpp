#pragma once

#include <odr/document_element.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>

#include <memory>
#include <utility>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {
class Document;
class Element;

Element *parse_tree(Document &document, pugi::xml_node node,
                    const Path &document_path,
                    const Relations &document_relations);

} // namespace odr::internal::ooxml::spreadsheet
