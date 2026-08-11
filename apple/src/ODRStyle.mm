#import <OdrCoreObjC/ODRStyle.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/style.hpp>

#include <optional>

using odr::apple::guarded_value;
using odr::apple::to_nsstring;

ODR_SAME_ENUM(ODRFontWeightNormal, odr::FontWeight::normal);
ODR_SAME_ENUM(ODRFontWeightBold, odr::FontWeight::bold);
ODR_SAME_ENUM(ODRFontStyleNormal, odr::FontStyle::normal);
ODR_SAME_ENUM(ODRFontStyleItalic, odr::FontStyle::italic);
ODR_SAME_ENUM(ODRFontPositionNormal, odr::FontPosition::normal);
ODR_SAME_ENUM(ODRFontPositionSuper, odr::FontPosition::super);
ODR_SAME_ENUM(ODRFontPositionSub, odr::FontPosition::sub);
ODR_SAME_ENUM(ODRTextAlignLeft, odr::TextAlign::left);
ODR_SAME_ENUM(ODRTextAlignRight, odr::TextAlign::right);
ODR_SAME_ENUM(ODRTextAlignCenter, odr::TextAlign::center);
ODR_SAME_ENUM(ODRTextAlignJustify, odr::TextAlign::justify);
ODR_SAME_ENUM(ODRHorizontalAlignLeft, odr::HorizontalAlign::left);
ODR_SAME_ENUM(ODRHorizontalAlignCenter, odr::HorizontalAlign::center);
ODR_SAME_ENUM(ODRHorizontalAlignRight, odr::HorizontalAlign::right);
ODR_SAME_ENUM(ODRVerticalAlignTop, odr::VerticalAlign::top);
ODR_SAME_ENUM(ODRVerticalAlignMiddle, odr::VerticalAlign::middle);
ODR_SAME_ENUM(ODRVerticalAlignBottom, odr::VerticalAlign::bottom);
ODR_SAME_ENUM(ODRPrintOrientationPortrait, odr::PrintOrientation::portrait);
ODR_SAME_ENUM(ODRPrintOrientationLandscape, odr::PrintOrientation::landscape);
ODR_SAME_ENUM(ODRTextWrapNone, odr::TextWrap::none);
ODR_SAME_ENUM(ODRTextWrapBefore, odr::TextWrap::before);
ODR_SAME_ENUM(ODRTextWrapAfter, odr::TextWrap::after);
ODR_SAME_ENUM(ODRTextWrapRunThrough, odr::TextWrap::run_through);

static_assert(sizeof(ODRColor) == sizeof(odr::Color),
              "ODRColor drifted from odr::Color");

// Reopened as `odr` so `apple::` below is a meaningful qualification rather
// than a redundant one — see ODRInternal.mm.
namespace odr {

NSValue *_Nullable apple::box(const std::optional<odr::Color> &color) {
  if (!color.has_value()) {
    return nil;
  }
  const ODRColor boxed =
      ODRColorMake(color->red, color->green, color->blue, color->alpha);
  return [NSValue valueWithBytes:&boxed objCType:@encode(ODRColor)];
}

ODRMeasure *_Nullable apple::box(const std::optional<odr::Measure> &measure) {
  return measure.has_value() ? [ODRMeasure measureWithHandle:*measure] : nil;
}

} // namespace odr

using odr::apple::box;

namespace {

/// An absent `std::optional` boxes as `nil` and not as a sentinel: a style that
/// does not set a property is different from one that sets it to a default, and
/// only the caller knows what to fall back to.
template <typename T>
NSNumber *_Nullable box_number(const std::optional<T> &value) {
  return value.has_value() ? @(*value) : nil;
}

template <typename Enum>
NSNumber *_Nullable box_enum(const std::optional<Enum> &value) {
  return value.has_value() ? @(static_cast<NSInteger>(*value)) : nil;
}

/// Also takes the `string_view` of `font_name`, which borrows from the document
/// that produced the style — copying it here is the point.
template <typename T>
NSString *_Nullable box_string(const std::optional<T> &value) {
  return value.has_value() ? to_nsstring(*value) : nil;
}

} // namespace

#pragma mark - ODRMeasure

@implementation ODRMeasure {
  std::optional<odr::Measure> _handle;
}

+ (instancetype)measureWithHandle:(const odr::Measure &)handle {
  ODRMeasure *const result = [[ODRMeasure alloc] init];
  result->_handle = handle;
  return result;
}

- (double)magnitude {
  return guarded_value([&] { return _handle->magnitude(); }, 0.0);
}

- (NSString *)unit {
  return guarded_value([&] { return to_nsstring(_handle->unit().name()); },
                       @"");
}

- (NSString *)stringValue {
  return guarded_value([&] { return to_nsstring(_handle->to_string()); }, @"");
}

- (NSString *)description {
  return self.stringValue;
}

@end

#pragma mark - directional

@implementation ODRDirectionalMeasure

+ (instancetype)directionalWithHandle:
    (const odr::DirectionalStyle<odr::Measure> &)handle {
  ODRDirectionalMeasure *const result = [[ODRDirectionalMeasure alloc] init];
  result->_right = box(handle.right);
  result->_top = box(handle.top);
  result->_left = box(handle.left);
  result->_bottom = box(handle.bottom);
  return result;
}

@end

@implementation ODRDirectionalString

+ (instancetype)directionalWithHandle:
    (const odr::DirectionalStyle<std::string> &)handle {
  ODRDirectionalString *const result = [[ODRDirectionalString alloc] init];
  result->_right = box_string(handle.right);
  result->_top = box_string(handle.top);
  result->_left = box_string(handle.left);
  result->_bottom = box_string(handle.bottom);
  return result;
}

@end

#pragma mark - styles

@implementation ODRTextStyle

+ (instancetype)styleWithHandle:(const odr::TextStyle &)handle {
  ODRTextStyle *const result = [[ODRTextStyle alloc] init];
  result->_fontName = box_string(handle.font_name);
  result->_fontSize = box(handle.font_size);
  result->_fontWeight = box_enum(handle.font_weight);
  result->_fontStyle = box_enum(handle.font_style);
  result->_fontUnderline = box_number(handle.font_underline);
  result->_fontLineThrough = box_number(handle.font_line_through);
  result->_fontShadow = box_string(handle.font_shadow);
  result->_fontColor = box(handle.font_color);
  result->_backgroundColor = box(handle.background_color);
  result->_fontPosition = box_enum(handle.font_position);
  return result;
}

@end

@implementation ODRParagraphStyle

+ (instancetype)styleWithHandle:(const odr::ParagraphStyle &)handle {
  ODRParagraphStyle *const result = [[ODRParagraphStyle alloc] init];
  result->_textAlign = box_enum(handle.text_align);
  result->_margin = [ODRDirectionalMeasure directionalWithHandle:handle.margin];
  result->_lineHeight = box(handle.line_height);
  result->_textIndent = box(handle.text_indent);
  return result;
}

@end

@implementation ODRTableStyle

+ (instancetype)styleWithHandle:(const odr::TableStyle &)handle {
  ODRTableStyle *const result = [[ODRTableStyle alloc] init];
  result->_width = box(handle.width);
  return result;
}

@end

@implementation ODRTableColumnStyle

+ (instancetype)styleWithHandle:(const odr::TableColumnStyle &)handle {
  ODRTableColumnStyle *const result = [[ODRTableColumnStyle alloc] init];
  result->_width = box(handle.width);
  return result;
}

@end

@implementation ODRTableRowStyle

+ (instancetype)styleWithHandle:(const odr::TableRowStyle &)handle {
  ODRTableRowStyle *const result = [[ODRTableRowStyle alloc] init];
  result->_height = box(handle.height);
  return result;
}

@end

@implementation ODRTableCellStyle

+ (instancetype)styleWithHandle:(const odr::TableCellStyle &)handle {
  ODRTableCellStyle *const result = [[ODRTableCellStyle alloc] init];
  result->_horizontalAlign = box_enum(handle.horizontal_align);
  result->_verticalAlign = box_enum(handle.vertical_align);
  result->_backgroundColor = box(handle.background_color);
  result->_padding =
      [ODRDirectionalMeasure directionalWithHandle:handle.padding];
  result->_border = [ODRDirectionalString directionalWithHandle:handle.border];
  result->_textRotation = box_number(handle.text_rotation);
  return result;
}

@end

@implementation ODRGraphicStyle

+ (instancetype)styleWithHandle:(const odr::GraphicStyle &)handle {
  ODRGraphicStyle *const result = [[ODRGraphicStyle alloc] init];
  result->_strokeWidth = box(handle.stroke_width);
  result->_strokeColor = box(handle.stroke_color);
  result->_fillColor = box(handle.fill_color);
  result->_verticalAlign = box_enum(handle.vertical_align);
  result->_textWrap = box_enum(handle.text_wrap);
  result->_horizontalPosition = box_enum(handle.horizontal_position);
  return result;
}

@end

@implementation ODRPageLayout

+ (instancetype)layoutWithHandle:(const odr::PageLayout &)handle {
  ODRPageLayout *const result = [[ODRPageLayout alloc] init];
  result->_width = box(handle.width);
  result->_height = box(handle.height);
  result->_printOrientation = box_enum(handle.print_orientation);
  result->_margin = [ODRDirectionalMeasure directionalWithHandle:handle.margin];
  return result;
}

@end
