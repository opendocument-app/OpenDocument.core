#ifndef ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
#define ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

#include <string>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {
class Document;

class Element : public common::Element {
public:
  explicit Element(pugi::xml_node);

  [[nodiscard]] virtual common::ResolvedStyle
  partial_style(const abstract::Document *) const;
  [[nodiscard]] virtual common::ResolvedStyle
  intermediate_style(const abstract::Document *) const;

  [[nodiscard]] bool
  is_editable(const abstract::Document *document) const override;

protected:
  static const Document *document_(const abstract::Document *);
  static pugi::xml_node slide_(const abstract::Document *,
                               const std::string &id);

  pugi::xml_node m_node;
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

class Slide final : public Element, public abstract::Slide {
public:
  using Element::Element;

  [[nodiscard]] PageLayout page_layout(const abstract::Document *) const final;

  [[nodiscard]] abstract::Element *
  master_page(const abstract::Document *) const final;

  [[nodiscard]] std::string name(const abstract::Document *) const final;

private:
  pugi::xml_node slide_node_(const abstract::Document *) const;
};

class Paragraph final : public Element, public abstract::Paragraph {
public:
  using Element::Element;

  [[nodiscard]] common::ResolvedStyle
  partial_style(const abstract::Document *) const final;

  [[nodiscard]] ParagraphStyle style(const abstract::Document *) const final;

  [[nodiscard]] TextStyle text_style(const abstract::Document *) const final;
};

class Span final : public Element, public abstract::Span {
public:
  using Element::Element;

  [[nodiscard]] common::ResolvedStyle
  partial_style(const abstract::Document *) const final;

  [[nodiscard]] TextStyle style(const abstract::Document *) const final;
};

class Text final : public Element, public abstract::Text {
public:
  explicit Text(pugi::xml_node node);
  Text(pugi::xml_node first, pugi::xml_node last);

  [[nodiscard]] std::string content(const abstract::Document *) const final;

  void set_content(const abstract::Document *, const std::string &content);

  [[nodiscard]] TextStyle style(const abstract::Document *) const final;

private:
  static std::string text_(const pugi::xml_node node);

  pugi::xml_node m_last;
};

class TableElement : public Element, public abstract::Table {
public:
  using Element::Element;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final;

  [[nodiscard]] abstract::Element *
  first_column(const abstract::Document *) const final;

  [[nodiscard]] abstract::Element *
  first_row(const abstract::Document *) const final;

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

  [[nodiscard]] bool is_covered(const abstract::Document *) const final;

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
};

class ImageElement final : public Element, public abstract::Image {
public:
  using Element::Element;

  [[nodiscard]] bool is_internal(const abstract::Document *) const final;

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *) const final;

  [[nodiscard]] std::string href(const abstract::Document *) const final;
};

} // namespace odr::internal::ooxml::presentation

#endif // ODR_INTERNAL_OOXML_PRESENTATION_ELEMENT_H
