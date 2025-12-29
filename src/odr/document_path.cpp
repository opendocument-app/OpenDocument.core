#include <odr/document_path.hpp>

#include <stdexcept>

namespace odr {

const std::string &DocumentPath::Child::prefix_string() {
  static std::string result(prefix);
  return result;
}

DocumentPath::Child::Child(const std::uint32_t number) : m_number{number} {}

std::uint32_t DocumentPath::Child::number() const { return m_number; }

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

DocumentPath::Cell::Cell(const TablePosition &position)
    : m_position{position} {}

TablePosition DocumentPath::Cell::position() const { return m_position; }

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

const DocumentPath::Component &DocumentPath::back() const {
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
