#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_CURSOR_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_CURSOR_H

#include <memory>

namespace odr::internal::common {
class DocumentPath;
} // namespace odr::internal::common

namespace odr::internal::abstract {
class Element;

class DocumentCursor {
public:
  virtual ~DocumentCursor() = default;

  [[nodiscard]] virtual bool equals(const DocumentCursor &other) const = 0;

  [[nodiscard]] virtual std::unique_ptr<DocumentCursor> copy() const = 0;

  [[nodiscard]] virtual common::DocumentPath document_path() const = 0;

  [[nodiscard]] virtual Element *element() = 0;
  [[nodiscard]] virtual const Element *element() const = 0;

  virtual bool move_to_parent() = 0;
  virtual bool move_to_first_child() = 0;
  virtual bool move_to_previous_sibling() = 0;
  virtual bool move_to_next_sibling() = 0;

  virtual bool move_to_master_page() = 0;

  virtual bool move_to_first_table_column() = 0;
  virtual bool move_to_first_table_row() = 0;

  virtual bool move_to_first_sheet_shape() = 0;

  virtual void move(const common::DocumentPath &path) = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_CURSOR_H
