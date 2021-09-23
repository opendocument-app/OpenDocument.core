#ifndef ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
#define ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H

#include <functional>
#include <internal/abstract/document.h>
#include <internal/common/document_path.h>
#include <internal/common/style.h>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace odr::internal::common {

class DocumentCursor : public abstract::DocumentCursor {
public:
  explicit DocumentCursor(const abstract::Document *document);
  ~DocumentCursor() override;

  [[nodiscard]] bool
  equals(const abstract::DocumentCursor &other) const override;

  [[nodiscard]] DocumentPath document_path() const override;

  [[nodiscard]] abstract::Element *element() override;
  [[nodiscard]] const abstract::Element *element() const override;

  bool move_to_parent() override;
  bool move_to_first_child() override;
  bool move_to_previous_sibling() override;
  bool move_to_next_sibling() override;

  [[nodiscard]] bool move_to_master_page() override;

  [[nodiscard]] bool move_to_first_table_column() override;
  [[nodiscard]] bool move_to_first_table_row() override;

  [[nodiscard]] bool move_to_first_sheet_shape() override;

  [[nodiscard]] const ResolvedStyle &intermediate_style() const;

protected:
  const abstract::Document *m_document;

  void *push_(std::size_t size);

  virtual void pushed_(abstract::Element *element);
  virtual void popping_(abstract::Element *element);

  abstract::Element *back_();
  [[nodiscard]] const abstract::Element *back_() const;

  [[nodiscard]] virtual ResolvedStyle partial_style() const;

private:
  std::vector<std::int32_t> m_element_stack_top;
  std::string m_element_stack;
  std::vector<ResolvedStyle> m_style_stack;

  DocumentPath m_parent_path;
  std::optional<DocumentPath::Component> m_current_component;

  [[nodiscard]] std::int32_t next_offset_() const;
  [[nodiscard]] std::int32_t back_offset_() const;

  void pop_();
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
