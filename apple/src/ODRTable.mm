#import <OdrCoreObjC/ODRTable.h>

#import "ODRInternal.h"

#include <cstring>

#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::to_nsstring;
using odr::apple::to_string;

static_assert(sizeof(ODRTableDimensions) == sizeof(odr::TableDimensions),
              "ODRTableDimensions drifted from odr::TableDimensions");
static_assert(sizeof(ODRTablePosition) == sizeof(odr::TablePosition),
              "ODRTablePosition drifted from odr::TablePosition");

@implementation NSValue (ODRTableDimensions)

+ (NSValue *)odr_valueWithTableDimensions:(ODRTableDimensions)dimensions {
  return [NSValue valueWithBytes:&dimensions
                        objCType:@encode(ODRTableDimensions)];
}

- (ODRTableDimensions)odr_tableDimensionsValue {
  ODRTableDimensions dimensions = ODRTableDimensionsMake(0, 0);
  if (strcmp(self.objCType, @encode(ODRTableDimensions)) == 0) {
    [self getValue:&dimensions size:sizeof(dimensions)];
  }
  return dimensions;
}

@end

@implementation ODRTableAddress

// The parses throw on anything that is not a cell address — an empty string, a
// lowercase column, a non-numeric row. These have no error out-parameter, so
// they fall back to 0; `position:fromString:` is the one that reports why.
+ (uint32_t)columnNumberFromString:(NSString *)string {
  return guarded_value(
      [&] { return odr::TablePosition::to_column_num(to_string(string)); }, 0u);
}

+ (uint32_t)rowNumberFromString:(NSString *)string {
  return guarded_value(
      [&] { return odr::TablePosition::to_row_num(to_string(string)); }, 0u);
}

+ (NSString *)stringFromColumnNumber:(uint32_t)column {
  return guarded_value(
      [&] { return to_nsstring(odr::TablePosition::to_column_string(column)); },
      @"");
}

+ (NSString *)stringFromRowNumber:(uint32_t)row {
  return guarded_value(
      [&] { return to_nsstring(odr::TablePosition::to_row_string(row)); }, @"");
}

+ (NSString *)stringFromPosition:(ODRTablePosition)position {
  return guarded_value(
      [&] {
        return to_nsstring(
            odr::TablePosition(position.column, position.row).to_string());
      },
      @"");
}

+ (BOOL)position:(ODRTablePosition *)position
      fromString:(NSString *)string
           error:(NSError **)error {
  return guarded(error, [&] {
    const odr::TablePosition parsed{to_string(string)};
    *position = ODRTablePositionMake(parsed.column, parsed.row);
    return YES;
  });
}

@end
