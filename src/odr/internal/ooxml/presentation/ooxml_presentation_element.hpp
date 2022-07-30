#ifndef ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
#define ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <pugixml.hpp>
#include <string>

namespace odr::internal::ooxml::presentation {
class Document;

class Element : public common::Element {
public:
  Element();
  explicit Element(pugi::xml_node node);

  common::ResolvedStyle partial_style(const abstract::Document *document) const;
  common::ResolvedStyle
  intermediate_style(const abstract::Document *document) const;

protected:
  static const Document *document_(const abstract::Document *document);
  static pugi::xml_node slide_(const abstract::Document *document,
                               const std::string &id);
};

template <ElementType element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return element_type;
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;
};

class Slide final : public Element, public abstract::SlideElement {
public:
  using Element::Element;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final;

  [[nodiscard]] abstract::MasterPageElement *
  master_page(const abstract::Document *document) const final;

  [[nodiscard]] std::string
  name(const abstract::Document *document) const final;

private:
  pugi::xml_node slide_node_(const abstract::Document *document) const;
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
  Text();
  explicit Text(pugi::xml_node node);
  Text(pugi::xml_node first, pugi::xml_node last);

  [[nodiscard]] std::string
  content(const abstract::Document *document) const final;

  void set_content(const abstract::Document *document,
                   const std::string &content);

  [[nodiscard]] TextStyle style(const abstract::Document *document) const final;

private:
  static std::string text_(const pugi::xml_node node);

  pugi::xml_node m_last;
};

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *document) const final;

  [[nodiscard]] abstract::Element *
  first_column(const abstract::Document *document) const final;

  [[nodiscard]] abstract::Element *
  first_row(const abstract::Document *document) const final;

  [[nodiscard]] TableStyle
  style(const abstract::Document *document) const final;
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

class ImageElement final : public Element, public abstract::ImageElement {
public:
  using Element::Element;

  [[nodiscard]] bool internal(const abstract::Document *document) const final;

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *document) const final;

  [[nodiscard]] std::string
  href(const abstract::Document *document) const final;
};

} // namespace odr::internal::ooxml::presentation

#endif // ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
