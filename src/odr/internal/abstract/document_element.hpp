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

  [[nodiscard]] virtual ElementType type(const Document *,
                                         ElementIdentifier) const = 0;

  [[nodiscard]] virtual Element *parent(const Document *,
                                        ElementIdentifier) const = 0;
  [[nodiscard]] virtual Element *first_child(const Document *,
                                             ElementIdentifier) const = 0;
  [[nodiscard]] virtual Element *last_child(const Document *,
                                            ElementIdentifier) const = 0;
  [[nodiscard]] virtual Element *previous_sibling(const Document *,
                                                  ElementIdentifier) const = 0;
  [[nodiscard]] virtual Element *next_sibling(const Document *,
                                              ElementIdentifier) const = 0;
};

class TextRoot : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::root;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *,
                                               ElementIdentifier) const = 0;

  [[nodiscard]] virtual MasterPage *
  first_master_page(const Document *, ElementIdentifier) const = 0;
};

class Slide : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::slide;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *,
                                               ElementIdentifier) const = 0;

  [[nodiscard]] virtual MasterPage *master_page(const Document *,
                                                ElementIdentifier) const = 0;

  [[nodiscard]] virtual std::string name(const Document *,
                                         ElementIdentifier) const = 0;
};

class Sheet : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::sheet;
  }

  [[nodiscard]] virtual std::string name(const Document *,
                                         ElementIdentifier) const = 0;

  [[nodiscard]] virtual TableDimensions dimensions(const Document *,
                                                   ElementIdentifier) const = 0;
  [[nodiscard]] virtual TableDimensions
  content(const Document *, ElementIdentifier,
          std::optional<TableDimensions> range) const = 0;

  [[nodiscard]] virtual Element *column(const Document *, ElementIdentifier,
                                        ColumnIndex column) const = 0;
  [[nodiscard]] virtual Element *row(const Document *, ElementIdentifier,
                                     RowIndex row) const = 0;
  [[nodiscard]] virtual Element *cell(const Document *, ElementIdentifier,
                                      ColumnIndex column,
                                      RowIndex row) const = 0;

  [[nodiscard]] virtual Element *first_shape(const Document *,
                                             ElementIdentifier) const = 0;

  [[nodiscard]] virtual TableStyle style(const Document *,
                                         ElementIdentifier) const = 0;
};

class Page : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::page;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *,
                                               ElementIdentifier) const = 0;

  [[nodiscard]] virtual MasterPage *master_page(const Document *,
                                                ElementIdentifier) const = 0;

  [[nodiscard]] virtual std::string name(const Document *,
                                         ElementIdentifier) const = 0;
};

class MasterPage : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::master_page;
  }

  [[nodiscard]] virtual PageLayout page_layout(const Document *,
                                               ElementIdentifier) const = 0;
};

class LineBreak : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::line_break;
  }

  [[nodiscard]] virtual TextStyle style(const Document *,
                                        ElementIdentifier) const = 0;
};

class Paragraph : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::paragraph;
  }

  [[nodiscard]] virtual ParagraphStyle style(const Document *,
                                             ElementIdentifier) const = 0;
  [[nodiscard]] virtual TextStyle text_style(const Document *,
                                             ElementIdentifier) const = 0;
};

class Span : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::span;
  }

  [[nodiscard]] virtual TextStyle style(const Document *document,
                                        ElementIdentifier) const = 0;
};

class Text : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::text;
  }

  [[nodiscard]] virtual std::string content(const Document *,
                                            ElementIdentifier) const = 0;
  virtual void set_content(const Document *, ElementIdentifier,
                           const std::string &text) = 0;

  [[nodiscard]] virtual TextStyle style(const Document *,
                                        ElementIdentifier) const = 0;
};

class Link : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::link;
  }

  [[nodiscard]] virtual std::string href(const Document *,
                                         ElementIdentifier) const = 0;
};

class Bookmark : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::bookmark;
  }

  [[nodiscard]] virtual std::string name(const Document *,
                                         ElementIdentifier) const = 0;
};

class ListItem : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::list_item;
  }

  [[nodiscard]] virtual TextStyle style(const Document *,
                                        ElementIdentifier) const = 0;
};

class Table : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::table;
  }

  [[nodiscard]] virtual TableDimensions dimensions(const Document *,
                                                   ElementIdentifier) const = 0;

  [[nodiscard]] virtual Element *first_column(const Document *,
                                              ElementIdentifier) const = 0;
  [[nodiscard]] virtual Element *first_row(const Document *,
                                           ElementIdentifier) const = 0;

  [[nodiscard]] virtual TableStyle style(const Document *,
                                         ElementIdentifier) const = 0;
};

class TableColumn : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::table_column;
  }

  [[nodiscard]] virtual TableColumnStyle style(const Document *,
                                               ElementIdentifier) const = 0;
};

class TableRow : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::table_row;
  }

  [[nodiscard]] virtual TableRowStyle style(const Document *,
                                            ElementIdentifier) const = 0;
};

class TableCell : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::table_cell;
  }

  [[nodiscard]] virtual bool covered(const Document *,
                                     ElementIdentifier) const = 0;
  [[nodiscard]] virtual TableDimensions span(const Document *,
                                             ElementIdentifier) const = 0;
  [[nodiscard]] virtual ValueType value_type(const Document *,
                                             ElementIdentifier) const = 0;

  [[nodiscard]] virtual TableCellStyle style(const Document *,
                                             ElementIdentifier) const = 0;
};

class Frame : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::frame;
  }

  [[nodiscard]] virtual AnchorType anchor_type(const Document *,
                                               ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *, ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *, ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  width(const Document *, ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  height(const Document *, ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  z_index(const Document *, ElementIdentifier) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *,
                                           ElementIdentifier) const = 0;
};

class Rect : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::rect;
  }

  [[nodiscard]] virtual std::string x(const Document *,
                                      ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string y(const Document *,
                                      ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string width(const Document *,
                                          ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string height(const Document *,
                                           ElementIdentifier) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *,
                                           ElementIdentifier) const = 0;
};

class Line : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::line;
  }

  [[nodiscard]] virtual std::string x1(const Document *,
                                       ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string y1(const Document *,
                                       ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string x2(const Document *,
                                       ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string y2(const Document *,
                                       ElementIdentifier) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *,
                                           ElementIdentifier) const = 0;
};

class Circle : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::circle;
  }

  [[nodiscard]] virtual std::string x(const Document *,
                                      ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string y(const Document *,
                                      ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string width(const Document *,
                                          ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string height(const Document *,
                                           ElementIdentifier) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *,
                                           ElementIdentifier) const = 0;
};

class CustomShape : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::custom_shape;
  }

  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *, ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *, ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string width(const Document *,
                                          ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string height(const Document *,
                                           ElementIdentifier) const = 0;

  [[nodiscard]] virtual GraphicStyle style(const Document *,
                                           ElementIdentifier) const = 0;
};

class Image : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::image;
  }

  [[nodiscard]] virtual bool internal(const Document *,
                                      ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::optional<odr::File>
  file(const Document *, ElementIdentifier) const = 0;
  [[nodiscard]] virtual std::string href(const Document *,
                                         ElementIdentifier) const = 0;
};

class SheetColumn : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::table_column;
  }

  [[nodiscard]] virtual TableColumnStyle
  style(const Document *, ElementIdentifier, ColumnIndex) const = 0;
};

class SheetRow : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::table_row;
  }

  [[nodiscard]] virtual TableRowStyle style(const Document *, ElementIdentifier,
                                            RowIndex) const = 0;
};

class SheetCell : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *,
                                 ElementIdentifier) const override {
    return ElementType::table_cell;
  }

  [[nodiscard]] virtual bool covered(const Document *, ElementIdentifier,
                                     ColumnIndex, RowIndex) const = 0;
  [[nodiscard]] virtual TableDimensions
  span(const Document *, ElementIdentifier, ColumnIndex, RowIndex) const = 0;
  [[nodiscard]] virtual ValueType value_type(const Document *,
                                             ElementIdentifier, ColumnIndex,
                                             RowIndex) const = 0;

  [[nodiscard]] virtual TableCellStyle
  style(const Document *, ElementIdentifier, ColumnIndex, RowIndex) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_ELEMENT_H
