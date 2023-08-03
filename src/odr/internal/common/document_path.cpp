#include <odr/internal/common/document_path.hpp>

#include <iterator>

namespace odr::internal::common {

const std::string DocumentPath::Child::prefix = "child:";
const std::string DocumentPath::Column::prefix = "column:";
const std::string DocumentPath::Row::prefix = "row:";

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
        component_from_string(string.substr(pos + 1, next - pos - 1)));
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

} // namespace odr::internal::common
