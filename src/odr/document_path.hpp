#pragma once

#include <string>
#include <variant>
#include <vector>

namespace odr {
class Document;
class Element;

/// @brief A path to a specific element in a document.
class DocumentPath final {
public:
  template <typename Derived> struct ComponentTemplate {
    static const std::string &prefix_string();

    std::uint32_t number{0};

    explicit ComponentTemplate(std::uint32_t number);

    bool operator==(const ComponentTemplate &other) const noexcept;
    bool operator!=(const ComponentTemplate &other) const noexcept;

    Derived &operator++();
    Derived &operator--();

    [[nodiscard]] std::string to_string() const noexcept;
  };

  struct Child final : ComponentTemplate<Child> {
    static constexpr std::string_view prefix = "child";
    using ComponentTemplate::ComponentTemplate;
  };

  struct Column final : ComponentTemplate<Column> {
    static constexpr std::string_view prefix = "column";
    using ComponentTemplate::ComponentTemplate;
  };

  struct Row final : ComponentTemplate<Row> {
    static constexpr std::string_view prefix = "row";
    using ComponentTemplate::ComponentTemplate;
  };

  using Component = std::variant<Child, Column, Row>;
  using Container = std::vector<Component>;
  using const_iterator = Container::const_iterator;

  static Component component_from_string(const std::string &string);
  static DocumentPath extract(Element element);
  static DocumentPath extract(Element element, Element root);
  static Element find(Element root, const DocumentPath &path);

  DocumentPath() noexcept;
  explicit DocumentPath(const Container &components);
  explicit DocumentPath(Container &&components);
  explicit DocumentPath(const char *c_string);
  explicit DocumentPath(const std::string &string);

  bool operator==(const DocumentPath &other) const noexcept;
  bool operator!=(const DocumentPath &other) const noexcept;

  [[nodiscard]] std::string to_string() const noexcept;

  [[nodiscard]] bool empty() const noexcept;

  [[nodiscard]] Component back() const;
  [[nodiscard]] DocumentPath parent() const;
  [[nodiscard]] DocumentPath join(const DocumentPath &other) const;

  [[nodiscard]] const_iterator begin() const;
  [[nodiscard]] const_iterator end() const;

private:
  Container m_components;
};

} // namespace odr
