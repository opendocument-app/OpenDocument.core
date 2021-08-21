#ifndef ODR_TABLE_H
#define ODR_TABLE_H

#include <cstdint>
#include <memory>
#include <odr/any_thing.h>
#include <odr/property.h>

namespace odr::internal::abstract {
class Table;
}

namespace odr {

class Element;
class TableElement;

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

class Table : public AnyThing<internal::abstract::Table> {
public:
  Table();

  template <typename Derived,
            typename = typename std::enable_if<std::is_base_of<
                internal::abstract::Table, Derived>::value>::type>
  explicit Table(Derived &&derived) : Base{derived} {}

  template <typename Derived,
            typename = typename std::enable_if<std::is_base_of<
                internal::abstract::Table, Derived>::value>::type>
  explicit Table(Derived &derived) : Base{derived} {}

  [[nodiscard]] TableDimensions dimensions() const;
  [[nodiscard]] TableDimensions content_bounds() const;
  [[nodiscard]] TableDimensions content_bounds(TableDimensions within) const;

  [[nodiscard]] Element column(std::uint32_t column) const;
  [[nodiscard]] Element row(std::uint32_t row) const;
  [[nodiscard]] Element cell(std::uint32_t row, std::uint32_t column) const;
};

} // namespace odr

#endif // ODR_TABLE_H
