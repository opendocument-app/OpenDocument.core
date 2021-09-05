#ifndef ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
#define ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H

#include <functional>
#include <internal/abstract/document.h>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace odr::internal::common {

class DocumentCursor : public abstract::DocumentCursor {
public:
  explicit DocumentCursor(const abstract::Document *document);

  [[nodiscard]] bool equals(const abstract::DocumentCursor &other) const final;

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;

  [[nodiscard]] std::string document_path() const final;

  [[nodiscard]] abstract::Element *element() final;
  [[nodiscard]] const abstract::Element *element() const final;

  bool move_to_parent() final;
  bool move_to_first_child() final;
  bool move_to_previous_sibling() final;
  bool move_to_next_sibling() final;

  [[nodiscard]] bool move_to_master_page() final;

  [[nodiscard]] bool move_to_first_table_column() final;
  [[nodiscard]] bool move_to_first_table_row() final;

protected:
  void *push_(std::size_t size);
  void pop_();
  [[nodiscard]] std::int32_t next_offset_() const;
  [[nodiscard]] std::int32_t back_offset_() const;
  abstract::Element *back_();
  [[nodiscard]] const abstract::Element *back_() const;

  const abstract::Document *m_document;

private:
  std::vector<std::int32_t> m_element_stack_top;
  std::string m_element_stack;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
