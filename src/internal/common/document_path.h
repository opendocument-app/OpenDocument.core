#ifndef ODR_INTERNAL_COMMON_DOCUMENT_PATH_H
#define ODR_INTERNAL_COMMON_DOCUMENT_PATH_H

#include <internal/util/string_util.h>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::common {
class TablePosition;

class DocumentPath final {
public:
  template <typename Component> struct ComponentTemplate {
    std::uint32_t number{0};

    ComponentTemplate(const std::uint32_t number) : number{number} {}
    ComponentTemplate(const std::string &string) {
      if (!util::string::starts_with(string, Component::prefix)) {
        throw std::invalid_argument("string");
      }
      number = std::stoul(string.substr(Component::prefix.size()));
    }

    bool operator==(const ComponentTemplate &other) const noexcept {
      return number == other.number;
    }
    bool operator!=(const ComponentTemplate &other) const noexcept {
      return number != other.number;
    }

    ComponentTemplate &operator++() {
      ++number;
      return *this;
    }

    ComponentTemplate &operator--() {
      --number;
      return *this;
    }

    [[nodiscard]] std::string to_string() const noexcept {
      return Component::prefix + std::to_string(number);
    }
  };

  struct Child final : public ComponentTemplate<Child> {
    static const std::string prefix;
    using ComponentTemplate::ComponentTemplate;
  };

  struct Column final : public ComponentTemplate<Column> {
    static const std::string prefix;
    using ComponentTemplate::ComponentTemplate;
  };

  struct Row final : public ComponentTemplate<Row> {
    static const std::string prefix;
    using ComponentTemplate::ComponentTemplate;
  };

  using Component = std::variant<Child, Column, Row>;

  static Component component_from_string(const std::string &string);

  DocumentPath() noexcept;
  DocumentPath(const char *c_string);
  DocumentPath(const std::string &string);
  DocumentPath(Component component);

  bool operator==(const DocumentPath &other) const noexcept;
  bool operator!=(const DocumentPath &other) const noexcept;

  operator std::string() const noexcept;

  [[nodiscard]] std::string to_string() const noexcept;

  [[nodiscard]] bool empty() const noexcept;

  [[nodiscard]] Component back() const;
  [[nodiscard]] DocumentPath parent() const;
  [[nodiscard]] DocumentPath join(const DocumentPath &other) const;

  using Container = std::vector<Component>;
  using const_iterator = Container::const_iterator;

  [[nodiscard]] const_iterator begin() const;
  [[nodiscard]] const_iterator end() const;

private:
  Container m_components;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_PATH_H
