#ifndef ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
#define ODR_INTERNAL_OOXML_TEXT_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {
class Document;
class StyleRegistry;
class Style;

class Element : public virtual common::Element {
public:
  explicit Element(pugi::xml_node);

  virtual common::ResolvedStyle partial_style(const abstract::Document *) const;
  virtual common::ResolvedStyle
  intermediate_style(const abstract::Document *) const;

  bool is_editable(const abstract::Document *) const override;

protected:
  pugi::xml_node m_node;

  static const Document *document_(const abstract::Document *);
  static const StyleRegistry *style_(const abstract::Document *);
  static const std::unordered_map<std::string, std::string> &
  document_relations_(const abstract::Document *);

  friend class Style;
};

template <ElementType _element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return _element_type;
  }
};

class Root final : public Element, public abstract::TextRoot {
public:
  using Element::Element;

  [[nodiscard]] PageLayout page_layout(const abstract::Document *) const final;

  [[nodiscard]] abstract::Element *
  first_master_page(const abstract::Document *) const final;
};

class Paragraph : public Element, public abstract::Paragraph {
public:
  using Element::Element;

  common::ResolvedStyle partial_style(const abstract::Document *) const final;

  [[nodiscard]] ParagraphStyle style(const abstract::Document *) const final;

  [[nodiscard]] TextStyle text_style(const abstract::Document *) const final;
};

class Span final : public Element, public abstract::Span {
public:
  using Element::Element;

  common::ResolvedStyle partial_style(const abstract::Document *) const final;

  [[nodiscard]] TextStyle style(const abstract::Document *) const final;
};

class Text final : public Element, public abstract::Text {
public:
  using Element::Element;

  explicit Text(pugi::xml_node);
  Text(pugi::xml_node first, pugi::xml_node last);

  [[nodiscard]] std::string content(const abstract::Document *) const final;

  void set_content(const abstract::Document *, const std::string &text) final;

  [[nodiscard]] TextStyle style(const abstract::Document *) const final;

private:
  pugi::xml_node m_last;

  static std::string text_(const pugi::xml_node node);
  static pugi::xml_node last_(const abstract::Document *);
};

class Link final : public Element, public abstract::Link {
public:
  using Element::Element;

  [[nodiscard]] std::string href(const abstract::Document *) const final;
};

class Bookmark final : public Element, public abstract::Bookmark {
public:
  using Element::Element;

  [[nodiscard]] std::string name(const abstract::Document *) const final;
};

class List final : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const final;
};

class ListItem final : public Element, public abstract::ListItem {
public:
  using Element::Element;

  [[nodiscard]] TextStyle style(const abstract::Document *) const final;
};

class Table final : public Element, public common::Table {
public:
  using Element::Element;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final;

  [[nodiscard]] TableStyle style(const abstract::Document *) const final;
};

class TableColumn final : public Element, public abstract::TableColumn {
public:
  using Element::Element;

  [[nodiscard]] TableColumnStyle style(const abstract::Document *) const final;
};

class TableRow final : public Element, public abstract::TableRow {
public:
  using Element::Element;

  [[nodiscard]] TableRowStyle style(const abstract::Document *) const final;
};

class TableCell final : public Element, public abstract::TableCell {
public:
  using Element::Element;

  [[nodiscard]] bool covered(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final;

  [[nodiscard]] ValueType value_type(const abstract::Document *) const final;

  [[nodiscard]] TableCellStyle style(const abstract::Document *) const final;
};

class Frame final : public Element, public abstract::Frame {
public:
  using Element::Element;

  [[nodiscard]] AnchorType anchor_type(const abstract::Document *) const final;

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const final;
  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const final;
  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *) const final;
  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const final;

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *) const final;

  [[nodiscard]] GraphicStyle style(const abstract::Document *) const final;

private:
  [[nodiscard]] pugi::xml_node inner_node_(const abstract::Document *) const;
};

class Image final : public Element, public abstract::Image {
public:
  using Element::Element;

  [[nodiscard]] bool internal(const abstract::Document *) const final;

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *) const final;

  [[nodiscard]] std::string href(const abstract::Document *) const final;
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_ELEMENT_H
