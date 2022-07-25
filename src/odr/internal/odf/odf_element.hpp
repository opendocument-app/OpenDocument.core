#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

namespace odr::internal::odf {
class Document;
class StyleRegistry;

class Element : public common::Element {
public:
  Element();
  explicit Element(pugi::xml_node node);

  virtual common::ResolvedStyle
  partial_style(const abstract::Document *document) const;
  virtual common::ResolvedStyle
  intermediate_style(const abstract::Document *document) const;

protected:
  virtual const char *style_name_(const abstract::Document *document) const;

  static const Document *document_(const abstract::Document *document);
  static const StyleRegistry *style_(const abstract::Document *document);
};

template <ElementType element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return element_type;
  }
};

class MasterPage final : public Element, public abstract::MasterPageElement {
public:
  using Element::Element;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final;
};

class Root : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType
  type(const abstract::Document *document) const override;
};

class TextRoot final : public Root, public abstract::TextRootElement {
public:
  using Root::Root;

  [[nodiscard]] ElementType
  type(const abstract::Document *document) const override;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final;

  [[nodiscard]] abstract::Element *
  first_master_page(const abstract::Document *document) const final;
};

class PresentationRoot final : public Root {
public:
  using Root::Root;
};

class SpreadsheetRoot final : public Root {
public:
  using Root::Root;
};

class DrawingRoot final : public Root {
public:
  using Root::Root;
};

class Slide final : public Element, public abstract::SlideElement {
public:
  using Element::Element;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final;

  [[nodiscard]] abstract::Element *
  master_page(const abstract::Document *document) const final;

  [[nodiscard]] std::string
  name(const abstract::Document *document) const final;

private:
  MasterPage *master_page_(const abstract::Document *document) const;
};

class Sheet final : public Element, public abstract::SheetElement {
public:
  using Element::Element;

  [[nodiscard]] std::string
  name(const abstract::Document *document) const final;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *document) const final;

  [[nodiscard]] TableDimensions
  content(const abstract::Document *document,
          const std::optional<TableDimensions> range) const final;

  [[nodiscard]] abstract::Element *column(const abstract::Document *document,
                                          std::uint32_t column) const final;
  [[nodiscard]] abstract::Element *row(const abstract::Document *document,
                                       std::uint32_t column) const final;
  [[nodiscard]] abstract::Element *cell(const abstract::Document *document,
                                        std::uint32_t column) const final;

  [[nodiscard]] abstract::Element *
  first_shape(const abstract::Document *document) const final;

  [[nodiscard]] TableStyle
  style(const abstract::Document *document) const final;
};

class Page final : public Element, public abstract::PageElement {
public:
  using Element::Element;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final;

  [[nodiscard]] abstract::Element *
  master_page(const abstract::Document *document) const final;

  [[nodiscard]] std::string
  name(const abstract::Document *document) const final;

private:
  MasterPage *master_page_(const abstract::Document *document) const;
};

class LineBreak final : public Element, public abstract::LineBreakElement {
public:
  using Element::Element;

  [[nodiscard]] TextStyle style(const abstract::Document *document) const final;
};

class Paragraph final : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  [[nodiscard]] ParagraphStyle
  style(const abstract::Document *document) const final;

  [[nodiscard]] TextStyle
  text_style(const abstract::Document *document) const final;
};

class Span final : public Element, public abstract::SpanElement {
public:
  using Element::Element;

  [[nodiscard]] TextStyle style(const abstract::Document *document) const final;
};

class Text final : public Element, public abstract::TextElement {
public:
  static std::string text(const pugi::xml_node node);

  Text();
  explicit Text(pugi::xml_node node);
  Text(pugi::xml_node first, pugi::xml_node last);

  [[nodiscard]] std::string content(const abstract::Document *) const final;

  void set_content(const abstract::Document *, const std::string &text) final;

  [[nodiscard]] TextStyle style(const abstract::Document *document) const final;

private:
  pugi::xml_node m_last;
};

class Link final : public Element, public abstract::LinkElement {
public:
  using Element::Element;

  [[nodiscard]] std::string
  href(const abstract::Document *document) const final;
};

class Bookmark final : public Element, public abstract::BookmarkElement {
public:
  using Element::Element;

  [[nodiscard]] std::string
  name(const abstract::Document *document) const final;
};

class ListItem final : public Element, public abstract::ListItemElement {
public:
  using Element::Element;

  [[nodiscard]] TextStyle style(const abstract::Document *document) const final;
};

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] TableStyle
  style(const abstract::Document *document) const final;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *document) const final;

  abstract::Element *
  first_column(const abstract::Document *document) const final;
  abstract::Element *first_row(const abstract::Document *document) const final;
};

class TableColumn final : public Element, public abstract::TableColumnElement {
public:
  using Element::Element;

  [[nodiscard]] TableColumnStyle
  style(const abstract::Document *document) const final;
};

class TableRow final : public Element, public abstract::TableRowElement {
public:
  using Element::Element;

  [[nodiscard]] TableRowStyle
  style(const abstract::Document *document) const final;
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  column(const abstract::Document *document) const final;
  [[nodiscard]] abstract::Element *
  row(const abstract::Document *document) const final;

  [[nodiscard]] bool covered(const abstract::Document *document) const final;

  [[nodiscard]] TableDimensions
  span(const abstract::Document *document) const final;

  [[nodiscard]] ValueType
  value_type(const abstract::Document *document) const final;

  [[nodiscard]] TableCellStyle
  style(const abstract::Document *document) const final;
};

class Frame final : public Element, public abstract::FrameElement {
public:
  using Element::Element;

  [[nodiscard]] AnchorType
  anchor_type(const abstract::Document *document) const final;

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *document) const final;
  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *document) const final;
  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *document) const final;
  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *document) const final;

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *document) const final;

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final;
};

class Rect final : public Element, public abstract::RectElement {
public:
  using Element::Element;

  [[nodiscard]] std::string x(const abstract::Document *document) const final;
  [[nodiscard]] std::string y(const abstract::Document *document) const final;
  [[nodiscard]] std::string
  width(const abstract::Document *document) const final;
  [[nodiscard]] std::string
  height(const abstract::Document *document) const final;

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final;
};

class Line final : public Element, public abstract::LineElement {
public:
  using Element::Element;

  [[nodiscard]] std::string x1(const abstract::Document *document) const final;
  [[nodiscard]] std::string y1(const abstract::Document *document) const final;
  [[nodiscard]] std::string x2(const abstract::Document *document) const final;
  [[nodiscard]] std::string y2(const abstract::Document *document) const final;

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final;
};

class Circle final : public Element, public abstract::CircleElement {
public:
  using Element::Element;

  [[nodiscard]] std::string x(const abstract::Document *document) const final;
  [[nodiscard]] std::string y(const abstract::Document *document) const final;
  [[nodiscard]] std::string
  width(const abstract::Document *document) const final;
  [[nodiscard]] std::string
  height(const abstract::Document *document) const final;

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final;
};

class CustomShape final : public Element, public abstract::CustomShapeElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *document) const final;
  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *document) const final;
  [[nodiscard]] std::string
  width(const abstract::Document *document) const final;
  [[nodiscard]] std::string
  height(const abstract::Document *document) const final;

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final;
};

class ImageElement final : public Element, public abstract::ImageElement {
public:
  using Element::Element;

  [[nodiscard]] bool internal(const abstract::Document *document) const final;

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *document) const final;

  [[nodiscard]] std::string
  href(const abstract::Document *document) const final;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
