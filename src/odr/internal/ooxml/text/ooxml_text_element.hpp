#pragma once

#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {
class Document;
class StyleRegistry;
class Style;

class Element : public virtual internal::Element {
public:
  explicit Element(pugi::xml_node);

  virtual ResolvedStyle partial_style(const abstract::Document *) const;
  virtual ResolvedStyle intermediate_style(const abstract::Document *) const;

  bool is_editable(const abstract::Document *) const override;

protected:
  pugi::xml_node m_node;

  static const Document *document_(const abstract::Document *);
  static const StyleRegistry *style_(const abstract::Document *);
  static const std::unordered_map<std::string, std::string> &
  document_relations_(const abstract::Document *);

  friend class Style;
};

template <ElementType _element_type>
class DefaultElement final : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return _element_type;
  }
};

class Root final : public Element, public abstract::TextRoot {
public:
  using Element::Element;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *) const override;

  [[nodiscard]] abstract::Element *
  first_master_page(const abstract::Document *) const override;
};

class Paragraph final : public Element, public abstract::Paragraph {
public:
  using Element::Element;

  ResolvedStyle partial_style(const abstract::Document *) const override;

  [[nodiscard]] ParagraphStyle style(const abstract::Document *) const override;

  [[nodiscard]] TextStyle text_style(const abstract::Document *) const override;
};

class Span final : public Element, public abstract::Span {
public:
  using Element::Element;

  ResolvedStyle partial_style(const abstract::Document *) const override;

  [[nodiscard]] TextStyle style(const abstract::Document *) const override;
};

class Text final : public Element, public abstract::Text {
public:
  using Element::Element;

  explicit Text(pugi::xml_node);
  Text(pugi::xml_node first, pugi::xml_node last);

  [[nodiscard]] std::string content(const abstract::Document *) const override;

  void set_content(const abstract::Document *,
                   const std::string &text) override;

  [[nodiscard]] TextStyle style(const abstract::Document *) const override;

private:
  pugi::xml_node m_last;

  static std::string text_(pugi::xml_node node);
};

class Link final : public Element, public abstract::Link {
public:
  using Element::Element;

  [[nodiscard]] std::string href(const abstract::Document *) const override;
};

class Bookmark final : public Element, public abstract::Bookmark {
public:
  using Element::Element;

  [[nodiscard]] std::string name(const abstract::Document *) const override;
};

class List final : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override;
};

class ListItem final : public Element, public abstract::ListItem {
public:
  using Element::Element;

  [[nodiscard]] TextStyle style(const abstract::Document *) const override;
};

class Table final : public Element, public internal::Table {
public:
  using Element::Element;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const override;

  [[nodiscard]] TableStyle style(const abstract::Document *) const override;
};

class TableColumn final : public Element, public abstract::TableColumn {
public:
  using Element::Element;

  [[nodiscard]] TableColumnStyle
  style(const abstract::Document *) const override;
};

class TableRow final : public Element, public abstract::TableRow {
public:
  using Element::Element;

  [[nodiscard]] TableRowStyle style(const abstract::Document *) const override;
};

class TableCell final : public Element, public abstract::TableCell {
public:
  using Element::Element;

  [[nodiscard]] bool is_covered(const abstract::Document *) const override;

  [[nodiscard]] TableDimensions span(const abstract::Document *) const override;

  [[nodiscard]] ValueType value_type(const abstract::Document *) const override;

  [[nodiscard]] TableCellStyle style(const abstract::Document *) const override;
};

class Frame final : public Element, public abstract::Frame {
public:
  using Element::Element;

  [[nodiscard]] AnchorType
  anchor_type(const abstract::Document *) const override;

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const override;
  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const override;
  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *) const override;
  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const override;

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *) const override;

  [[nodiscard]] GraphicStyle style(const abstract::Document *) const override;

private:
  [[nodiscard]] pugi::xml_node inner_node_(const abstract::Document *) const;
};

class Image final : public Element, public abstract::Image {
public:
  using Element::Element;

  [[nodiscard]] bool is_internal(const abstract::Document *) const override;

  [[nodiscard]] std::optional<File>
  file(const abstract::Document *) const override;

  [[nodiscard]] std::string href(const abstract::Document *) const override;
};

} // namespace odr::internal::ooxml::text
