#import <Foundation/Foundation.h>

#import <OdrCoreObjC/ODRStyle.h>
#import <OdrCoreObjC/ODRTable.h>

NS_ASSUME_NONNULL_BEGIN

@class ODRFile;

typedef NS_ENUM(NSInteger, ODRListType) {
  ODRListTypeUnordered,
  ODRListTypeOrdered,
} NS_SWIFT_NAME(ListType);

typedef NS_ENUM(NSInteger, ODRElementType) {
  ODRElementTypeNone = 0,

  ODRElementTypeRoot,
  ODRElementTypeSlide,
  ODRElementTypeSheet,
  ODRElementTypePage,

  ODRElementTypeMasterPage,

  ODRElementTypeSheetCell,

  ODRElementTypeText,
  ODRElementTypeLineBreak,
  ODRElementTypePageBreak,
  ODRElementTypeParagraph,
  ODRElementTypeSpan,
  ODRElementTypeLink,
  ODRElementTypeBookmark,

  ODRElementTypeList,
  ODRElementTypeListItem,

  ODRElementTypeTable,
  ODRElementTypeTableColumn,
  ODRElementTypeTableRow,
  ODRElementTypeTableCell,

  ODRElementTypeFrame,
  ODRElementTypeImage,
  ODRElementTypeGroup,
} NS_SWIFT_NAME(ElementType);

typedef NS_ENUM(NSInteger, ODRShapeType) {
  ODRShapeTypeNone = 0,
  ODRShapeTypeRect,
  ODRShapeTypeEllipse,
  ODRShapeTypeLine,
  ODRShapeTypeCustom,
} NS_SWIFT_NAME(ShapeType);

typedef NS_ENUM(NSInteger, ODRAnchorType) {
  ODRAnchorTypeAsChar = 0,
  ODRAnchorTypeAtChar,
  ODRAnchorTypeAtFrame,
  ODRAnchorTypeAtPage,
  ODRAnchorTypeAtParagraph,
} NS_SWIFT_NAME(AnchorType);

typedef NS_ENUM(NSInteger, ODRValueType) {
  ODRValueTypeUnknown = 0,
  ODRValueTypeString,
  ODRValueTypeFloatNumber,
} NS_SWIFT_NAME(ValueType);

/// A node in a document's element tree — `odr::Element`.
///
/// Navigation returns the most derived class the node qualifies for, so a
/// `first(where: { $0 is Paragraph })` gives you something you can use without
/// casting.
///
/// An element does **not** own the document it came from — `odr::Element` holds
/// a bare pointer into the document's adapter. Every element therefore keeps a
/// strong reference to its document, and navigating from one element to another
/// carries that reference along, so the tree cannot outlive what it points
/// into.
NS_SWIFT_NAME(Element)
@interface ODRElement : NSObject

@property(nonatomic, readonly) ODRElementType type;
/// `NO` for the element you get past the end of the tree.
@property(nonatomic, readonly) BOOL exists;

@property(nonatomic, readonly, nullable) ODRElement *parent;
@property(nonatomic, readonly, nullable) ODRElement *firstChild;
@property(nonatomic, readonly, nullable) ODRElement *previousSibling;
@property(nonatomic, readonly, nullable) ODRElement *nextSibling;
/// Every child, in order.
@property(nonatomic, readonly) NSArray<ODRElement *> *children;

@property(nonatomic, readonly) BOOL isUnique;
@property(nonatomic, readonly) BOOL isSelfLocatable;
@property(nonatomic, readonly) BOOL isEditable;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

/// The root of a text document — `odr::TextRoot`.
NS_SWIFT_NAME(TextRoot)
@interface ODRTextRoot : ODRElement
@property(nonatomic, readonly) ODRPageLayout *pageLayout;
@property(nonatomic, readonly, nullable) ODRElement *firstMasterPage;
@end

/// `odr::Slide`.
NS_SWIFT_NAME(Slide)
@interface ODRSlide : ODRElement
@property(nonatomic, readonly, copy) NSString *name;
@property(nonatomic, readonly) ODRPageLayout *pageLayout;
@property(nonatomic, readonly, nullable) ODRElement *masterPage;
@end

/// `odr::Sheet`.
NS_SWIFT_NAME(Sheet)
@interface ODRSheet : ODRElement
@property(nonatomic, readonly, copy) NSString *name;
/// The paper the sheet is printed on, where the file states one.
@property(nonatomic, readonly) ODRPageLayout *pageLayout;
@property(nonatomic, readonly) ODRTableDimensions dimensions;
/// The dimensions actually filled with content, optionally within `range`.
- (ODRTableDimensions)contentDimensions NS_SWIFT_NAME(contentDimensions());
- (ODRTableDimensions)contentDimensionsInRange:(ODRTableDimensions)range
    NS_SWIFT_NAME(contentDimensions(in:));
- (nullable ODRElement *)cellAtColumn:(uint32_t)column
                                  row:(uint32_t)row
    NS_SWIFT_NAME(cell(column:row:));
/// The floating shapes on the sheet, which are not part of the cell grid.
@property(nonatomic, readonly) NSArray<ODRElement *> *shapes;
@property(nonatomic, readonly) ODRTableStyle *style;
- (ODRTableColumnStyle *)styleForColumn:(uint32_t)column
    NS_SWIFT_NAME(style(column:));
- (ODRTableRowStyle *)styleForRow:(uint32_t)row NS_SWIFT_NAME(style(row:));
- (ODRTableCellStyle *)styleForCellAtColumn:(uint32_t)column
                                        row:(uint32_t)row
    NS_SWIFT_NAME(style(column:row:));
@end

/// `odr::SheetCell`.
NS_SWIFT_NAME(SheetCell)
@interface ODRSheetCell : ODRElement
@property(nonatomic, readonly) ODRTablePosition position;
/// Covered by another cell's span.
@property(nonatomic, readonly) BOOL isCovered;
@property(nonatomic, readonly) ODRTableDimensions span;
@property(nonatomic, readonly) ODRValueType valueType;
@end

/// `odr::Page`.
NS_SWIFT_NAME(Page)
@interface ODRPage : ODRElement
@property(nonatomic, readonly, copy) NSString *name;
@property(nonatomic, readonly) ODRPageLayout *pageLayout;
@property(nonatomic, readonly, nullable) ODRElement *masterPage;
@end

/// `odr::MasterPage`.
NS_SWIFT_NAME(MasterPage)
@interface ODRMasterPage : ODRElement
@property(nonatomic, readonly) ODRPageLayout *pageLayout;
@end

/// `odr::LineBreak`.
NS_SWIFT_NAME(LineBreak)
@interface ODRLineBreak : ODRElement
@property(nonatomic, readonly) ODRTextStyle *style;
@end

/// `odr::Paragraph`.
NS_SWIFT_NAME(Paragraph)
@interface ODRParagraph : ODRElement
@property(nonatomic, readonly) ODRParagraphStyle *style;
@property(nonatomic, readonly) ODRTextStyle *textStyle;
@end

/// `odr::Span`.
NS_SWIFT_NAME(Span)
@interface ODRSpan : ODRElement
@property(nonatomic, readonly) ODRTextStyle *style;
@end

/// A run of text — `odr::Text`.
NS_SWIFT_NAME(Text)
@interface ODRText : ODRElement
@property(nonatomic, readonly, copy) NSString *content;
/// Replaces the text. Only meaningful on an editable document.
- (BOOL)setContent:(NSString *)content error:(NSError **)error;
@property(nonatomic, readonly) ODRTextStyle *style;
@end

/// `odr::Link`.
NS_SWIFT_NAME(Link)
@interface ODRLink : ODRElement
@property(nonatomic, readonly, copy) NSString *href;
@end

/// `odr::Bookmark`.
NS_SWIFT_NAME(Bookmark)
@interface ODRBookmark : ODRElement
@property(nonatomic, readonly, copy) NSString *name;
@end

/// `odr::List`.
NS_SWIFT_NAME(List)
@interface ODRList : ODRElement
/// Named apart from `ODRElement.type`, which every element answers.
@property(nonatomic, readonly) ODRListType listType;
@end

/// `odr::ListItem`.
NS_SWIFT_NAME(ListItem)
@interface ODRListItem : ODRElement
@property(nonatomic, readonly) ODRTextStyle *style;
/// The resolved label, or empty where the list style asks for none.
@property(nonatomic, readonly, copy) NSString *marker;
/// The counter behind `marker`, `nil` for an unordered item.
@property(nonatomic, readonly, nullable) NSNumber *number;
@end

/// `odr::Table`.
NS_SWIFT_NAME(Table)
@interface ODRTable : ODRElement
@property(nonatomic, readonly, nullable) ODRElement *firstRow;
@property(nonatomic, readonly, nullable) ODRElement *firstColumn;
@property(nonatomic, readonly) NSArray<ODRElement *> *columns;
@property(nonatomic, readonly) NSArray<ODRElement *> *rows;
@property(nonatomic, readonly) ODRTableDimensions dimensions;
@property(nonatomic, readonly) ODRTableStyle *style;
@end

/// `odr::TableColumn`.
NS_SWIFT_NAME(TableColumn)
@interface ODRTableColumn : ODRElement
@property(nonatomic, readonly) ODRTableColumnStyle *style;
@end

/// `odr::TableRow`.
NS_SWIFT_NAME(TableRow)
@interface ODRTableRow : ODRElement
@property(nonatomic, readonly) ODRTableRowStyle *style;
@end

/// `odr::TableCell`.
NS_SWIFT_NAME(TableCell)
@interface ODRTableCell : ODRElement
@property(nonatomic, readonly) BOOL isCovered;
@property(nonatomic, readonly) ODRTableDimensions span;
@property(nonatomic, readonly) ODRValueType valueType;
@property(nonatomic, readonly) ODRTableCellStyle *style;
@end

/// `odr::DrawingPath`: an outline, in the user-space box `x`/`y`/`width`/
/// `height` that the shape's own box stretches to.
NS_SWIFT_NAME(DrawingPath)
@interface ODRDrawingPath : NSObject
@property(nonatomic, readonly, copy) NSString *data;
@property(nonatomic, readonly) double x;
@property(nonatomic, readonly) double y;
@property(nonatomic, readonly) double width;
@property(nonatomic, readonly) double height;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// `odr::DrawingLine`: the two ends of a line shape, in the parent's space.
NS_SWIFT_NAME(DrawingLine)
@interface ODRDrawingLine : NSObject
@property(nonatomic, readonly) ODRMeasure *x1;
@property(nonatomic, readonly) ODRMeasure *y1;
@property(nonatomic, readonly) ODRMeasure *x2;
@property(nonatomic, readonly) ODRMeasure *y2;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// `odr::DrawingTransform`.
NS_SWIFT_NAME(DrawingTransform)
@interface ODRDrawingTransform : NSObject
@property(nonatomic, readonly) double a;
@property(nonatomic, readonly) double b;
@property(nonatomic, readonly) double c;
@property(nonatomic, readonly) double d;
@property(nonatomic, readonly) ODRMeasure *e;
@property(nonatomic, readonly) ODRMeasure *f;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// `odr::Frame`.
NS_SWIFT_NAME(Frame)
@interface ODRFrame : ODRElement
@property(nonatomic, readonly) ODRShapeType shapeType;
@property(nonatomic, readonly) ODRAnchorType anchorType;
@property(nonatomic, readonly, nullable) ODRMeasure *x;
@property(nonatomic, readonly, nullable) ODRMeasure *y;
@property(nonatomic, readonly, nullable) ODRMeasure *width;
@property(nonatomic, readonly, nullable) ODRMeasure *height;
/// `int32_t`, boxed; `nil` when the document did not set one.
@property(nonatomic, readonly, nullable) NSNumber *zIndex;
@property(nonatomic, readonly, nullable) ODRDrawingTransform *transform;
/// `nil` for a shape whose geometry we cannot read, leaving its box.
@property(nonatomic, readonly, nullable) ODRDrawingPath *path;
/// The ends of an `ODRShapeTypeLine`, which states them instead of a box.
@property(nonatomic, readonly, nullable) ODRDrawingLine *line;
@property(nonatomic, readonly) ODRGraphicStyle *style;
@end

/// `odr::Image`.
NS_SWIFT_NAME(Image)
@interface ODRImage : ODRElement
/// Stored inside the document rather than referenced from outside.
@property(nonatomic, readonly) BOOL isInternal;
/// The backing file for an internal image.
@property(nonatomic, readonly, nullable) ODRFile *file;
@property(nonatomic, readonly, copy) NSString *href;
@end

NS_ASSUME_NONNULL_END
