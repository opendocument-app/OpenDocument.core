#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// The size of a table — `odr::TableDimensions`.
///
/// A plain C struct rather than a class: it is two integers with no identity,
/// and Swift imports it as a value type.
typedef struct ODRTableDimensions {
  uint32_t rows;
  uint32_t columns;
} ODRTableDimensions NS_SWIFT_NAME(TableDimensions);

NS_INLINE ODRTableDimensions ODRTableDimensionsMake(uint32_t rows,
                                                    uint32_t columns)
    NS_SWIFT_NAME(TableDimensions.init(rows:columns:)) {
  return (ODRTableDimensions){.rows = rows, .columns = columns};
}

/// A cell address — `odr::TablePosition`.
typedef struct ODRTablePosition {
  uint32_t column;
  uint32_t row;
} ODRTablePosition NS_SWIFT_NAME(TablePosition);

NS_INLINE ODRTablePosition ODRTablePositionMake(uint32_t column, uint32_t row)
    NS_SWIFT_NAME(TablePosition.init(column:row:)) {
  return (ODRTablePosition){.column = column, .row = row};
}

/// Spreadsheet-style conversions, e.g. `"C5"` ↔ column 2, row 4.
NS_SWIFT_NAME(TableAddress)
@interface ODRTableAddress : NSObject

+ (uint32_t)columnNumberFromString:(NSString *)string
    NS_SWIFT_NAME(columnNumber(from:));
+ (uint32_t)rowNumberFromString:(NSString *)string
    NS_SWIFT_NAME(rowNumber(from:));
+ (NSString *)stringFromColumnNumber:(uint32_t)column
    NS_SWIFT_NAME(string(fromColumn:));
+ (NSString *)stringFromRowNumber:(uint32_t)row NS_SWIFT_NAME(string(fromRow:));

/// `"C5"` for column 2, row 4.
+ (NSString *)stringFromPosition:(ODRTablePosition)position
    NS_SWIFT_NAME(string(from:));
/// Parses `"C5"`. Fails on anything that is not a cell address.
+ (BOOL)position:(ODRTablePosition *)position
      fromString:(NSString *)string
           error:(NSError **)error NS_SWIFT_NAME(position(_:from:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
