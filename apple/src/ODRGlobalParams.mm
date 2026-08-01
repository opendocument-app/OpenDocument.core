#import <OdrCoreObjC/ODRGlobalParams.h>

#import "ODRInternal.h"

#include <odr/global_params.hpp>

@implementation ODRGlobalParams

+ (NSString *)odrCoreDataPath {
  return odr::apple::guarded_value(
      [&] {
        return odr::apple::to_nsstring(odr::GlobalParams::odr_core_data_path());
      },
      @"");
}

+ (void)setOdrCoreDataPath:(NSString *)odrCoreDataPath {
  odr::apple::guarded_void([&] {
    odr::GlobalParams::set_odr_core_data_path(
        odr::apple::to_string(odrCoreDataPath));
  });
}

+ (NSString *)libmagicDatabasePath {
  return odr::apple::guarded_value(
      [&] {
        return odr::apple::to_nsstring(
            odr::GlobalParams::libmagic_database_path());
      },
      @"");
}

+ (void)setLibmagicDatabasePath:(NSString *)libmagicDatabasePath {
  odr::apple::guarded_void([&] {
    odr::GlobalParams::set_libmagic_database_path(
        odr::apple::to_string(libmagicDatabasePath));
  });
}

+ (void)bootstrapFromFrameworkBundle {
  NSBundle *const bundle = [NSBundle bundleForClass:self];

  // Flat at the bundle root on iOS, `Versions/A/Resources` on macOS — never
  // assume the layout, ask NSBundle.
  NSString *const resources = bundle.resourcePath;
  if (resources != nil) {
    ODRGlobalParams.odrCoreDataPath = resources;
  }

  // Only a framework built against the deprecated libmagic carries the
  // database, so normally there is nothing to point at.
  NSString *const magic = [bundle pathForResource:@"magic" ofType:@"mgc"];
  if (magic != nil) {
    ODRGlobalParams.libmagicDatabasePath = magic;
  }
}

@end
