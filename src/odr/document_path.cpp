#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
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
  auto colon = string.find(":");
  if (colon == std::string::npos) {
    throw std::invalid_argument("string");
  }

  const std::string prefix = string.substr(0, colon);
  const std::uint32_t number = std::stoul(string.substr(colon + 1));

  if (prefix == Child::prefix_string()) {
    return Child(number);
  } else if (prefix == Column::prefix_string()) {
    return Column(number);
  } else if (prefix == Row::prefix_string()) {
    return Row(number);
  }

  throw std::invalid_argument("string");
}

DocumentPath DocumentPath::extract(Element element) {
  return extract(element, {});
}

DocumentPath DocumentPath::extract(Element element, Element root) {
  std::vector<Component> reverse;

  for (auto current = element; current != root;) {
    auto parent = current.parent();
    if (!parent) {
      break;
    }

    std::uint32_t distance = 0;
    for (; current.previous_sibling(); current = current.previous_sibling()) {
      ++distance;
    }

    if (current.table_column() || current.table_cell()) {
      reverse.push_back(Column(distance));
    } else if (current.table_row()) {
      reverse.push_back(Row(distance));
    } else {
      reverse.push_back(Child(distance));
    }

    current = parent;
  }

  std::reverse(std::begin(reverse), std::end(reverse));
  return DocumentPath(reverse);
}

Element DocumentPath::find(Element root, const DocumentPath & /*path*/) {
  return root; // TODO
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
