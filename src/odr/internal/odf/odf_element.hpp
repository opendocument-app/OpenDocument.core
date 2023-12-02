#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

#include <odr/internal/common/table_position.hpp>

namespace pugi {
class xml_node;
}

namespace odr::internal::odf {
class Document;
class StyleRegistry;

class Element : public virtual common::Element {
public:
  explicit Element(pugi::xml_node);

  virtual common::ResolvedStyle partial_style(const abstract::Document *) const;
  virtual common::ResolvedStyle
  intermediate_style(const abstract::Document *) const;

protected:
  pugi::xml_node m_node;

  virtual const char *style_name_(const abstract::Document *) const;

  static const Document *document_(const abstract::Document *);
  static const StyleRegistry *style_(const abstract::Document *);
};

template <ElementType element_type> class DefaultElement : public Element {
public:
  explicit DefaultElement(pugi::xml_node node) : Element(node) {}

  [[nodiscard]] ElementType type(const abstract::Document *,
                                 ElementIdentifier) const final {
    return element_type;
  }
};

class Root : public Element {
public:
  explicit Root(pugi::xml_node);

  [[nodiscard]] ElementType type(const abstract::Document *,
                                 ElementIdentifier) const override;
};

class TextRoot final : public Root, public abstract::TextRoot {
public:
  explicit TextRoot(pugi::xml_node);

  [[nodiscard]] ElementType type(const abstract::Document *,
                                 ElementIdentifier) const final;

  [[nodiscard]] PageLayout page_layout(const abstract::Document *,
                                       ElementIdentifier) const final;

  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  first_master_page(const abstract::Document *, ElementIdentifier) const final;
};

class PresentationRoot final : public Root {
public:
  explicit PresentationRoot(pugi::xml_node);
};

class DrawingRoot final : public Root {
public:
  explicit DrawingRoot(pugi::xml_node);
};

class MasterPage final : public Element, public abstract::MasterPage {
public:
  explicit MasterPage(pugi::xml_node);

  [[nodiscard]] PageLayout page_layout(const abstract::Document *,
                                       ElementIdentifier) const final;
};

class Slide final : public Element, public abstract::Slide {
public:
  explicit Slide(pugi::xml_node);

  [[nodiscard]] PageLayout page_layout(const abstract::Document *,
                                       ElementIdentifier) const final;

  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  master_page(const abstract::Document *, ElementIdentifier) const final;

  [[nodiscard]] std::string name(const abstract::Document *,
                                 ElementIdentifier) const final;
};

class Page final : public Element, public abstract::Page {
public:
  explicit Page(pugi::xml_node);

  [[nodiscard]] PageLayout page_layout(const abstract::Document *,
                                       ElementIdentifier) const final;

  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  master_page(const abstract::Document *, ElementIdentifier) const final;

  [[nodiscard]] std::string name(const abstract::Document *,
                                 ElementIdentifier) const final;
};

class LineBreak final : public Element, public abstract::LineBreak {
public:
  explicit LineBreak(pugi::xml_node);

  [[nodiscard]] TextStyle style(const abstract::Document *,
                                ElementIdentifier) const final;
};

class Paragraph final : public Element, public abstract::Paragraph {
public:
  explicit Paragraph(pugi::xml_node);

  [[nodiscard]] ParagraphStyle style(const abstract::Document *,
                                     ElementIdentifier) const final;

  [[nodiscard]] TextStyle text_style(const abstract::Document *,
                                     ElementIdentifier) const final;
};

class Span final : public Element, public abstract::Span {
public:
  explicit Span(pugi::xml_node);

  [[nodiscard]] TextStyle style(const abstract::Document *,
                                ElementIdentifier) const final;
};

class Text final : public Element, public abstract::Text {
public:
  static std::string text(const pugi::xml_node);

  explicit Text(pugi::xml_node);
  Text(pugi::xml_node first, pugi::xml_node last);

  [[nodiscard]] std::string content(const abstract::Document *,
                                    ElementIdentifier) const final;

  void set_content(const abstract::Document *, ElementIdentifier,
                   const std::string &text) final;

  [[nodiscard]] TextStyle style(const abstract::Document *,
                                ElementIdentifier) const final;

private:
  pugi::xml_node m_last;
};

class Link final : public Element, public abstract::Link {
public:
  explicit Link(pugi::xml_node);

  [[nodiscard]] std::string href(const abstract::Document *,
                                 ElementIdentifier) const final;
};

class Bookmark final : public Element, public abstract::Bookmark {
public:
  explicit Bookmark(pugi::xml_node);

  [[nodiscard]] std::string name(const abstract::Document *,
                                 ElementIdentifier) const final;
};

class ListItem final : public Element, public abstract::ListItem {
public:
  explicit ListItem(pugi::xml_node);

  [[nodiscard]] TextStyle style(const abstract::Document *,
                                ElementIdentifier) const final;
};

class Table : public Element, public common::Table {
public:
  explicit Table(pugi::xml_node);

  [[nodiscard]] TableStyle style(const abstract::Document *,
                                 ElementIdentifier) const final;

  [[nodiscard]] TableDimensions dimensions(const abstract::Document *,
                                           ElementIdentifier) const final;
};

class TableColumn final : public Element, public abstract::TableColumn {
public:
  explicit TableColumn(pugi::xml_node);

  [[nodiscard]] TableColumnStyle style(const abstract::Document *,
                                       ElementIdentifier) const final;
};

class TableRow final : public Element, public abstract::TableRow {
public:
  explicit TableRow(pugi::xml_node);

  [[nodiscard]] TableRowStyle style(const abstract::Document *,
                                    ElementIdentifier) const final;
};

class TableCell final : public Element, public abstract::TableCell {
public:
  explicit TableCell(pugi::xml_node);

  [[nodiscard]] bool covered(const abstract::Document *,
                             ElementIdentifier) const final;

  [[nodiscard]] TableDimensions span(const abstract::Document *,
                                     ElementIdentifier) const final;

  [[nodiscard]] ValueType value_type(const abstract::Document *,
                                     ElementIdentifier) const final;

  [[nodiscard]] TableCellStyle style(const abstract::Document *,
                                     ElementIdentifier) const final;
};

class Frame final : public Element, public abstract::Frame {
public:
  explicit Frame(pugi::xml_node);

  [[nodiscard]] AnchorType anchor_type(const abstract::Document *,
                                       ElementIdentifier) const final;

  [[nodiscard]] std::optional<std::string> x(const abstract::Document *,
                                             ElementIdentifier) const final;
  [[nodiscard]] std::optional<std::string> y(const abstract::Document *,
                                             ElementIdentifier) const final;
  [[nodiscard]] std::optional<std::string> width(const abstract::Document *,
                                                 ElementIdentifier) const final;
  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *, ElementIdentifier) const final;

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *, ElementIdentifier) const final;

  [[nodiscard]] GraphicStyle style(const abstract::Document *,
                                   ElementIdentifier) const final;
};

class Rect final : public Element, public abstract::Rect {
public:
  explicit Rect(pugi::xml_node);

  [[nodiscard]] std::string x(const abstract::Document *,
                              ElementIdentifier) const final;
  [[nodiscard]] std::string y(const abstract::Document *,
                              ElementIdentifier) const final;
  [[nodiscard]] std::string width(const abstract::Document *,
                                  ElementIdentifier) const final;
  [[nodiscard]] std::string height(const abstract::Document *,
                                   ElementIdentifier) const final;

  [[nodiscard]] GraphicStyle style(const abstract::Document *,
                                   ElementIdentifier) const final;
};

class Line final : public Element, public abstract::Line {
public:
  explicit Line(pugi::xml_node);

  [[nodiscard]] std::string x1(const abstract::Document *,
                               ElementIdentifier) const final;
  [[nodiscard]] std::string y1(const abstract::Document *,
                               ElementIdentifier) const final;
  [[nodiscard]] std::string x2(const abstract::Document *,
                               ElementIdentifier) const final;
  [[nodiscard]] std::string y2(const abstract::Document *,
                               ElementIdentifier) const final;

  [[nodiscard]] GraphicStyle style(const abstract::Document *,
                                   ElementIdentifier) const final;
};

class Circle final : public Element, public abstract::Circle {
public:
  explicit Circle(pugi::xml_node);

  [[nodiscard]] std::string x(const abstract::Document *,
                              ElementIdentifier) const final;
  [[nodiscard]] std::string y(const abstract::Document *,
                              ElementIdentifier) const final;
  [[nodiscard]] std::string width(const abstract::Document *,
                                  ElementIdentifier) const final;
  [[nodiscard]] std::string height(const abstract::Document *,
                                   ElementIdentifier) const final;

  [[nodiscard]] GraphicStyle style(const abstract::Document *,
                                   ElementIdentifier) const final;
};

class CustomShape final : public Element, public abstract::CustomShape {
public:
  explicit CustomShape(pugi::xml_node);

  [[nodiscard]] std::optional<std::string> x(const abstract::Document *,
                                             ElementIdentifier) const final;
  [[nodiscard]] std::optional<std::string> y(const abstract::Document *,
                                             ElementIdentifier) const final;
  [[nodiscard]] std::string width(const abstract::Document *,
                                  ElementIdentifier) const final;
  [[nodiscard]] std::string height(const abstract::Document *,
                                   ElementIdentifier) const final;

  [[nodiscard]] GraphicStyle style(const abstract::Document *,
                                   ElementIdentifier) const final;
};

class Image final : public Element, public abstract::Image {
public:
  explicit Image(pugi::xml_node);

  [[nodiscard]] bool internal(const abstract::Document *,
                              ElementIdentifier) const final;

  [[nodiscard]] std::optional<odr::File> file(const abstract::Document *,
                                              ElementIdentifier) const final;

  [[nodiscard]] std::string href(const abstract::Document *,
                                 ElementIdentifier) const final;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
