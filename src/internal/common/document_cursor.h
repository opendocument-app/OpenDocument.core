#ifndef ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
#define ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H

#include <cstdint>
#include <functional>
#include <internal/abstract/document_cursor.h>
#include <internal/common/document_path.h>
#include <internal/common/style.h>
#include <optional>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class Document;
}

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

  bool move_to_master_page() override;

  bool move_to_first_table_column() override;
  bool move_to_first_table_row() override;

  bool move_to_first_sheet_shape() override;

  void move(const common::DocumentPath &path) override;

  [[nodiscard]] const ResolvedStyle &intermediate_style() const;

protected:
  const abstract::Document *m_document;

  void *reset_current_(std::size_t size);
  void push_style_(const ResolvedStyle &style);

  virtual void pushed_(abstract::Element *element);
  virtual void popping_(abstract::Element *element);

  [[nodiscard]] virtual ResolvedStyle partial_style() const;

private:
  std::vector<std::int32_t> m_parent_element_stack_top;
  std::string m_parent_element_stack;
  std::string m_current_element;
  std::string m_temporary_element;

  std::vector<ResolvedStyle> m_style_stack;

  DocumentPath m_parent_path;
  std::optional<DocumentPath::Component> m_current_component;

  [[nodiscard]] abstract::Element *temporary_();
  [[nodiscard]] abstract::Element *parent_();

  [[nodiscard]] std::int32_t parent_next_offset_() const;
  [[nodiscard]] std::int32_t parent_back_offset_() const;

  void swap_current_temporary();

  void *reset_temporary_(std::size_t size);
  void *push_parent_(std::size_t size);
  void pop_parent_();
  void pop_style_();
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
