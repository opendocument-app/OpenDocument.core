#include <functional>
#include <odr/file.hpp>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/text/ooxml_text_document.hpp>
#include <odr/internal/ooxml/text/ooxml_text_element.hpp>
#include <odr/internal/ooxml/text/ooxml_text_style.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>
#include <optional>
#include <pugixml.hpp>
#include <utility>

namespace odr::internal::ooxml::text {

Element::Element() = default;

Element::Element(pugi::xml_node node) : common::Element(node) {}

common::ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {};
}

common::ResolvedStyle
Element::intermediate_style(const abstract::Document *document) const {
  if (m_parent == nullptr) {
    return partial_style(document);
  }
  auto base = dynamic_cast<Element *>(m_parent)->intermediate_style(document);
  base.override(partial_style(document));
  return base;
}

const Document *Element::document_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document);
}

const StyleRegistry *Element::style_(const abstract::Document *document) {
  return &dynamic_cast<const Document *>(document)->m_style_registry;
}

const std::unordered_map<std::string, std::string> &
Element::document_relations_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document)->m_document_relations;
}

common::ResolvedStyle
Paragraph::partial_style(const abstract::Document *document) const {
  return style_(document)->partial_paragraph_style(m_node);
}

ParagraphStyle Paragraph::style(const abstract::Document *document) const {
  return intermediate_style(document).paragraph_style;
}

TextStyle Paragraph::text_style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

common::ResolvedStyle
Span::partial_style(const abstract::Document *document) const {
  return style_(document)->partial_text_style(m_node);
}

TextStyle Span::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

bool Text::is_text_(const pugi::xml_node node) {
  std::string name = node.name();

  if (name == "w:t") {
    return true;
  }
  if (name == "w:tab") {
    return true;
  }

  return false;
}

std::string Text::text_(const pugi::xml_node node) {
  std::string name = node.name();

  if (name == "w:t") {
    return node.text().get();
  }
  if (name == "w:tab") {
    return "\t";
  }

  return "";
}

Text::Text() = default;

Text::Text(pugi::xml_node node) : Text(node, node) {}

Text::Text(pugi::xml_node fist, pugi::xml_node last)
    : Element(fist), m_last{last} {}

std::string Text::content(const abstract::Document *) const {
  std::string result;
  for (auto node = m_node; node != m_last.next_sibling();
       node = node.next_sibling()) {
    result += text_(node);
  }
  return result;
}

void Text::set_content(const abstract::Document *, const std::string &text) {
  // TODO http://officeopenxml.com/WPtextSpacing.php
  // <w:t xml:space="preserve">
  // use `xml:space`
  auto parent = m_node.parent();
  auto old_start = m_node;

  auto tokens = util::xml::tokenize_text(text);
  for (auto &&token : tokens) {
    switch (token.type) {
    case util::xml::StringToken::Type::none:
      break;
    case util::xml::StringToken::Type::string: {
      auto text_node = parent.insert_child_before("w:t", old_start);
      text_node.append_child(pugi::xml_node_type::node_pcdata)
          .text()
          .set(token.string.c_str());
    } break;
    case util::xml::StringToken::Type::spaces: {
      auto text_node = parent.insert_child_before("w:t", old_start);
      text_node.append_attribute("xml:space").set_value("preserve");
      text_node.append_child(pugi::xml_node_type::node_pcdata)
          .text()
          .set(token.string.c_str());
    } break;
    case util::xml::StringToken::Type::tabs: {
      for (std::size_t i = 0; i < token.string.size(); ++i) {
        parent.insert_child_before("w:tab", old_start);
      }
    } break;
    }
  }

  // TODO remove other
}

TextStyle Text::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

std::string Link::href(const abstract::Document *document) const {
  if (auto anchor = m_node.attribute("w:anchor")) {
    return std::string("#") + anchor.value();
  }
  if (auto ref = m_node.attribute("r:id")) {
    auto relations = document_relations_(document);
    if (auto rel = relations.find(ref.value()); rel != std::end(relations)) {
      return rel->second;
    }
  }
  return "";
}

std::string Bookmark::name(const abstract::Document *) const {
  return m_node.attribute("w:name").value();
}

bool ListElement::is_list_item(const pugi::xml_node node) {
  return node.child("w:pPr").child("w:numPr");
}

std::int32_t ListElement::level(const pugi::xml_node node) {
  return node.child("w:pPr")
      .child("w:numPr")
      .child("w:ilvl")
      .attribute("w:val")
      .as_int(0);
}

ElementType ListElement::type(const abstract::Document *) const {
  return ElementType::list;
}

TextStyle ListElement::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

ElementType ListRoot::type(const abstract::Document *) const {
  return ElementType::list;
}

TableDimensions TableElement::dimensions(const abstract::Document *) const {
  return {}; // TODO
}

abstract::Element *
TableElement::first_column(const abstract::Document *document) const {
  return nullptr; // TODO
}

abstract::Element *
TableElement::first_row(const abstract::Document *document) const {
  return nullptr; // TODO
}

TableStyle TableElement::style(const abstract::Document *document) const {
  return style_(document)->partial_table_style(m_node).table_style;
}

TableColumnStyle TableColumn::style(const abstract::Document *) const {
  TableColumnStyle result;
  if (auto width = read_twips_attribute(m_node.attribute("w:w"))) {
    result.width = width;
  }
  return result;
}

TableRowStyle TableRow::style(const abstract::Document *document) const {
  return style_(document)->partial_table_row_style(m_node).table_row_style;
}

abstract::Element *TableCell::column(const abstract::Document *) const {
  return nullptr; // TODO
}

abstract::Element *TableCell::row(const abstract::Document *) const {
  return nullptr; // TODO
}

bool TableCell::covered(const abstract::Document *) const { return false; }

TableDimensions TableCell::span(const abstract::Document *) const {
  return {1, 1};
}

ValueType TableCell::value_type(const abstract::Document *) const {
  return ValueType::string;
}

TableCellStyle TableCell::style(const abstract::Document *document) const {
  return style_(document)->partial_table_cell_style(m_node).table_cell_style;
}

AnchorType Frame::anchor_type(const abstract::Document *) const {
  if (m_node.child("wp:inline")) {
    return AnchorType::as_char;
  }
  return AnchorType::as_char; // TODO default?
}

std::optional<std::string> Frame::x(const abstract::Document *) const {
  return {};
}

std::optional<std::string> Frame::y(const abstract::Document *) const {
  return {};
}

std::optional<std::string> Frame::width(const abstract::Document *) const {
  if (auto width = read_emus_attribute(
          inner_node_().child("wp:extent").attribute("cx"))) {
    return width->to_string();
  }
  return {};
}

std::optional<std::string> Frame::height(const abstract::Document *) const {
  if (auto height = read_emus_attribute(
          inner_node_().child("wp:extent").attribute("cy"))) {
    return height->to_string();
  }
  return {};
}

std::optional<std::string> Frame::z_index(const abstract::Document *) const {
  return {};
}

GraphicStyle Frame::style(const abstract::Document *) const { return {}; }

pugi::xml_node Frame::inner_node_() const {
  if (auto anchor = m_node.child("wp:anchor")) {
    return anchor;
  } else if (auto inline_node = m_node.child("wp:inline")) {
    return inline_node;
  }
  return {};
}

bool ImageElement::internal(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !doc->files()) {
    return false;
  }
  try {
    return doc->files()->is_file(href(document));
  } catch (...) {
  }
  return false;
}

std::optional<odr::File>
ImageElement::file(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !internal(document)) {
    return {};
  }
  return File(doc->files()->open(href(document)));
}

std::string ImageElement::href(const abstract::Document *document) const {
  if (auto ref = m_node.child("pic:pic")
                     .child("pic:blipFill")
                     .child("a:blip")
                     .attribute("r:embed")) {
    auto relations = document_relations_(document);
    if (auto rel = relations.find(ref.value()); rel != std::end(relations)) {
      return common::Path("word").join(rel->second).string();
    }
  }
  return ""; // TODO
}

} // namespace odr::internal::ooxml::text
