#ifndef ODR_INTERNAL_ABSTRACT_TABLE_H
#define ODR_INTERNAL_ABSTRACT_TABLE_H

#include <cstdint>

namespace odr {
class Element;
struct TableDimensions;
} // namespace odr

namespace odr::internal::common {
class TableRange;
} // namespace odr::internal::common

namespace odr::internal::abstract {

class Table {
public:
  virtual ~Table() = default;

  [[nodiscard]] virtual TableDimensions dimensions() const = 0;

  [[nodiscard]] virtual common::TableRange content_bounds() const = 0;
  [[nodiscard]] virtual common::TableRange
  content_bounds(common::TableRange within) const = 0;

  [[nodiscard]] virtual odr::Element column(std::uint32_t column) const = 0;

  [[nodiscard]] virtual odr::Element row(std::uint32_t row) const = 0;

  [[nodiscard]] virtual odr::Element cell(std::uint32_t row,
                                          std::uint32_t column) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_TABLE_H
