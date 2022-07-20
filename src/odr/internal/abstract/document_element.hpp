#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_ELEMENT_H

#include <memory>
#include <odr/document_element.hpp>
#include <optional>
#include <string>

namespace odr {
class File;

enum class ElementType;
struct TextStyle;
struct ParagraphStyle;
struct TableStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableCellStyle;
struct GraphicStyle;
struct PageLayout;
struct TableDimensions;
} // namespace odr

namespace odr::internal::abstract {
class Document;

class Element {
public:
  virtual ~Element() = default;

  [[nodiscard]] virtual ElementType type(const Document *) const = 0;

  [[nodiscard]] virtual Element *parent(const Document *) const = 0;
  [[nodiscard]] virtual Element *first_child(const Document *) const = 0;
  [[nodiscard]] virtual Element *previous_sibling(const Document *) const = 0;
  [[nodiscard]] virtual Element *next_sibling(const Document *) const = 0;
};

class TextRootElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::root;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  first_master_page(const Document *document) const = 0;
};

class SlideElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::slide;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  master_page(const Document *document) const = 0;

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;
};

class SheetElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::sheet;
  }

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;

  [[nodiscard]] virtual TableDimensions
  dimensions(const Document *document) const = 0;
  [[nodiscard]] virtual TableDimensions
  content(const Document *document,
          std::optional<TableDimensions> range) const = 0;

  [[nodiscard]] virtual Element *column(const Document *document,
                                        std::uint32_t column) const = 0;
  [[nodiscard]] virtual Element *row(const Document *document,
                                     std::uint32_t column) const = 0;
  [[nodiscard]] virtual Element *cell(const Document *document,
                                      std::uint32_t column) const = 0;

  [[nodiscard]] virtual Element *
  first_shape(const Document *document) const = 0;

  [[nodiscard]] virtual TableStyle style(const Document *document) const = 0;
};

class PageElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::page;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  master_page(const Document *document) const = 0;

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;
};

class MasterPageElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::master_page;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;
};

class LineBreakElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::line_break;
  }

  [[nodiscard]] virtual TextStyle style(const Document *document) const = 0;
};

class ParagraphElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::paragraph;
  }

  [[nodiscard]] virtual ParagraphStyle
  style(const Document *document) const = 0;
  [[nodiscard]] virtual TextStyle
  text_style(const Document *document) const = 0;
};

class SpanElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::span;
  }

  [[nodiscard]] virtual TextStyle style(const Document *document) const = 0;
};

class TextElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::text;
  }

  [[nodiscard]] virtual std::string content(const Document *document) const = 0;
  virtual void set_content(const Document *document,
                           const std::string &text) = 0;

  [[nodiscard]] virtual TextStyle style(const Document *document) const = 0;
};

class LinkElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::link;
  }

  [[nodiscard]] virtual std::string href(const Document *document) const = 0;
};

class BookmarkElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::bookmark;
  }

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;
};

class ListItemElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::list_item;
  }

  [[nodiscard]] virtual TextStyle style(const Document *document) const = 0;
};

class TableElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table;
  }

  [[nodiscard]] virtual TableDimensions
  dimensions(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  first_column(const Document *document) const = 0;
  [[nodiscard]] virtual Element *first_row(const Document *document) const = 0;

  [[nodiscard]] virtual TableStyle style(const Document *document) const = 0;
};

class TableColumnElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_column;
  }

  [[nodiscard]] virtual TableColumnStyle
  style(const Document *document) const = 0;
};

class TableRowElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_row;
  }

  [[nodiscard]] virtual TableRowStyle style(const Document *document) const = 0;
};

class TableCellElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_cell;
  }

  [[nodiscard]] virtual Element *column(const Document *document) const = 0;
  [[nodiscard]] virtual Element *row(const Document *document) const = 0;

  [[nodiscard]] virtual bool covered(const Document *document) const = 0;
  [[nodiscard]] virtual TableDimensions
  span(const Document *document) const = 0;
  [[nodiscard]] virtual ValueType
  value_type(const Document *document) const = 0;

  [[nodiscard]] virtual TableCellStyle
  style(const Document *document) const = 0;
};

class FrameElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::frame;
  }

  [[nodiscard]] virtual AnchorType
  anchor_type(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  width(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  height(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  z_index(const Document *document) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *document) const = 0;
};

class RectElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::rect;
  }

  [[nodiscard]] virtual std::string x(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *document) const = 0;
};

class LineElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::line;
  }

  [[nodiscard]] virtual std::string x1(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y1(const Document *document) const = 0;
  [[nodiscard]] virtual std::string x2(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y2(const Document *document) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *document) const = 0;
};

class CircleElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::circle;
  }

  [[nodiscard]] virtual std::string x(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *document) const = 0;
};

class CustomShapeElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::custom_shape;
  }

  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *document) const = 0;
};

class ImageElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::image;
  }

  [[nodiscard]] virtual bool internal(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<odr::File>
  file(const Document *document) const = 0;
  [[nodiscard]] virtual std::string href(const Document *document) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_ELEMENT_H
