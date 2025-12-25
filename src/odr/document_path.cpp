#include <odr/document_path.hpp>

#include <odr/document_element.hpp>

#include <algorithm>
#include <stdexcept>

namespace odr {

const std::string &DocumentPath::Child::prefix_string() {
  static std::string result(prefix);
  return result;
}

std::pair<Element, DocumentPath::Child>
DocumentPath::Child::extract(const Element element) {
  if (!element) {
    throw std::invalid_argument("element is null");
  }
  const Element parent = element.parent();
  if (!parent) {
    throw std::invalid_argument("parent not found");
  }

  Element current = element;
  std::uint32_t distance = 0;
  for (; current.previous_sibling(); current = current.previous_sibling()) {
    ++distance;
  }

  return {parent, Child(distance)};
}

Element DocumentPath::Child::resolve(const Element element,
                                     const Child &child) {
  if (!element) {
    throw std::invalid_argument("element is null");
  }
  if (!element.first_child()) {
    throw std::invalid_argument("child not found");
  }
  Element result = element.first_child();
  for (std::uint32_t i = 0; i < child.m_number; ++i) {
    if (!result.next_sibling()) {
      throw std::invalid_argument("sibling not found");
    }
    result = result.next_sibling();
  }
  return result;
}

DocumentPath::Child::Child(const std::uint32_t number) : m_number{number} {}

bool DocumentPath::Child::operator==(const Child &other) const noexcept {
  return m_number == other.m_number;
}

[[nodiscard]] std::string DocumentPath::Child::to_string() const noexcept {
  return prefix_string() + ":" + std::to_string(m_number);
}

const std::string &DocumentPath::Cell::prefix_string() {
  static std::string result(prefix);
  return result;
}

std::pair<Sheet, DocumentPath::Cell>
DocumentPath::Cell::extract(const SheetCell &element) {
  if (!element) {
    throw std::invalid_argument("element is null");
  }
  const Element parent = element.parent();
  if (!parent) {
    throw std::invalid_argument("parent not found");
  }
  const Sheet sheet = parent.as_sheet();
  if (!sheet) {
    throw std::invalid_argument("parent is not a sheet");
  }
  const SheetCell cell = element.as_sheet_cell();
  if (!cell) {
    throw std::invalid_argument("element is not a sheet cell");
  }
  return {sheet, Cell(cell.position())};
}

SheetCell DocumentPath::Cell::resolve(const Sheet &element, const Cell &cell) {
  if (!element) {
    throw std::invalid_argument("element is null");
  }
  return element.as_sheet().cell(cell.m_position.column(),
                                 cell.m_position.row());
}

DocumentPath::Cell::Cell(const TablePosition &position)
    : m_position{position} {}

bool DocumentPath::Cell::operator==(const Cell &other) const noexcept {
  return m_position == other.m_position;
}

[[nodiscard]] std::string DocumentPath::Cell::to_string() const noexcept {
  return prefix_string() + ":" + m_position.to_string();
}

DocumentPath::Component
DocumentPath::component_from_string(const std::string &string) {
  const auto colon = string.find(':');
  if (colon == std::string::npos) {
    throw std::invalid_argument("string");
  }

  const std::string prefix = string.substr(0, colon);

  if (prefix == Child::prefix_string()) {
    const std::uint32_t number = std::stoul(string.substr(colon + 1));
    return Child(number);
  }
  if (prefix == Cell::prefix_string()) {
    const TablePosition position(string.substr(colon + 1));
    return Cell(position);
  }

  throw std::invalid_argument("string");
}

DocumentPath DocumentPath::extract(const Element element) {
  return extract(element, {});
}

DocumentPath DocumentPath::extract(const Element element, const Element root) {
  std::vector<Component> reverse;

  for (auto current = element; current != root;) {
    if (!current.parent()) {
      break;
    }

    if (const SheetCell sheet_cell = element.as_sheet_cell(); sheet_cell) {
      const auto [parent, cell] = Cell::extract(sheet_cell);
      reverse.emplace_back(cell);
      current = static_cast<Element>(parent);
    } else {
      const auto [parent, child] = Child::extract(current);
      reverse.emplace_back(child);
      current = parent;
    }
  }

  std::ranges::reverse(reverse);
  return DocumentPath(reverse);
}

Element DocumentPath::resolve(const Element root, const DocumentPath &path) {
  Element element = root;

  for (const Component &c : path) {
    if (const auto *child = std::get_if<Child>(&c); child != nullptr) {
      element = Child::resolve(element, *child);
    } else if (const auto *cell = std::get_if<Cell>(&c); cell != nullptr) {
      const Sheet sheet = element.as_sheet();
      if (!sheet) {
        throw std::invalid_argument("element is not a sheet");
      }
      const SheetCell sheet_cell = Cell::resolve(sheet, *cell);
      element = static_cast<Element>(sheet_cell);
    } else {
      throw std::invalid_argument("unknown component");
    }
    if (!element) {
      throw std::invalid_argument("element not found");
    }
  }

  return element;
}

DocumentPath::DocumentPath() noexcept = default;

DocumentPath::DocumentPath(const Container &components)
    : m_components{components} {}

DocumentPath::DocumentPath(Container &&components)
    : m_components{std::move(components)} {}

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
        component_from_string(string.substr(pos + 1, next - pos - 1)));
    pos = next;
  }
}

bool DocumentPath::operator==(const DocumentPath &other) const noexcept {
  return m_components == other.m_components;
}

bool DocumentPath::operator!=(const DocumentPath &other) const noexcept {
  return m_components != other.m_components;
}

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

DocumentPath::Component DocumentPath::back() const {
  return m_components.back();
}

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

} // namespace odr
