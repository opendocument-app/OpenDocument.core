#import <OdrCoreObjC/ODROdr.h>

#import "ODRInternal.h"

#include <odr/odr.hpp>

using odr::apple::guarded_value;

@implementation ODROdr

+ (NSString *)libraryVersion {
  return guarded_value([&] { return odr::apple::to_nsstring(odr::version()); },
                       @"");
}

+ (NSString *)commitHash {
  return guarded_value(
      [&] { return odr::apple::to_nsstring(odr::commit_hash()); }, @"");
}

+ (BOOL)isDirty {
  return odr::is_dirty() ? YES : NO;
}

+ (BOOL)isDebug {
  return odr::is_debug() ? YES : NO;
}

+ (NSString *)identification {
  return guarded_value([&] { return odr::apple::to_nsstring(odr::identify()); },
                       @"");
}

@end
