#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT2_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT2_H

#include <any>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace odr::experimental {
enum class DocumentType;
struct DocumentMeta;
enum class ElementType;
} // namespace odr::experimental

namespace odr::internal::common {
class Path;
}

namespace odr::internal::abstract {
class File;

template <typename Number, typename Tag> struct Identifier {
  Identifier() = default;
  Identifier(Number id) : id{id} {}

  operator bool() const { return id == 0; }
  operator Number() const { return id; }

  Number id{0};
};

struct element_identifier_tag {};

using ElementIdentifier = Identifier<std::uint64_t, element_identifier_tag>;

enum class DocumentElementProperty {
  NAME,
  NOTES,
  TEXT,
  HREF,
  ANCHOR_TYPE,

  X,
  Y,
  WIDTH,
  HEIGHT,

  X1,
  Y1,
  X2,
  Y2,

  Z_INDEX,

  TABLE_ROW_COUNT,
  TABLE_COL_COUNT,
  TABLE_CELL_ROW_SPAN,
  TABLE_CELL_COL_SPAN,

  MARGIN_TOP,
  MARGIN_BOTTOM,
  MARGIN_LEFT,
  MARGIN_RIGHT,

  PADDING_TOP,
  PADDING_BOTTOM,
  PADDING_LEFT,
  PADDING_RIGHT,

  BORDER_TOP,
  BORDER_BOTTOM,
  BORDER_LEFT,
  BORDER_RIGHT,

  PRINT_ORIENTATION,

  FONT_NAME,
  FONT_SIZE,
  FONT_WEIGHT,
  FONT_STYLE,
  FONT_COLOR,

  BACKGROUND_COLOR,

  TEXT_ALIGN,

  STROKE_WIDTH,
  STROKE_COLOR,
  FILL_COLOR,
  VERTICAL_ALIGN,

  IMAGE_INTERNAL,
};

class Document2 {
public:
  virtual ~Document2() = default;

  [[nodiscard]] virtual bool editable() const noexcept = 0;
  [[nodiscard]] virtual bool savable(bool encrypted) const noexcept = 0;

  virtual void save(const common::Path &path) const = 0;
  virtual void save(const common::Path &path,
                    const std::string &password) const = 0;

  [[nodiscard]] virtual experimental::DocumentType
  document_type() const noexcept = 0;
  [[nodiscard]] virtual experimental::DocumentMeta
  document_meta() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t entry_count() const = 0;
  [[nodiscard]] virtual ElementIdentifier root_element() const = 0;

  [[nodiscard]] virtual experimental::ElementType
  element_type(ElementIdentifier elementId) const = 0;

  [[nodiscard]] virtual ElementIdentifier
  element_parent(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  element_first_child(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  element_previous_sibling(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  element_next_sibling(ElementIdentifier elementId) const = 0;

  [[nodiscard]] virtual std::any
  element_property(ElementIdentifier elementId,
                   DocumentElementProperty property) const = 0;
  [[nodiscard]] virtual std::string
  element_string_property(ElementIdentifier elementId,
                          DocumentElementProperty property) const = 0;
  [[nodiscard]] virtual std::uint32_t
  element_uint32_property(ElementIdentifier elementId,
                          DocumentElementProperty property) const = 0;
  [[nodiscard]] virtual bool
  element_bool_property(ElementIdentifier elementId,
                        DocumentElementProperty property) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  element_optional_string_property(ElementIdentifier elementId,
                                   DocumentElementProperty property) const = 0;

  [[nodiscard]] virtual std::shared_ptr<File>
  image_file(ElementIdentifier elementId) const = 0;

  virtual void set_element_property(ElementIdentifier elementId,
                                    DocumentElementProperty property,
                                    const std::any &value) const = 0;
  virtual void set_element_string_property(ElementIdentifier elementId,
                                           DocumentElementProperty property,
                                           const std::string &value) const = 0;

  virtual void
  remove_element_property(ElementIdentifier elementId,
                          DocumentElementProperty property) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT2_H
