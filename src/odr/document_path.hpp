#ifndef ODR_DOCUMENT_PATH_H
#define ODR_DOCUMENT_PATH_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace odr {
class Document;
class Element;

class DocumentPath final {
public:
  template <typename Derived> struct ComponentTemplate {
    static const std::string &prefix_string();

    std::uint32_t number{0};

    ComponentTemplate(const std::uint32_t number);

    bool operator==(const ComponentTemplate &other) const noexcept;
    bool operator!=(const ComponentTemplate &other) const noexcept;

    Derived &operator++();
    Derived &operator--();

    [[nodiscard]] std::string to_string() const noexcept;
  };

  struct Child final : public ComponentTemplate<Child> {
    static constexpr std::string_view prefix = "child";
    using ComponentTemplate::ComponentTemplate;
  };

  struct Column final : public ComponentTemplate<Column> {
    static constexpr std::string_view prefix = "column";
    using ComponentTemplate::ComponentTemplate;
  };

  struct Row final : public ComponentTemplate<Row> {
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
  DocumentPath(const Container &components);
  DocumentPath(Container &&components);
  DocumentPath(const char *c_string);
  DocumentPath(const std::string &string);

  bool operator==(const DocumentPath &other) const noexcept;
  bool operator!=(const DocumentPath &other) const noexcept;

  operator std::string() const noexcept;

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

#endif // ODR_INTERNAL_COMMON_DOCUMENT_PATH_H
