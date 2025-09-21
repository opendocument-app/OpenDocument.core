#include <odr/internal/ooxml/text/ooxml_text_element.hpp>

#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/text/ooxml_text_document.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <optional>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {

Element::Element(pugi::xml_node node) : m_node{node} {
  if (!node) {
    // TODO log error
    throw std::runtime_error("node not set");
  }
}

ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {};
}

ResolvedStyle
Element::intermediate_style(const abstract::Document *document) const {
  ResolvedStyle base;
  abstract::Element *parent = this->parent(document);
  if (parent == nullptr) {
    base = style_(document)->default_style()->resolved();
  } else {
    base = dynamic_cast<Element *>(parent)->intermediate_style(document);
  }
  base.override(partial_style(document));
  return base;
}

bool Element::is_editable(const abstract::Document *document) const {
  if (m_parent == nullptr) {
    return document_(document)->is_editable();
  }
  return m_parent->is_editable(document);
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

PageLayout Root::page_layout(const abstract::Document *) const {
  return {}; // TODO
}

abstract::Element *Root::first_master_page(const abstract::Document *) const {
  return {}; // TODO
}

ResolvedStyle
Paragraph::partial_style(const abstract::Document *document) const {
  return style_(document)->partial_paragraph_style(m_node);
}

ParagraphStyle Paragraph::style(const abstract::Document *document) const {
  return intermediate_style(document).paragraph_style;
}

TextStyle Paragraph::text_style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

ResolvedStyle Span::partial_style(const abstract::Document *document) const {
  return style_(document)->partial_text_style(m_node);
}

TextStyle Span::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

Text::Text(pugi::xml_node node) : Text(node, node) {}

Text::Text(pugi::xml_node first, pugi::xml_node last)
    : Element(first), m_last{last} {
  if (!last) {
    // TODO log error
    throw std::runtime_error("last not set");
  }
}

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
  auto old_first = m_node;
  auto old_last = m_last;
  auto new_first = old_first;
  auto new_last = m_last;

  const auto insert_node = [&](const char *node) {
    auto new_node = parent.insert_child_before(node, old_first);
    if (new_first == old_first) {
      new_first = new_node;
    }
    new_last = new_node;
    return new_node;
  };

  for (const util::xml::StringToken &token : util::xml::tokenize_text(text)) {
    switch (token.type) {
    case util::xml::StringToken::Type::none:
      break;
    case util::xml::StringToken::Type::string: {
      auto text_node = insert_node("w:t");
      text_node.append_child(pugi::xml_node_type::node_pcdata)
          .text()
          .set(token.string.c_str());
    } break;
    case util::xml::StringToken::Type::spaces: {
      auto text_node = insert_node("w:t");
      text_node.append_attribute("xml:space").set_value("preserve");
      text_node.append_child(pugi::xml_node_type::node_pcdata)
          .text()
          .set(token.string.c_str());
    } break;
    case util::xml::StringToken::Type::tabs: {
      for (std::size_t i = 0; i < token.string.size(); ++i) {
        insert_node("w:tab");
      }
    } break;
    }
  }

  m_node = new_first;
  m_last = new_last;

  for (auto node = old_first; node != old_last.next_sibling();) {
    auto next = node.next_sibling();
    parent.remove_child(node);
    node = next;
  }
}

TextStyle Text::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
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

ElementType List::type(const abstract::Document *) const {
  return ElementType::list;
}

TextStyle ListItem::style(const abstract::Document *document) const {
  return intermediate_style(document).text_style;
}

TableDimensions Table::dimensions(const abstract::Document *) const {
  return {}; // TODO
}

TableStyle Table::style(const abstract::Document *document) const {
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

bool TableCell::is_covered(const abstract::Document *) const { return false; }

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

std::optional<std::string>
Frame::width(const abstract::Document *document) const {
  if (auto width = read_emus_attribute(
          inner_node_(document).child("wp:extent").attribute("cx"))) {
    return width->to_string();
  }
  return {};
}

std::optional<std::string>
Frame::height(const abstract::Document *document) const {
  if (auto height = read_emus_attribute(
          inner_node_(document).child("wp:extent").attribute("cy"))) {
    return height->to_string();
  }
  return {};
}

std::optional<std::string> Frame::z_index(const abstract::Document *) const {
  return {};
}

GraphicStyle Frame::style(const abstract::Document *) const { return {}; }

pugi::xml_node Frame::inner_node_(const abstract::Document *) const {
  if (auto anchor = m_node.child("wp:anchor")) {
    return anchor;
  } else if (auto inline_node = m_node.child("wp:inline")) {
    return inline_node;
  }
  return {};
}

bool Image::is_internal(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !doc->as_filesystem()) {
    return false;
  }
  try {
    return doc->as_filesystem()->is_file(AbsPath(href(document)));
  } catch (...) {
  }
  return false;
}

std::optional<odr::File> Image::file(const abstract::Document *document) const {
  auto doc = document_(document);
  if (!doc || !is_internal(document)) {
    return {};
  }
  AbsPath path = Path(href(document)).make_absolute();
  return File(doc->as_filesystem()->open(path));
}

std::string Image::href(const abstract::Document *document) const {
  if (auto ref = m_node.child("pic:pic")
                     .child("pic:blipFill")
                     .child("a:blip")
                     .attribute("r:embed")) {
    auto relations = document_relations_(document);
    if (auto rel = relations.find(ref.value()); rel != std::end(relations)) {
      return AbsPath("/word").join(RelPath(rel->second)).string();
    }
  }
  return ""; // TODO
}

} // namespace odr::internal::ooxml::text
