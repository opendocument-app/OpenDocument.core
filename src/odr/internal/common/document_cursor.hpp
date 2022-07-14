#ifndef ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
#define ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H

#include <cstdint>
#include <functional>
#include <odr/internal/abstract/document_cursor.hpp>
#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/common/document_path.hpp>
#include <odr/internal/common/style.hpp>
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
  DocumentCursor(const DocumentCursor &other);

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

  void push_element_(std::unique_ptr<abstract::Element> element);
  void push_style_(const ResolvedStyle &style);

  virtual void pushed_(abstract::Element *element);
  virtual void popping_(abstract::Element *element);

  [[nodiscard]] virtual ResolvedStyle partial_style() const;

private:
  std::vector<std::unique_ptr<abstract::Element>> m_element_stack;
  std::vector<ResolvedStyle> m_style_stack;

  DocumentPath m_parent_path;
  std::optional<DocumentPath::Component> m_current_component;

  void pop_style_();
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
