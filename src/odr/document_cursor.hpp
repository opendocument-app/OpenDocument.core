#ifndef ODR_DOCUMENT_CURSOR_H
#define ODR_DOCUMENT_CURSOR_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace odr::internal::abstract {
class Document;
class DocumentCursor;
} // namespace odr::internal::abstract

namespace odr {

class Document;
enum class ElementType;
class Element;

class DocumentCursor final {
public:
  DocumentCursor(const DocumentCursor &other);
  ~DocumentCursor();
  DocumentCursor &operator=(const DocumentCursor &other);

  bool operator==(const DocumentCursor &rhs) const;
  bool operator!=(const DocumentCursor &rhs) const;

  [[nodiscard]] std::string document_path() const;

  [[nodiscard]] ElementType element_type() const;

  bool move_to_parent();
  bool move_to_first_child();
  bool move_to_previous_sibling();
  bool move_to_next_sibling();

  [[nodiscard]] Element element() const;

  [[nodiscard]] bool move_to_master_page();

  [[nodiscard]] bool move_to_first_table_column();
  [[nodiscard]] bool move_to_first_table_row();

  [[nodiscard]] bool move_to_first_sheet_shape();

  void move(const std::string &path);

  using ChildVisitor =
      std::function<void(DocumentCursor &cursor, std::uint32_t i)>;
  using ConditionalChildVisitor =
      std::function<bool(DocumentCursor &cursor, std::uint32_t i)>;

  void for_each_child(const ChildVisitor &visitor);
  void for_each_table_column(const ConditionalChildVisitor &visitor);
  void for_each_table_row(const ConditionalChildVisitor &visitor);
  void for_each_table_cell(const ConditionalChildVisitor &visitor);
  void for_each_sheet_shape(const ChildVisitor &visitor);

private:
  std::shared_ptr<internal::abstract::Document> m_document;
  std::unique_ptr<internal::abstract::DocumentCursor> m_cursor;

  DocumentCursor(std::shared_ptr<internal::abstract::Document> document,
                 std::unique_ptr<internal::abstract::DocumentCursor> cursor);

  void for_each_(const ChildVisitor &visitor);
  void for_each_(const ConditionalChildVisitor &visitor);

  friend Document;
};

} // namespace odr

#endif // ODR_DOCUMENT_CURSOR_H
