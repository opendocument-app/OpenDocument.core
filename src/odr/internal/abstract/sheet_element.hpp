#ifndef ODR_INTERNAL_ABSTRACT_SHEET_ELEMENT_H
#define ODR_INTERNAL_ABSTRACT_SHEET_ELEMENT_H

#include <odr/internal/abstract/document_element.hpp>

namespace odr::internal::abstract {
class SheetColumn;
class SheetRow;
class SheetCell;

class Sheet : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::sheet;
  }

  [[nodiscard]] virtual std::string name(const Document *) const = 0;

  [[nodiscard]] virtual TableDimensions dimensions(const Document *) const = 0;
  [[nodiscard]] virtual TableDimensions
  content(const Document *, std::optional<TableDimensions> range) const = 0;

  [[nodiscard]] virtual SheetColumn *column(const Document *,
                                            std::uint32_t column) const = 0;
  [[nodiscard]] virtual SheetRow *row(const Document *,
                                      std::uint32_t row) const = 0;
  [[nodiscard]] virtual SheetCell *cell(const Document *, std::uint32_t column,
                                        std::uint32_t row) const = 0;

  [[nodiscard]] virtual Element *first_shape(const Document *) const = 0;

  [[nodiscard]] virtual TableStyle style(const Document *) const = 0;
};

class SheetColumn : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_column;
  }

  [[nodiscard]] virtual TableColumnStyle style(const Document *, Sheet *,
                                               std::uint32_t column) const = 0;
};

class SheetRow : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_row;
  }

  [[nodiscard]] virtual TableRowStyle style(const Document *, Sheet *,
                                            std::uint32_t row) const = 0;
};

class SheetCell : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_cell;
  }

  [[nodiscard]] virtual bool is_covered(const Document *, Sheet *,
                                        std::uint32_t column,
                                        std::uint32_t row) const = 0;
  [[nodiscard]] virtual TableDimensions span(const Document *, Sheet *,
                                             std::uint32_t column,
                                             std::uint32_t row) const = 0;
  [[nodiscard]] virtual ValueType value_type(const Document *, Sheet *,
                                             std::uint32_t column,
                                             std::uint32_t row) const = 0;

  [[nodiscard]] virtual TableCellStyle style(const Document *, Sheet *,
                                             std::uint32_t column,
                                             std::uint32_t row) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_SHEET_ELEMENT_H
