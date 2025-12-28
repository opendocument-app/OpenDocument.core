#pragma once

#include <odr/table_position.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace odr {
class Document;
class Element;
class Sheet;
class SheetCell;

/// @brief A path to a specific element in a document.
class DocumentPath final {
public:
  class Child final {
  public:
    static constexpr std::string_view prefix = "child";

    static const std::string &prefix_string();
    static std::pair<Element, Child> extract(Element element);
    static Element resolve(Element element, const Child &child);

    explicit Child(std::uint32_t number);

    bool operator==(const Child &other) const noexcept;
    [[nodiscard]] std::string to_string() const noexcept;

  private:
    std::uint32_t m_number{0};
  };

  class Cell final {
  public:
    static constexpr std::string_view prefix = "cell";

    static const std::string &prefix_string();
    static std::pair<Sheet, Cell> extract(const SheetCell &element);
    static SheetCell resolve(const Sheet &element, const Cell &cell);

    explicit Cell(const TablePosition &position);

    bool operator==(const Cell &other) const noexcept;
    [[nodiscard]] std::string to_string() const noexcept;

  private:
    TablePosition m_position;
  };

  using Component = std::variant<Child, Cell>;
  using Container = std::vector<Component>;
  using const_iterator = Container::const_iterator;

  static Component component_from_string(const std::string &string);
  static DocumentPath extract(Element element);
  static DocumentPath extract(Element element, Element root);
  static Element resolve(Element root, const DocumentPath &path);

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
