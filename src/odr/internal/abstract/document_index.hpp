#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_INDEX_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_INDEX_H

#include <cstdint>

namespace odr {
struct TableDimensions;
} // namespace odr

namespace odr::internal::common {
struct TableRange;
} // namespace odr::internal::common

namespace odr::internal::abstract {
class ELement;
class TableElement;
class TableColumnElement;
class TableRowElement;
class TableCellElement;

class DocumentIndex {
public:
  virtual ~DocumentIndex() = default;

  [[nodiscard]] virtual ElementIndex *first_element() const = 0;
};

class ElementIndex {
public:
  virtual ~ElementIndex() = default;

  [[nodiscard]] virtual Element *element() const = 0;

  [[nodiscard]] virtual ElementIndex *parent() const = 0;
  [[nodiscard]] virtual ElementIndex *first_child() const = 0;
  [[nodiscard]] virtual ElementIndex *previous_sibling() const = 0;
  [[nodiscard]] virtual ElementIndex *next_sibling() const = 0;
};

class TableIndex : public ElementIndex {
public:
  virtual ~TableIndex() = default;

  [[nodiscard]] virtual TableRange index_range() const = 0;

  [[nodiscard]] virtual TableElement *table_element() const = 0;

  [[nodiscard]] virtual ElementIndex *column(std::uint32_t column) const = 0;
  [[nodiscard]] virtual ElementIndex *row(std::uint32_t row) const = 0;
  [[nodiscard]] virtual ElementIndex *cell(std::uint32_t row,
                                           std::uint32_t column) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_INDEX_H
