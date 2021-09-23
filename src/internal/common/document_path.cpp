#include "table_position.h"
#include <internal/common/document_path.h>
#include <internal/util/string_util.h>
#include <stdexcept>

namespace odr::internal::common {

DocumentPath::Child::Child(const std::uint32_t number) : number{number} {}

DocumentPath::Child::Child(const std::string &string) {
  if (!util::string::starts_with(string, "child:")) {
    throw std::invalid_argument("string");
  }
  number = std::stoul(string.substr(6));
}

bool DocumentPath::Child::operator==(const Child &other) const noexcept {
  return number == other.number;
}

bool DocumentPath::Child::operator!=(const Child &other) const noexcept {
  return number != other.number;
}

std::string DocumentPath::Child::to_string() const noexcept {
  return std::string("child:").append(std::to_string(number));
}

DocumentPath::Column::Column(const std::uint32_t number) : number{number} {}

DocumentPath::Column::Column(const std::string &string) {
  if (!util::string::starts_with(string, "column:")) {
    throw std::invalid_argument("string");
  }
  number = TablePosition::to_column_num(string.substr(7));
}

bool DocumentPath::Column::operator==(const Column &other) const noexcept {
  return number == other.number;
}

bool DocumentPath::Column::operator!=(const Column &other) const noexcept {
  return number != other.number;
}

std::string DocumentPath::Column::to_string() const noexcept {
  return std::string("column:").append(TablePosition::to_column_string(number));
}

DocumentPath::Row::Row(const std::uint32_t number) : number{number} {}

DocumentPath::Row::Row(const std::string &string) {
  if (!util::string::starts_with(string, "row:")) {
    throw std::invalid_argument("string");
  }
  number = TablePosition::to_column_num(string.substr(4));
}

bool DocumentPath::Row::operator==(const Row &other) const noexcept {
  return number == other.number;
}

bool DocumentPath::Row::operator!=(const Row &other) const noexcept {
  return number != other.number;
}

std::string DocumentPath::Row::to_string() const noexcept {
  return std::string("row:").append(TablePosition::to_row_string(number));
}

DocumentPath::Cell::Cell(TablePosition position)
    : Cell(position.row(), position.column()) {}

DocumentPath::Cell::Cell(const std::uint32_t row, const std::uint32_t column)
    : row{row}, column{column} {}

DocumentPath::Cell::Cell(const std::string &string) {
  if (!util::string::starts_with(string, "cell:")) {
    throw std::invalid_argument("string");
  }
  TablePosition position(string.substr(5));
  row = position.row();
  column = position.column();
}

bool DocumentPath::Cell::operator==(const Cell &other) const noexcept {
  return (row == other.row) && (column == other.column);
}

bool DocumentPath::Cell::operator!=(const Cell &other) const noexcept {
  return (row != other.row) || (column != other.column);
}

TablePosition DocumentPath::Cell::position() const { return {row, column}; }

std::string DocumentPath::Cell::to_string() const noexcept {
  return std::string("cell:").append(position().to_string());
}

DocumentPath::Component
DocumentPath::component_from_string(const std::string &string) {
  try {
    return Child(string);
  } catch (...) {
  }
  try {
    return Column(string);
  } catch (...) {
  }
  try {
    return Row(string);
  } catch (...) {
  }
  try {
    return Cell(string);
  } catch (...) {
  }
  throw std::invalid_argument("string");
}

DocumentPath::DocumentPath() noexcept = default;

DocumentPath::DocumentPath(const char *c_string)
    : DocumentPath(std::string(c_string)) {}

DocumentPath::DocumentPath(const std::string &string) {
  if (string.empty()) {
    return;
  }

  std::size_t pos = 0;
  while (pos < string.size()) {
    if (string[pos] != '/') {
      throw std::invalid_argument("missing /");
    }
    std::size_t next = string.find('/', pos + 1);
    if (next == std::string::npos) {
      next = string.size();
    }
    m_components.push_back(
        component_from_string(string.substr(pos + 1, next - pos)));
    pos = next;
  }
}

DocumentPath::DocumentPath(Component component) {
  m_components.push_back(component);
}

bool DocumentPath::operator==(const DocumentPath &other) const noexcept {
  return m_components == other.m_components;
}

bool DocumentPath::operator!=(const DocumentPath &other) const noexcept {
  return m_components != other.m_components;
}

DocumentPath::operator std::string() const noexcept { return to_string(); }

std::string DocumentPath::to_string() const noexcept {
  std::string result;

  for (auto &&component : m_components) {
    result.append("/");
    std::visit([&result](auto &&c) { result.append(c.to_string()); },
               component);
  }

  return result;
}

bool DocumentPath::empty() const noexcept { return m_components.empty(); }

DocumentPath DocumentPath::parent() const {
  if (empty()) {
    throw std::invalid_argument("there is no parent");
  }
  DocumentPath result = *this;
  result.m_components.pop_back();
  return result;
}

DocumentPath DocumentPath::join(const DocumentPath &other) const {
  DocumentPath result = *this;
  result.m_components.insert(std::end(result.m_components),
                             std::begin(other.m_components),
                             std::end(other.m_components));
  return result;
}

DocumentPath::const_iterator DocumentPath::begin() const {
  return std::begin(m_components);
}

DocumentPath::const_iterator DocumentPath::end() const {
  return std::end(m_components);
}

} // namespace odr::internal::common
