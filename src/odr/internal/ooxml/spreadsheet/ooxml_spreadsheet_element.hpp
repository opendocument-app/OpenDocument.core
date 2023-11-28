#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

#include <pugixml.hpp>

#include <string>
#include <vector>

namespace odr::internal::ooxml::spreadsheet {
class Document;
class StyleRegistry;

class Element : public common::Element {
public:
  explicit Element(pugi::xml_node node);

  virtual common::ResolvedStyle partial_style(const abstract::Document *,
                                              ElementIdentifier) const;
  common::ResolvedStyle intermediate_style(const abstract::Document *,
                                           ElementIdentifier) const;

protected:
  static const Document *document_(const abstract::Document *);
  static const StyleRegistry *style_registry_(const abstract::Document *);
  static pugi::xml_node sheet_(const abstract::Document *,
                               const std::string &id);
  static pugi::xml_node drawing_(const abstract::Document *,
                                 const std::string &id);
  static const std::vector<pugi::xml_node> &
  shared_strings_(const abstract::Document *);
};

template <ElementType element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *,
                                 ElementIdentifier) const override {
    return element_type;
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;
};

class Sheet final : public Element, public abstract::Sheet {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *,
                                 ElementIdentifier) const final;

  [[nodiscard]] std::string name(const abstract::Document *,
                                 ElementIdentifier) const final;

  [[nodiscard]] TableDimensions dimensions(const abstract::Document *,
                                           ElementIdentifier) const final;

  [[nodiscard]] TableDimensions
  content(const abstract::Document *, ElementIdentifier,
          std::optional<TableDimensions>) const final;

  [[nodiscard]] abstract::Element *column(const abstract::Document *,
                                          ElementIdentifier,
                                          ColumnIndex column) const final;
  [[nodiscard]] abstract::Element *
  row(const abstract::Document *, ElementIdentifier, RowIndex row) const final;
  [[nodiscard]] abstract::Element *cell(const abstract::Document *,
                                        ElementIdentifier, ColumnIndex column,
                                        RowIndex row) const final;

  [[nodiscard]] abstract::Element *first_shape(const abstract::Document *,
                                               ElementIdentifier) const final;

  [[nodiscard]] TableStyle style(const abstract::Document *,
                                 ElementIdentifier) const final;

private:
  pugi::xml_node sheet_node_(const abstract::Document *) const;
  pugi::xml_node drawing_node_(const abstract::Document *) const;
};

class TableColumn final : public Element, public abstract::TableColumn {
public:
  using Element::Element;

  [[nodiscard]] TableColumnStyle style(const abstract::Document *,
                                       ElementIdentifier) const final;

private:
  [[nodiscard]] std::uint32_t min_() const;
  [[nodiscard]] std::uint32_t max_() const;
};

class TableRow final : public Element, public abstract::TableRow {
public:
  using Element::Element;

  [[nodiscard]] TableRowStyle style(const abstract::Document *,
                                    ElementIdentifier) const final;
};

class TableCell final : public Element, public abstract::TableCell {
public:
  using Element::Element;

  [[nodiscard]] bool covered(const abstract::Document *,
                             ElementIdentifier) const final;

  [[nodiscard]] TableDimensions span(const abstract::Document *,
                                     ElementIdentifier) const final;

  [[nodiscard]] ValueType value_type(const abstract::Document *,
                                     ElementIdentifier) const final;

  common::ResolvedStyle partial_style(const abstract::Document *,
                                      ElementIdentifier) const final;

  [[nodiscard]] TableCellStyle style(const abstract::Document *,
                                     ElementIdentifier) const final;
};

class Span final : public Element, public abstract::Span {
public:
  using Element::Element;

  [[nodiscard]] TextStyle style(const abstract::Document *,
                                ElementIdentifier) const final;
};

class Text final : public Element, public abstract::Text {
public:
  explicit Text(pugi::xml_node node);
  Text(pugi::xml_node first, pugi::xml_node last);

  [[nodiscard]] std::string content(const abstract::Document *,
                                    ElementIdentifier) const final;

  void set_content(const abstract::Document *, ElementIdentifier,
                   const std::string &) final;

  [[nodiscard]] TextStyle style(const abstract::Document *,
                                ElementIdentifier) const final;

private:
  static std::string text_(const pugi::xml_node node);

  pugi::xml_node m_last;
};

class Frame final : public Element, public abstract::Frame {
public:
  using Element::Element;

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

class ImageElement final : public Element, public abstract::Image {
public:
  using Element::Element;

  [[nodiscard]] bool internal(const abstract::Document *,
                              ElementIdentifier) const final;

  [[nodiscard]] std::optional<odr::File> file(const abstract::Document *,
                                              ElementIdentifier) const final;

  [[nodiscard]] std::string href(const abstract::Document *,
                                 ElementIdentifier) const final;
};

} // namespace odr::internal::ooxml::spreadsheet

#endif // ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H
