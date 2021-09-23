#ifndef ODR_INTERNAL_COMMON_DOCUMENT_PATH_H
#define ODR_INTERNAL_COMMON_DOCUMENT_PATH_H

#include <string>
#include <variant>
#include <vector>

namespace odr::internal::common {
class TablePosition;

class DocumentPath final {
public:
  struct Child final {
    std::uint32_t number{0};

    Child(std::uint32_t number);
    Child(const std::string &string);

    bool operator==(const Child &other) const noexcept;
    bool operator!=(const Child &other) const noexcept;

    std::string to_string() const noexcept;
  };

  struct Column final {
    std::uint32_t number;

    Column(std::uint32_t number);
    Column(const std::string &string);

    bool operator==(const Column &other) const noexcept;
    bool operator!=(const Column &other) const noexcept;

    std::string to_string() const noexcept;
  };

  struct Row final {
    std::uint32_t number;

    Row(std::uint32_t number);
    Row(const std::string &string);

    bool operator==(const Row &other) const noexcept;
    bool operator!=(const Row &other) const noexcept;

    std::string to_string() const noexcept;
  };

  struct Cell final {
    std::uint32_t row;
    std::uint32_t column;

    Cell(TablePosition position);
    Cell(std::uint32_t row, std::uint32_t column);
    Cell(const std::string &string);

    bool operator==(const Cell &other) const noexcept;
    bool operator!=(const Cell &other) const noexcept;

    TablePosition position() const;

    std::string to_string() const noexcept;
  };

  using Component = std::variant<Child, Column, Row, Cell>;

  Component component_from_string(const std::string &string);

  DocumentPath() noexcept;
  DocumentPath(const char *c_string);
  DocumentPath(const std::string &string);
  DocumentPath(Component component);

  bool operator==(const DocumentPath &other) const noexcept;
  bool operator!=(const DocumentPath &other) const noexcept;

  operator std::string() const noexcept;

  [[nodiscard]] std::string to_string() const noexcept;

  [[nodiscard]] bool empty() const noexcept;

  [[nodiscard]] DocumentPath parent() const;
  [[nodiscard]] DocumentPath join(const DocumentPath &other) const;

  using Container = std::vector<std::variant<Child, Column, Row, Cell>>;
  using const_iterator = Container::const_iterator;

  const_iterator begin() const;
  const_iterator end() const;

private:
  Container m_components;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_PATH_H
