#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ODRFontWeight) {
  ODRFontWeightNormal = 0,
  ODRFontWeightBold,
} NS_SWIFT_NAME(FontWeight);

typedef NS_ENUM(NSInteger, ODRFontStyle) {
  ODRFontStyleNormal = 0,
  ODRFontStyleItalic,
} NS_SWIFT_NAME(FontStyle);

/// Vertical font position — sub/superscript.
typedef NS_ENUM(NSInteger, ODRFontPosition) {
  ODRFontPositionNormal = 0,
  ODRFontPositionSuper,
  ODRFontPositionSub,
} NS_SWIFT_NAME(FontPosition);

typedef NS_ENUM(NSInteger, ODRTextAlign) {
  ODRTextAlignLeft = 0,
  ODRTextAlignRight,
  ODRTextAlignCenter,
  ODRTextAlignJustify,
} NS_SWIFT_NAME(TextAlign);

typedef NS_ENUM(NSInteger, ODRHorizontalAlign) {
  ODRHorizontalAlignLeft = 0,
  ODRHorizontalAlignCenter,
  ODRHorizontalAlignRight,
} NS_SWIFT_NAME(HorizontalAlign);

typedef NS_ENUM(NSInteger, ODRVerticalAlign) {
  ODRVerticalAlignTop = 0,
  ODRVerticalAlignMiddle,
  ODRVerticalAlignBottom,
} NS_SWIFT_NAME(VerticalAlign);

typedef NS_ENUM(NSInteger, ODRPrintOrientation) {
  ODRPrintOrientationPortrait = 0,
  ODRPrintOrientationLandscape,
} NS_SWIFT_NAME(PrintOrientation);

typedef NS_ENUM(NSInteger, ODRTextWrap) {
  ODRTextWrapNone = 0,
  ODRTextWrapBefore,
  ODRTextWrapAfter,
  ODRTextWrapRunThrough,
} NS_SWIFT_NAME(TextWrap);

/// An RGBA colour — `odr::Color`.
///
/// Swift sees `ODRColor`; see the note in ODRTable.h.
typedef struct ODRColor {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} ODRColor;

/// For ObjC callers; Swift has the synthesised memberwise initialiser.
NS_INLINE ODRColor ODRColorMake(uint8_t red, uint8_t green, uint8_t blue,
                                uint8_t alpha) {
  return (ODRColor){.red = red, .green = green, .blue = blue, .alpha = alpha};
}

/// A magnitude with a unit — `odr::Measure`, e.g. `12pt`, `2.5cm`.
///
/// A class rather than a struct because it is almost always optional in a
/// style, and `nil` says "the document did not specify this" far more clearly
/// than a sentinel magnitude would.
NS_SWIFT_NAME(Measure)
@interface ODRMeasure : NSObject
@property(nonatomic, readonly) double magnitude;
/// The unit's name, e.g. `pt`. Empty when the quantity is unitless.
@property(nonatomic, readonly, copy) NSString *unit;
/// Magnitude and unit as odrcore writes them, e.g. `12pt`.
@property(nonatomic, readonly, copy) NSString *stringValue;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Four optional measures — `odr::DirectionalStyle<Measure>`. A `nil` side is
/// one the document did not specify.
NS_SWIFT_NAME(DirectionalMeasure)
@interface ODRDirectionalMeasure : NSObject
@property(nonatomic, readonly, nullable) ODRMeasure *right;
@property(nonatomic, readonly, nullable) ODRMeasure *top;
@property(nonatomic, readonly, nullable) ODRMeasure *left;
@property(nonatomic, readonly, nullable) ODRMeasure *bottom;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// The same for strings — `odr::DirectionalStyle<std::string>`, used for
/// borders.
NS_SWIFT_NAME(DirectionalString)
@interface ODRDirectionalString : NSObject
@property(nonatomic, readonly, nullable, copy) NSString *right;
@property(nonatomic, readonly, nullable, copy) NSString *top;
@property(nonatomic, readonly, nullable, copy) NSString *left;
@property(nonatomic, readonly, nullable, copy) NSString *bottom;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Text style — `odr::TextStyle`.
///
/// Every property is optional, and `nil` means the document did not specify it
/// rather than that it is off. The enum-valued ones are boxed in `NSNumber`
/// for that reason; unbox with `ODRFontWeight(rawValue:)`.
NS_SWIFT_NAME(TextStyle)
@interface ODRTextStyle : NSObject
@property(nonatomic, readonly, nullable, copy) NSString *fontName;
@property(nonatomic, readonly, nullable) ODRMeasure *fontSize;
/// `ODRFontWeight`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *fontWeight;
/// `ODRFontStyle`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *fontStyle;
/// `BOOL`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *fontUnderline;
/// `BOOL`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *fontLineThrough;
@property(nonatomic, readonly, nullable, copy) NSString *fontShadow;
/// `ODRColor`, boxed in an `NSValue`.
@property(nonatomic, readonly, nullable) NSValue *fontColor;
/// `ODRColor`, boxed in an `NSValue`.
@property(nonatomic, readonly, nullable) NSValue *backgroundColor;
/// `ODRFontPosition`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *fontPosition;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Paragraph style — `odr::ParagraphStyle`.
NS_SWIFT_NAME(ParagraphStyle)
@interface ODRParagraphStyle : NSObject
/// `ODRTextAlign`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *textAlign;
@property(nonatomic, readonly) ODRDirectionalMeasure *margin;
@property(nonatomic, readonly, nullable) ODRMeasure *lineHeight;
@property(nonatomic, readonly, nullable) ODRMeasure *textIndent;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Table style — `odr::TableStyle`.
NS_SWIFT_NAME(TableStyle)
@interface ODRTableStyle : NSObject
@property(nonatomic, readonly, nullable) ODRMeasure *width;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Table column style — `odr::TableColumnStyle`.
NS_SWIFT_NAME(TableColumnStyle)
@interface ODRTableColumnStyle : NSObject
@property(nonatomic, readonly, nullable) ODRMeasure *width;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Table row style — `odr::TableRowStyle`.
NS_SWIFT_NAME(TableRowStyle)
@interface ODRTableRowStyle : NSObject
@property(nonatomic, readonly, nullable) ODRMeasure *height;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Table cell style — `odr::TableCellStyle`.
NS_SWIFT_NAME(TableCellStyle)
@interface ODRTableCellStyle : NSObject
/// `ODRHorizontalAlign`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *horizontalAlign;
/// `ODRVerticalAlign`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *verticalAlign;
/// `ODRColor`, boxed in an `NSValue`.
@property(nonatomic, readonly, nullable) NSValue *backgroundColor;
@property(nonatomic, readonly) ODRDirectionalMeasure *padding;
@property(nonatomic, readonly) ODRDirectionalString *border;
/// `double`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *textRotation;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Graphic style — `odr::GraphicStyle`.
NS_SWIFT_NAME(GraphicStyle)
@interface ODRGraphicStyle : NSObject
@property(nonatomic, readonly, nullable) ODRMeasure *strokeWidth;
/// `ODRColor`, boxed in an `NSValue`.
@property(nonatomic, readonly, nullable) NSValue *strokeColor;
/// `ODRColor`, boxed in an `NSValue`.
@property(nonatomic, readonly, nullable) NSValue *fillColor;
/// `ODRVerticalAlign`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *verticalAlign;
/// `ODRTextWrap`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *textWrap;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Page layout — `odr::PageLayout`.
NS_SWIFT_NAME(PageLayout)
@interface ODRPageLayout : NSObject
@property(nonatomic, readonly, nullable) ODRMeasure *width;
@property(nonatomic, readonly, nullable) ODRMeasure *height;
/// `ODRPrintOrientation`, boxed.
@property(nonatomic, readonly, nullable) NSNumber *printOrientation;
@property(nonatomic, readonly) ODRDirectionalMeasure *margin;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

NS_ASSUME_NONNULL_END
