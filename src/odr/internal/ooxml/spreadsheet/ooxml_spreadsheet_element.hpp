#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/abstract/sheet_element.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>

#include <string>
#include <vector>

#include "odr/internal/common/path.hpp"
#include "odr/internal/common/table_position.hpp"
#include "odr/internal/ooxml/ooxml_util.hpp"
#include <map>
#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {
class Document;
class StyleRegistry;

class SheetCell;

class Element : public common::Element {
public:
  Element(pugi::xml_node node, common::Path document_path,
          const Relations &document_relations);

  [[nodiscard]] virtual common::ResolvedStyle
  partial_style(const abstract::Document *) const;
  [[nodiscard]] common::ResolvedStyle
  intermediate_style(const abstract::Document *) const;

  [[nodiscard]] bool is_editable(const abstract::Document *) const override;

  [[nodiscard]] virtual const common::Path &
  document_path_(const abstract::Document *) const;
  [[nodiscard]] virtual const Relations &
  document_relations_(const abstract::Document *) const;

protected:
  pugi::xml_node m_node;

  static const Document *document_(const abstract::Document *);
  static const StyleRegistry *style_registry_(const abstract::Document *);
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
  Root(pugi::xml_node node, common::Path document_path,
       const Relations &document_relations);

  [[nodiscard]] const common::Path &
  document_path_(const abstract::Document *) const final;
  [[nodiscard]] const Relations &
  document_relations_(const abstract::Document *) const final;

private:
  common::Path m_document_path;
  const Relations &m_document_relations;
};

struct SheetIndex final {
  struct Row {
    pugi::xml_node row;
    std::map<std::uint32_t, pugi::xml_node> cells;
  };

  TableDimensions dimensions;

  std::map<std::uint32_t, pugi::xml_node> columns;
  std::map<std::uint32_t, Row> rows;

  void init_column(std::uint32_t min, std::uint32_t max,
                   pugi::xml_node element);
  void init_row(std::uint32_t row, pugi::xml_node element);
  void init_cell(std::uint32_t column, std::uint32_t row,
                 pugi::xml_node element);

  pugi::xml_node column(std::uint32_t) const;
  pugi::xml_node row(std::uint32_t) const;
  pugi::xml_node cell(std::uint32_t column, std::uint32_t row) const;
};

class Sheet final : public Element, public abstract::Sheet {
public:
  Sheet(pugi::xml_node node, common::Path document_path,
        const Relations &document_relations);

  [[nodiscard]] std::string name(const abstract::Document *) const final;

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final;
  [[nodiscard]] TableDimensions
  content(const abstract::Document *,
          std::optional<TableDimensions>) const final;

  [[nodiscard]] abstract::SheetCell *cell(const abstract::Document *,
                                          std::uint32_t column,
                                          std::uint32_t row) const final;

  [[nodiscard]] abstract::Element *
  first_shape(const abstract::Document *) const final;

  [[nodiscard]] TableStyle style(const abstract::Document *) const final;
  [[nodiscard]] TableColumnStyle column_style(const abstract::Document *,
                                              std::uint32_t column) const final;
  [[nodiscard]] TableRowStyle row_style(const abstract::Document *,
                                        std::uint32_t row) const final;
  [[nodiscard]] TableCellStyle cell_style(const abstract::Document *,
                                          std::uint32_t column,
                                          std::uint32_t row) const final;

  void init_column_(std::uint32_t min, std::uint32_t max,
                    pugi::xml_node element);
  void init_row_(std::uint32_t row, pugi::xml_node element);
  void init_cell_(std::uint32_t column, std::uint32_t row,
                  pugi::xml_node element);
  void init_cell_element_(std::uint32_t column, std::uint32_t row,
                          SheetCell *element);
  void init_dimensions_(TableDimensions dimensions);
  void append_shape_(Element *shape);

protected:
  [[nodiscard]] const common::Path &
  document_path_(const abstract::Document *) const final;
  [[nodiscard]] const Relations &
  document_relations_(const abstract::Document *) const final;

private:
  common::Path m_document_path;
  const Relations &m_document_relations;

  SheetIndex m_index;

  std::unordered_map<common::TablePosition, SheetCell *> m_cells;
  Element *m_first_shape{nullptr};
  Element *m_last_shape{nullptr};
};

class SheetCell final : public Element, public abstract::SheetCell {
public:
  using Element::Element;

  [[nodiscard]] bool is_covered(const abstract::Document *) const final;
  [[nodiscard]] TableDimensions span(const abstract::Document *) const final;
  [[nodiscard]] ValueType value_type(const abstract::Document *) const final;

  [[nodiscard]] TableCellStyle style(const abstract::Document *) const final;

  [[nodiscard]] common::ResolvedStyle
  partial_style(const abstract::Document *) const final;
};

class Span final : public Element, public abstract::Span {
public:
  using Element::Element;

  [[nodiscard]] TextStyle style(const abstract::Document *) const final;
};

class Text final : public Element, public abstract::Text {
public:
  explicit Text(pugi::xml_node node, common::Path document_path,
                const Relations &document_relations);
  Text(pugi::xml_node first, pugi::xml_node last, common::Path document_path,
       const Relations &document_relations);

  [[nodiscard]] std::string content(const abstract::Document *) const final;

  void set_content(const abstract::Document *, const std::string &) final;

  [[nodiscard]] TextStyle style(const abstract::Document *) const final;

private:
  pugi::xml_node m_last;

  static std::string text_(const pugi::xml_node node);
};

class Frame final : public Element, public abstract::Frame {
public:
  Frame(pugi::xml_node node, common::Path document_path,
        const Relations &document_relations);

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

  [[nodiscard]] const common::Path &
  document_path_(const abstract::Document *) const final;
  [[nodiscard]] const Relations &
  document_relations_(const abstract::Document *) const final;

private:
  common::Path m_document_path;
  const Relations &m_document_relations;
};

class ImageElement final : public Element, public abstract::Image {
public:
  using Element::Element;

  [[nodiscard]] bool is_internal(const abstract::Document *) const final;

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *) const final;

  [[nodiscard]] std::string href(const abstract::Document *) const final;
};

} // namespace odr::internal::ooxml::spreadsheet

#endif // ODR_INTERNAL_OOXML_SPREADSHEET_ELEMENT_H
