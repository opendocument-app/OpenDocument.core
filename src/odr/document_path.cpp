#include <odr/document_path.hpp>

#include <odr/document_element.hpp>

#include <algorithm>
#include <stdexcept>

namespace odr {

template <typename Derived>
const std::string &DocumentPath::ComponentTemplate<Derived>::prefix_string() {
  static std::string result(Derived::prefix);
  return result;
}

template <typename Derived>
DocumentPath::ComponentTemplate<Derived>::ComponentTemplate(
    const std::uint32_t number)
    : number{number} {}

template <typename Derived>
bool DocumentPath::ComponentTemplate<Derived>::operator==(
    const ComponentTemplate &other) const noexcept {
  return number == other.number;
}

template <typename Derived>
bool DocumentPath::ComponentTemplate<Derived>::operator!=(
    const ComponentTemplate &other) const noexcept {
  return number != other.number;
}

template <typename Derived>
Derived &DocumentPath::ComponentTemplate<Derived>::operator++() {
  ++number;
  return *this;
}

template <typename Derived>
Derived &DocumentPath::ComponentTemplate<Derived>::operator--() {
  --number;
  return *this;
}

template <typename Derived>
std::string
DocumentPath::ComponentTemplate<Derived>::to_string() const noexcept {
  return prefix_string() + ":" + std::to_string(number);
}

DocumentPath::Component
DocumentPath::component_from_string(const std::string &string) {
  const auto colon = string.find(':');
  if (colon == std::string::npos) {
    throw std::invalid_argument("string");
  }

  const std::string prefix = string.substr(0, colon);
  const std::uint32_t number = std::stoul(string.substr(colon + 1));

  if (prefix == Child::prefix_string()) {
    return Child(number);
  }
  if (prefix == Column::prefix_string()) {
    return Column(number);
  }
  if (prefix == Row::prefix_string()) {
    return Row(number);
  }

  throw std::invalid_argument("string");
}

DocumentPath DocumentPath::extract(const Element element) {
  return extract(element, {});
}

DocumentPath DocumentPath::extract(const Element element, const Element root) {
  std::vector<Component> reverse;

  for (auto current = element; current != root;) {
    const auto parent = current.parent();
    if (!parent) {
      break;
    }

    std::uint32_t distance = 0;
    for (; current.previous_sibling(); current = current.previous_sibling()) {
      ++distance;
    }

    if (current.as_table_column() || current.as_table_cell()) {
      reverse.emplace_back(Column(distance));
    } else if (current.as_table_row()) {
      reverse.emplace_back(Row(distance));
    } else {
      reverse.emplace_back(Child(distance));
    }

    current = parent;
  }

  std::ranges::reverse(reverse);
  return DocumentPath(reverse);
}

Element DocumentPath::find(const Element root, const DocumentPath &path) {
  Element element = root;

  for (const Component &c : path) {
    std::uint32_t number = 0;
    if (const auto child = std::get_if<Child>(&c)) {
      if (!element.first_child()) {
        throw std::invalid_argument("child not found");
      }
      element = element.first_child();
      number = child->number;
    } else if (const auto column = std::get_if<Column>(&c)) {
      if (!element.as_table().first_column()) {
        throw std::invalid_argument("column not found");
      }
      element = Element(element.as_table().first_column());
      number = column->number;
    } else if (const auto row = std::get_if<Row>(&c)) {
      if (!element.as_table().first_row()) {
        throw std::invalid_argument("row not found");
      }
      element = Element(element.as_table().first_row());
      number = row->number;
    } else {
      throw std::invalid_argument("unknown component");
    }
    for (std::uint32_t i = 0; i < number; ++i) {
      if (!element.next_sibling()) {
        throw std::invalid_argument("sibling not found");
      }
      element = element.next_sibling();
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
