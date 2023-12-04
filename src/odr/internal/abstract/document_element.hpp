#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_ELEMENT_H

#include <odr/document_element.hpp>

#include <memory>
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
class MasterPage;

class Element {
public:
  virtual ~Element() = default;

  [[nodiscard]] virtual ElementType type(const Document *) const = 0;

  [[nodiscard]] virtual Element *parent(const Document *) const = 0;
  [[nodiscard]] virtual Element *first_child(const Document *) const = 0;
  [[nodiscard]] virtual Element *last_child(const Document *) const = 0;
  [[nodiscard]] virtual Element *previous_sibling(const Document *) const = 0;
  [[nodiscard]] virtual Element *next_sibling(const Document *) const = 0;

  [[nodiscard]] virtual bool
  is_editable(const abstract::Document *document) const = 0;
};

class TextRoot : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::root;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *) const = 0;

  [[nodiscard]] virtual abstract::Element *
  first_master_page(const Document *) const = 0;
};

class Slide : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::slide;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *) const = 0;

  [[nodiscard]] virtual Element *master_page(const Document *) const = 0;

  [[nodiscard]] virtual std::string name(const Document *) const = 0;
};

class Page : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::page;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *) const = 0;

  [[nodiscard]] virtual Element *master_page(const Document *) const = 0;

  [[nodiscard]] virtual std::string name(const Document *) const = 0;
};

class MasterPage : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::master_page;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *) const = 0;
};

class LineBreak : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::line_break;
  }

  [[nodiscard]] virtual TextStyle style(const Document *) const = 0;
};

class Paragraph : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::paragraph;
  }

  [[nodiscard]] virtual ParagraphStyle style(const Document *) const = 0;
  [[nodiscard]] virtual TextStyle text_style(const Document *) const = 0;
};

class Span : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::span;
  }

  [[nodiscard]] virtual TextStyle style(const Document *document) const = 0;
};

class Text : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::text;
  }

  [[nodiscard]] virtual std::string content(const Document *) const = 0;
  virtual void set_content(const Document *, const std::string &text) = 0;

  [[nodiscard]] virtual TextStyle style(const Document *) const = 0;
};

class Link : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::link;
  }

  [[nodiscard]] virtual std::string href(const Document *) const = 0;
};

class Bookmark : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::bookmark;
  }

  [[nodiscard]] virtual std::string name(const Document *) const = 0;
};

class ListItem : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::list_item;
  }

  [[nodiscard]] virtual TextStyle style(const Document *) const = 0;
};

class Table : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table;
  }

  [[nodiscard]] virtual TableDimensions dimensions(const Document *) const = 0;

  [[nodiscard]] virtual Element *first_column(const Document *) const = 0;
  [[nodiscard]] virtual Element *first_row(const Document *) const = 0;

  [[nodiscard]] virtual TableStyle style(const Document *) const = 0;
};

class TableColumn : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_column;
  }

  [[nodiscard]] virtual TableColumnStyle style(const Document *) const = 0;
};

class TableRow : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_row;
  }

  [[nodiscard]] virtual TableRowStyle style(const Document *) const = 0;
};

class TableCell : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_cell;
  }

  [[nodiscard]] virtual bool is_covered(const Document *) const = 0;
  [[nodiscard]] virtual TableDimensions span(const Document *) const = 0;
  [[nodiscard]] virtual ValueType value_type(const Document *) const = 0;

  [[nodiscard]] virtual TableCellStyle style(const Document *) const = 0;
};

class Frame : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::frame;
  }

  [[nodiscard]] virtual AnchorType anchor_type(const Document *) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  width(const Document *) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  height(const Document *) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  z_index(const Document *) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *) const = 0;
};

class Rect : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::rect;
  }

  [[nodiscard]] virtual std::string x(const Document *) const = 0;
  [[nodiscard]] virtual std::string y(const Document *) const = 0;
  [[nodiscard]] virtual std::string width(const Document *) const = 0;
  [[nodiscard]] virtual std::string height(const Document *) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *) const = 0;
};

class Line : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::line;
  }

  [[nodiscard]] virtual std::string x1(const Document *) const = 0;
  [[nodiscard]] virtual std::string y1(const Document *) const = 0;
  [[nodiscard]] virtual std::string x2(const Document *) const = 0;
  [[nodiscard]] virtual std::string y2(const Document *) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *) const = 0;
};

class Circle : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::circle;
  }

  [[nodiscard]] virtual std::string x(const Document *) const = 0;
  [[nodiscard]] virtual std::string y(const Document *) const = 0;
  [[nodiscard]] virtual std::string width(const Document *) const = 0;
  [[nodiscard]] virtual std::string height(const Document *) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *) const = 0;
};

class CustomShape : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::custom_shape;
  }

  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *) const = 0;
  [[nodiscard]] virtual std::string width(const Document *) const = 0;
  [[nodiscard]] virtual std::string height(const Document *) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *) const = 0;
};

class Image : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::image;
  }

  [[nodiscard]] virtual bool is_internal(const Document *) const = 0;
  [[nodiscard]] virtual std::optional<odr::File>
  file(const Document *) const = 0;
  [[nodiscard]] virtual std::string href(const Document *) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_ELEMENT_H
