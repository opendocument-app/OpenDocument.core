#import <OdrCoreObjC/ODRDocumentElement.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/document_element.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <optional>

using odr::apple::box;
using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::to_nsstring;
using odr::apple::to_string;

ODR_SAME_ENUM(ODRElementTypeNone, odr::ElementType::none);
ODR_SAME_ENUM(ODRElementTypeRoot, odr::ElementType::root);
ODR_SAME_ENUM(ODRElementTypeSlide, odr::ElementType::slide);
ODR_SAME_ENUM(ODRElementTypeSheet, odr::ElementType::sheet);
ODR_SAME_ENUM(ODRElementTypePage, odr::ElementType::page);
ODR_SAME_ENUM(ODRElementTypeMasterPage, odr::ElementType::master_page);
ODR_SAME_ENUM(ODRElementTypeSheetCell, odr::ElementType::sheet_cell);
ODR_SAME_ENUM(ODRElementTypeText, odr::ElementType::text);
ODR_SAME_ENUM(ODRElementTypeLineBreak, odr::ElementType::line_break);
ODR_SAME_ENUM(ODRElementTypePageBreak, odr::ElementType::page_break);
ODR_SAME_ENUM(ODRElementTypeParagraph, odr::ElementType::paragraph);
ODR_SAME_ENUM(ODRElementTypeSpan, odr::ElementType::span);
ODR_SAME_ENUM(ODRElementTypeLink, odr::ElementType::link);
ODR_SAME_ENUM(ODRElementTypeBookmark, odr::ElementType::bookmark);
ODR_SAME_ENUM(ODRListTypeUnordered, odr::ListType::unordered);
ODR_SAME_ENUM(ODRListTypeOrdered, odr::ListType::ordered);

ODR_SAME_ENUM(ODRElementTypeList, odr::ElementType::list);
ODR_SAME_ENUM(ODRElementTypeListItem, odr::ElementType::list_item);
ODR_SAME_ENUM(ODRElementTypeTable, odr::ElementType::table);
ODR_SAME_ENUM(ODRElementTypeTableColumn, odr::ElementType::table_column);
ODR_SAME_ENUM(ODRElementTypeTableRow, odr::ElementType::table_row);
ODR_SAME_ENUM(ODRElementTypeTableCell, odr::ElementType::table_cell);
ODR_SAME_ENUM(ODRElementTypeFrame, odr::ElementType::frame);
ODR_SAME_ENUM(ODRElementTypeImage, odr::ElementType::image);
ODR_SAME_ENUM(ODRElementTypeRect, odr::ElementType::rect);
ODR_SAME_ENUM(ODRElementTypeLine, odr::ElementType::line);
ODR_SAME_ENUM(ODRElementTypeCircle, odr::ElementType::circle);
ODR_SAME_ENUM(ODRElementTypeCustomShape, odr::ElementType::custom_shape);
ODR_SAME_ENUM(ODRElementTypeGroup, odr::ElementType::group);

ODR_SAME_ENUM(ODRAnchorTypeAsChar, odr::AnchorType::as_char);
ODR_SAME_ENUM(ODRAnchorTypeAtChar, odr::AnchorType::at_char);
ODR_SAME_ENUM(ODRAnchorTypeAtFrame, odr::AnchorType::at_frame);
ODR_SAME_ENUM(ODRAnchorTypeAtPage, odr::AnchorType::at_page);
ODR_SAME_ENUM(ODRAnchorTypeAtParagraph, odr::AnchorType::at_paragraph);

ODR_SAME_ENUM(ODRValueTypeUnknown, odr::ValueType::unknown);
ODR_SAME_ENUM(ODRValueTypeString, odr::ValueType::string);
ODR_SAME_ENUM(ODRValueTypeFloatNumber, odr::ValueType::float_number);

namespace {

ODRTableDimensions to_dimensions(const odr::TableDimensions &value) {
  return ODRTableDimensionsMake(value.rows, value.columns);
}

/// Wraps a range through `-derive:`, so `source`'s owner is carried along.
NSArray<ODRElement *> *to_nsarray(ODRElement *const source,
                                  const odr::ElementRange &range) {
  NSMutableArray<ODRElement *> *const result = [NSMutableArray array];
  for (const odr::Element element : range) {
    if (ODRElement *const wrapped = [source derive:element]; wrapped != nil) {
      [result addObject:wrapped];
    }
  }
  return result;
}

} // namespace

@implementation ODRElement {
  odr::Element _handle;
  // `odr::Element` holds a bare pointer into the document's adapter, so the
  // tree has to keep the document alive itself.
  id _owner;
}

+ (nullable ODRElement *)elementWithHandle:(odr::Element)handle
                                     owner:(id)owner {
  if (!handle) {
    return nil;
  }

  // The most derived class the node qualifies for, so a caller never downcasts
  // something the type already tells it.
  Class klass = [ODRElement class];
  switch (handle.type()) {
  case odr::ElementType::root:
    klass = [ODRTextRoot class];
    break;
  case odr::ElementType::slide:
    klass = [ODRSlide class];
    break;
  case odr::ElementType::sheet:
    klass = [ODRSheet class];
    break;
  case odr::ElementType::page:
    klass = [ODRPage class];
    break;
  case odr::ElementType::master_page:
    klass = [ODRMasterPage class];
    break;
  case odr::ElementType::sheet_cell:
    klass = [ODRSheetCell class];
    break;
  case odr::ElementType::text:
    klass = [ODRText class];
    break;
  case odr::ElementType::line_break:
    klass = [ODRLineBreak class];
    break;
  case odr::ElementType::paragraph:
    klass = [ODRParagraph class];
    break;
  case odr::ElementType::span:
    klass = [ODRSpan class];
    break;
  case odr::ElementType::link:
    klass = [ODRLink class];
    break;
  case odr::ElementType::bookmark:
    klass = [ODRBookmark class];
    break;
  case odr::ElementType::list:
    klass = [ODRList class];
    break;
  case odr::ElementType::list_item:
    klass = [ODRListItem class];
    break;
  case odr::ElementType::table:
    klass = [ODRTable class];
    break;
  case odr::ElementType::table_column:
    klass = [ODRTableColumn class];
    break;
  case odr::ElementType::table_row:
    klass = [ODRTableRow class];
    break;
  case odr::ElementType::table_cell:
    klass = [ODRTableCell class];
    break;
  case odr::ElementType::frame:
    klass = [ODRFrame class];
    break;
  case odr::ElementType::image:
    klass = [ODRImage class];
    break;
  case odr::ElementType::rect:
    klass = [ODRRect class];
    break;
  case odr::ElementType::line:
    klass = [ODRLine class];
    break;
  case odr::ElementType::circle:
    klass = [ODRCircle class];
    break;
  case odr::ElementType::custom_shape:
    klass = [ODRCustomShape class];
    break;
  // page_break, list and group have no typed C++ view of their own
  default:
    break;
  }

  ODRElement *const result = [[klass alloc] init];
  result->_handle = std::move(handle);
  result->_owner = owner;
  return result;
}

- (const odr::Element &)handle {
  return _handle;
}

- (id)owner {
  return _owner;
}

/// Every navigation goes through here, so the owner is carried along by
/// construction rather than by remembering to pass it.
- (nullable ODRElement *)derive:(odr::Element)handle {
  return [ODRElement elementWithHandle:std::move(handle) owner:_owner];
}

- (ODRElementType)type {
  return guarded_value(
      [&] { return static_cast<ODRElementType>(_handle.type()); },
      ODRElementTypeNone);
}

- (BOOL)exists {
  return _handle ? YES : NO;
}

- (nullable ODRElement *)parent {
  return guarded_value(
      [&]() -> ODRElement * { return [self derive:_handle.parent()]; }, nil);
}

- (nullable ODRElement *)firstChild {
  return guarded_value(
      [&]() -> ODRElement * { return [self derive:_handle.first_child()]; },
      nil);
}

- (nullable ODRElement *)previousSibling {
  return guarded_value(
      [&]() -> ODRElement * {
        return [self derive:_handle.previous_sibling()];
      },
      nil);
}

- (nullable ODRElement *)nextSibling {
  return guarded_value(
      [&]() -> ODRElement * { return [self derive:_handle.next_sibling()]; },
      nil);
}

- (NSArray<ODRElement *> *)children {
  return guarded_value(
      [&]() -> NSArray<ODRElement *> * {
        return to_nsarray(self, _handle.children());
      },
      @[]);
}

- (BOOL)isUnique {
  return guarded_value([&] { return _handle.is_unique() ? YES : NO; }, NO);
}

- (BOOL)isSelfLocatable {
  return guarded_value([&] { return _handle.is_self_locatable() ? YES : NO; },
                       NO);
}

- (BOOL)isEditable {
  return guarded_value([&] { return _handle.is_editable() ? YES : NO; }, NO);
}

@end

#pragma mark - typed views

// Each typed view re-derives from the base handle per call rather than storing
// a derived subobject, as in the JNI bindings.

@implementation ODRTextRoot

- (ODRPageLayout *)pageLayout {
  return guarded_value(
      [&]() -> ODRPageLayout * {
        return [ODRPageLayout
            layoutWithHandle:self.handle.as_text_root().page_layout()];
      },
      nil);
}

- (nullable ODRElement *)firstMasterPage {
  return guarded_value(
      [&]() -> ODRElement * {
        return [self derive:self.handle.as_text_root().first_master_page()];
      },
      nil);
}

@end

@implementation ODRSlide

- (NSString *)name {
  return guarded_value(
      [&] { return to_nsstring(self.handle.as_slide().name()); }, @"");
}

- (ODRPageLayout *)pageLayout {
  return guarded_value(
      [&]() -> ODRPageLayout * {
        return [ODRPageLayout
            layoutWithHandle:self.handle.as_slide().page_layout()];
      },
      nil);
}

- (nullable ODRElement *)masterPage {
  return guarded_value(
      [&]() -> ODRElement * {
        return [self derive:self.handle.as_slide().master_page()];
      },
      nil);
}

@end

@implementation ODRSheet

- (NSString *)name {
  return guarded_value(
      [&] { return to_nsstring(self.handle.as_sheet().name()); }, @"");
}

- (ODRPageLayout *)pageLayout {
  return guarded_value(
      [&]() -> ODRPageLayout * {
        return [ODRPageLayout
            layoutWithHandle:self.handle.as_sheet().page_layout()];
      },
      nil);
}

- (ODRTableDimensions)dimensions {
  return guarded_value(
      [&] { return to_dimensions(self.handle.as_sheet().dimensions()); },
      ODRTableDimensionsMake(0, 0));
}

- (ODRTableDimensions)contentDimensions {
  return guarded_value(
      [&] {
        return to_dimensions(self.handle.as_sheet().content(std::nullopt));
      },
      ODRTableDimensionsMake(0, 0));
}

- (ODRTableDimensions)contentDimensionsInRange:(ODRTableDimensions)range {
  return guarded_value(
      [&] {
        return to_dimensions(self.handle.as_sheet().content(
            odr::TableDimensions(range.rows, range.columns)));
      },
      ODRTableDimensionsMake(0, 0));
}

- (nullable ODRElement *)cellAtColumn:(uint32_t)column row:(uint32_t)row {
  return guarded_value(
      [&]() -> ODRElement * {
        return [self derive:self.handle.as_sheet().cell(column, row)];
      },
      nil);
}

- (NSArray<ODRElement *> *)shapes {
  return guarded_value(
      [&]() -> NSArray<ODRElement *> * {
        return to_nsarray(self, self.handle.as_sheet().shapes());
      },
      @[]);
}

- (ODRTableStyle *)style {
  return guarded_value(
      [&]() -> ODRTableStyle * {
        return [ODRTableStyle styleWithHandle:self.handle.as_sheet().style()];
      },
      nil);
}

- (ODRTableColumnStyle *)styleForColumn:(uint32_t)column {
  return guarded_value(
      [&]() -> ODRTableColumnStyle * {
        return [ODRTableColumnStyle
            styleWithHandle:self.handle.as_sheet().column_style(column)];
      },
      nil);
}

- (ODRTableRowStyle *)styleForRow:(uint32_t)row {
  return guarded_value(
      [&]() -> ODRTableRowStyle * {
        return [ODRTableRowStyle
            styleWithHandle:self.handle.as_sheet().row_style(row)];
      },
      nil);
}

- (ODRTableCellStyle *)styleForCellAtColumn:(uint32_t)column row:(uint32_t)row {
  return guarded_value(
      [&]() -> ODRTableCellStyle * {
        return [ODRTableCellStyle
            styleWithHandle:self.handle.as_sheet().cell_style(column, row)];
      },
      nil);
}

@end

@implementation ODRSheetCell

- (ODRTablePosition)position {
  return guarded_value(
      [&] {
        const odr::TablePosition position =
            self.handle.as_sheet_cell().position();
        return ODRTablePositionMake(position.column, position.row);
      },
      ODRTablePositionMake(0, 0));
}

- (BOOL)isCovered {
  return guarded_value(
      [&] { return self.handle.as_sheet_cell().is_covered() ? YES : NO; }, NO);
}

- (ODRTableDimensions)span {
  return guarded_value(
      [&] { return to_dimensions(self.handle.as_sheet_cell().span()); },
      ODRTableDimensionsMake(0, 0));
}

- (ODRValueType)valueType {
  return guarded_value(
      [&] {
        return static_cast<ODRValueType>(
            self.handle.as_sheet_cell().value_type());
      },
      ODRValueTypeUnknown);
}

@end

@implementation ODRPage

- (NSString *)name {
  return guarded_value(
      [&] { return to_nsstring(self.handle.as_page().name()); }, @"");
}

- (ODRPageLayout *)pageLayout {
  return guarded_value(
      [&]() -> ODRPageLayout * {
        return [ODRPageLayout
            layoutWithHandle:self.handle.as_page().page_layout()];
      },
      nil);
}

- (nullable ODRElement *)masterPage {
  return guarded_value(
      [&]() -> ODRElement * {
        return [self derive:self.handle.as_page().master_page()];
      },
      nil);
}

@end

@implementation ODRMasterPage

- (ODRPageLayout *)pageLayout {
  return guarded_value(
      [&]() -> ODRPageLayout * {
        return [ODRPageLayout
            layoutWithHandle:self.handle.as_master_page().page_layout()];
      },
      nil);
}

@end

@implementation ODRLineBreak

- (ODRTextStyle *)style {
  return guarded_value(
      [&]() -> ODRTextStyle * {
        return
            [ODRTextStyle styleWithHandle:self.handle.as_line_break().style()];
      },
      nil);
}

@end

@implementation ODRParagraph

- (ODRParagraphStyle *)style {
  return guarded_value(
      [&]() -> ODRParagraphStyle * {
        return [ODRParagraphStyle
            styleWithHandle:self.handle.as_paragraph().style()];
      },
      nil);
}

- (ODRTextStyle *)textStyle {
  return guarded_value(
      [&]() -> ODRTextStyle * {
        return [ODRTextStyle
            styleWithHandle:self.handle.as_paragraph().text_style()];
      },
      nil);
}

@end

@implementation ODRSpan

- (ODRTextStyle *)style {
  return guarded_value(
      [&]() -> ODRTextStyle * {
        return [ODRTextStyle styleWithHandle:self.handle.as_span().style()];
      },
      nil);
}

@end

@implementation ODRText

- (NSString *)content {
  return guarded_value(
      [&] { return to_nsstring(self.handle.as_text().content()); }, @"");
}

- (BOOL)setContent:(NSString *)content error:(NSError **)error {
  return guarded(error, [&] {
    self.handle.as_text().set_content(to_string(content));
    return YES;
  });
}

- (ODRTextStyle *)style {
  return guarded_value(
      [&]() -> ODRTextStyle * {
        return [ODRTextStyle styleWithHandle:self.handle.as_text().style()];
      },
      nil);
}

@end

@implementation ODRLink

- (NSString *)href {
  return guarded_value(
      [&] { return to_nsstring(self.handle.as_link().href()); }, @"");
}

@end

@implementation ODRBookmark

- (NSString *)name {
  return guarded_value(
      [&] { return to_nsstring(self.handle.as_bookmark().name()); }, @"");
}

@end

@implementation ODRList

- (ODRListType)listType {
  return guarded_value(
      [&] { return static_cast<ODRListType>(self.handle.as_list().type()); },
      ODRListTypeUnordered);
}

@end

@implementation ODRListItem

- (ODRTextStyle *)style {
  return guarded_value(
      [&]() -> ODRTextStyle * {
        return
            [ODRTextStyle styleWithHandle:self.handle.as_list_item().style()];
      },
      nil);
}

- (NSString *)marker {
  return guarded_value(
      [&]() -> NSString * {
        return to_nsstring(self.handle.as_list_item().marker());
      },
      @"");
}

- (nullable NSNumber *)number {
  return guarded_value(
      [&]() -> NSNumber * {
        const std::optional<std::uint32_t> number =
            self.handle.as_list_item().number();
        return number.has_value() ? @(*number) : nil;
      },
      nil);
}

@end

@implementation ODRTable

- (nullable ODRElement *)firstRow {
  return guarded_value(
      [&]() -> ODRElement * {
        return [self derive:self.handle.as_table().first_row()];
      },
      nil);
}

- (nullable ODRElement *)firstColumn {
  return guarded_value(
      [&]() -> ODRElement * {
        return [self derive:self.handle.as_table().first_column()];
      },
      nil);
}

- (NSArray<ODRElement *> *)columns {
  return guarded_value(
      [&]() -> NSArray<ODRElement *> * {
        return to_nsarray(self, self.handle.as_table().columns());
      },
      @[]);
}

- (NSArray<ODRElement *> *)rows {
  return guarded_value(
      [&]() -> NSArray<ODRElement *> * {
        return to_nsarray(self, self.handle.as_table().rows());
      },
      @[]);
}

- (ODRTableDimensions)dimensions {
  return guarded_value(
      [&] { return to_dimensions(self.handle.as_table().dimensions()); },
      ODRTableDimensionsMake(0, 0));
}

- (ODRTableStyle *)style {
  return guarded_value(
      [&]() -> ODRTableStyle * {
        return [ODRTableStyle styleWithHandle:self.handle.as_table().style()];
      },
      nil);
}

@end

@implementation ODRTableColumn

- (ODRTableColumnStyle *)style {
  return guarded_value(
      [&]() -> ODRTableColumnStyle * {
        return [ODRTableColumnStyle
            styleWithHandle:self.handle.as_table_column().style()];
      },
      nil);
}

@end

@implementation ODRTableRow

- (ODRTableRowStyle *)style {
  return guarded_value(
      [&]() -> ODRTableRowStyle * {
        return [ODRTableRowStyle
            styleWithHandle:self.handle.as_table_row().style()];
      },
      nil);
}

@end

@implementation ODRTableCell

- (BOOL)isCovered {
  return guarded_value(
      [&] { return self.handle.as_table_cell().is_covered() ? YES : NO; }, NO);
}

- (ODRTableDimensions)span {
  return guarded_value(
      [&] { return to_dimensions(self.handle.as_table_cell().span()); },
      ODRTableDimensionsMake(0, 0));
}

- (ODRValueType)valueType {
  return guarded_value(
      [&] {
        return static_cast<ODRValueType>(
            self.handle.as_table_cell().value_type());
      },
      ODRValueTypeUnknown);
}

- (ODRTableCellStyle *)style {
  return guarded_value(
      [&]() -> ODRTableCellStyle * {
        return [ODRTableCellStyle
            styleWithHandle:self.handle.as_table_cell().style()];
      },
      nil);
}

@end

@implementation ODRFrame

- (ODRAnchorType)anchorType {
  return guarded_value(
      [&] {
        return static_cast<ODRAnchorType>(self.handle.as_frame().anchor_type());
      },
      ODRAnchorTypeAsChar);
}

- (nullable ODRMeasure *)x {
  return guarded_value([&] { return box(self.handle.as_frame().x()); }, nil);
}

- (nullable ODRMeasure *)y {
  return guarded_value([&] { return box(self.handle.as_frame().y()); }, nil);
}

- (nullable ODRMeasure *)width {
  return guarded_value([&] { return box(self.handle.as_frame().width()); },
                       nil);
}

- (nullable ODRMeasure *)height {
  return guarded_value([&] { return box(self.handle.as_frame().height()); },
                       nil);
}

- (nullable NSNumber *)zIndex {
  return guarded_value(
      [&]() -> NSNumber * {
        const std::optional<std::int32_t> z = self.handle.as_frame().z_index();
        return z.has_value() ? @(*z) : nil;
      },
      nil);
}

- (nullable ODRDrawingTransform *)transform {
  return guarded_value(
      [&]() -> ODRDrawingTransform * {
        return [ODRDrawingTransform
            transformWithHandle:self.handle.as_frame().transform()];
      },
      nil);
}

- (ODRGraphicStyle *)style {
  return guarded_value(
      [&]() -> ODRGraphicStyle * {
        return [ODRGraphicStyle styleWithHandle:self.handle.as_frame().style()];
      },
      nil);
}

@end

@implementation ODRDrawingPath

+ (nullable instancetype)pathWithHandle:
    (const std::optional<odr::DrawingPath> &)handle {
  if (!handle.has_value()) {
    return nil;
  }
  ODRDrawingPath *const result = [[ODRDrawingPath alloc] init];
  result->_data = to_nsstring(handle->data);
  result->_x = handle->x;
  result->_y = handle->y;
  result->_width = handle->width;
  result->_height = handle->height;
  return result;
}

@end

@implementation ODRDrawingTransform

+ (nullable instancetype)transformWithHandle:
    (const std::optional<odr::DrawingTransform> &)handle {
  if (!handle.has_value()) {
    return nil;
  }
  ODRDrawingTransform *const result = [[ODRDrawingTransform alloc] init];
  result->_a = handle->a;
  result->_b = handle->b;
  result->_c = handle->c;
  result->_d = handle->d;
  result->_e = [ODRMeasure measureWithHandle:handle->e];
  result->_f = [ODRMeasure measureWithHandle:handle->f];
  return result;
}

@end

@implementation ODRRect

- (ODRMeasure *)x {
  return guarded_value(
      [&] { return [ODRMeasure measureWithHandle:self.handle.as_rect().x()]; },
      nil);
}

- (ODRMeasure *)y {
  return guarded_value(
      [&] { return [ODRMeasure measureWithHandle:self.handle.as_rect().y()]; },
      nil);
}

- (ODRMeasure *)width {
  return guarded_value(
      [&] {
        return [ODRMeasure measureWithHandle:self.handle.as_rect().width()];
      },
      nil);
}

- (ODRMeasure *)height {
  return guarded_value(
      [&] {
        return [ODRMeasure measureWithHandle:self.handle.as_rect().height()];
      },
      nil);
}

- (nullable ODRDrawingTransform *)transform {
  return guarded_value(
      [&]() -> ODRDrawingTransform * {
        return [ODRDrawingTransform
            transformWithHandle:self.handle.as_rect().transform()];
      },
      nil);
}

- (ODRGraphicStyle *)style {
  return guarded_value(
      [&]() -> ODRGraphicStyle * {
        return [ODRGraphicStyle styleWithHandle:self.handle.as_rect().style()];
      },
      nil);
}

@end

@implementation ODRLine

- (ODRMeasure *)x1 {
  return guarded_value(
      [&] { return [ODRMeasure measureWithHandle:self.handle.as_line().x1()]; },
      nil);
}

- (ODRMeasure *)y1 {
  return guarded_value(
      [&] { return [ODRMeasure measureWithHandle:self.handle.as_line().y1()]; },
      nil);
}

- (ODRMeasure *)x2 {
  return guarded_value(
      [&] { return [ODRMeasure measureWithHandle:self.handle.as_line().x2()]; },
      nil);
}

- (ODRMeasure *)y2 {
  return guarded_value(
      [&] { return [ODRMeasure measureWithHandle:self.handle.as_line().y2()]; },
      nil);
}

- (nullable ODRDrawingTransform *)transform {
  return guarded_value(
      [&]() -> ODRDrawingTransform * {
        return [ODRDrawingTransform
            transformWithHandle:self.handle.as_line().transform()];
      },
      nil);
}

- (ODRGraphicStyle *)style {
  return guarded_value(
      [&]() -> ODRGraphicStyle * {
        return [ODRGraphicStyle styleWithHandle:self.handle.as_line().style()];
      },
      nil);
}

@end

@implementation ODRCircle

- (ODRMeasure *)x {
  return guarded_value(
      [&] {
        return [ODRMeasure measureWithHandle:self.handle.as_circle().x()];
      },
      nil);
}

- (ODRMeasure *)y {
  return guarded_value(
      [&] {
        return [ODRMeasure measureWithHandle:self.handle.as_circle().y()];
      },
      nil);
}

- (ODRMeasure *)width {
  return guarded_value(
      [&] {
        return [ODRMeasure measureWithHandle:self.handle.as_circle().width()];
      },
      nil);
}

- (ODRMeasure *)height {
  return guarded_value(
      [&] {
        return [ODRMeasure measureWithHandle:self.handle.as_circle().height()];
      },
      nil);
}

- (nullable ODRDrawingTransform *)transform {
  return guarded_value(
      [&]() -> ODRDrawingTransform * {
        return [ODRDrawingTransform
            transformWithHandle:self.handle.as_circle().transform()];
      },
      nil);
}

- (ODRGraphicStyle *)style {
  return guarded_value(
      [&]() -> ODRGraphicStyle * {
        return
            [ODRGraphicStyle styleWithHandle:self.handle.as_circle().style()];
      },
      nil);
}

@end

@implementation ODRCustomShape

- (nullable ODRMeasure *)x {
  return guarded_value([&] { return box(self.handle.as_custom_shape().x()); },
                       nil);
}

- (nullable ODRMeasure *)y {
  return guarded_value([&] { return box(self.handle.as_custom_shape().y()); },
                       nil);
}

- (ODRMeasure *)width {
  return guarded_value(
      [&] {
        return [ODRMeasure
            measureWithHandle:self.handle.as_custom_shape().width()];
      },
      nil);
}

- (ODRMeasure *)height {
  return guarded_value(
      [&] {
        return [ODRMeasure
            measureWithHandle:self.handle.as_custom_shape().height()];
      },
      nil);
}

- (nullable ODRDrawingTransform *)transform {
  return guarded_value(
      [&]() -> ODRDrawingTransform * {
        return [ODRDrawingTransform
            transformWithHandle:self.handle.as_custom_shape().transform()];
      },
      nil);
}

- (nullable ODRDrawingPath *)path {
  return guarded_value(
      [&]() -> ODRDrawingPath * {
        return [ODRDrawingPath
            pathWithHandle:self.handle.as_custom_shape().path()];
      },
      nil);
}

- (ODRGraphicStyle *)style {
  return guarded_value(
      [&]() -> ODRGraphicStyle * {
        return [ODRGraphicStyle
            styleWithHandle:self.handle.as_custom_shape().style()];
      },
      nil);
}

@end

@implementation ODRImage

- (BOOL)isInternal {
  return guarded_value(
      [&] { return self.handle.as_image().is_internal() ? YES : NO; }, NO);
}

- (nullable ODRFile *)file {
  return guarded_value(
      [&]() -> ODRFile * {
        const std::optional<odr::File> file = self.handle.as_image().file();
        return file.has_value() ? [ODRFile fileWithHandle:*file] : nil;
      },
      nil);
}

- (NSString *)href {
  return guarded_value(
      [&] { return to_nsstring(self.handle.as_image().href()); }, @"");
}

@end
