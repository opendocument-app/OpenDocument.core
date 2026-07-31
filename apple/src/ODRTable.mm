#import <OdrCoreObjC/ODRTable.h>

#import "ODRInternal.h"

#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

using odr::apple::guarded;
using odr::apple::to_nsstring;
using odr::apple::to_string;

static_assert(sizeof(ODRTableDimensions) == sizeof(odr::TableDimensions),
              "ODRTableDimensions drifted from odr::TableDimensions");
static_assert(sizeof(ODRTablePosition) == sizeof(odr::TablePosition),
              "ODRTablePosition drifted from odr::TablePosition");

@implementation ODRTableAddress

+ (uint32_t)columnNumberFromString:(NSString *)string {
  return odr::TablePosition::to_column_num(to_string(string));
}

+ (uint32_t)rowNumberFromString:(NSString *)string {
  return odr::TablePosition::to_row_num(to_string(string));
}

+ (NSString *)stringFromColumnNumber:(uint32_t)column {
  return to_nsstring(odr::TablePosition::to_column_string(column));
}

+ (NSString *)stringFromRowNumber:(uint32_t)row {
  return to_nsstring(odr::TablePosition::to_row_string(row));
}

+ (NSString *)stringFromPosition:(ODRTablePosition)position {
  return to_nsstring(
      odr::TablePosition(position.column, position.row).to_string());
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
