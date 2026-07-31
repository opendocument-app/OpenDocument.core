#import <OdrCoreObjC/ODROdr.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/odr.hpp>

#include <span>
#include <string_view>
#include <vector>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::to_nsstring;
using odr::apple::to_string;

namespace {

NSArray<NSString *> *to_nsarray(std::span<const std::string_view> strings) {
  NSMutableArray<NSString *> *const result =
      [NSMutableArray arrayWithCapacity:strings.size()];
  for (const std::string_view string : strings) {
    [result addObject:to_nsstring(string)];
  }
  return result;
}

} // namespace

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

+ (NSArray<NSNumber *> *)allFileTypes {
  return guarded_value(
      [&]() -> NSArray<NSNumber *> * {
        const std::vector<odr::FileType> types = odr::all_file_types();
        NSMutableArray<NSNumber *> *const result =
            [NSMutableArray arrayWithCapacity:types.size()];
        for (const odr::FileType type : types) {
          [result addObject:@(static_cast<NSInteger>(type))];
        }
        return result;
      },
      @[]);
}

+ (ODRFileType)fileTypeForExtension:(NSString *)extension {
  return guarded_value(
      [&] {
        return static_cast<ODRFileType>(
            odr::file_type_by_file_extension(to_string(extension)));
      },
      ODRFileTypeUnknown);
}

+ (NSArray<NSString *> *)extensionsForFileType:(ODRFileType)type {
  return guarded_value(
      [&] {
        return to_nsarray(odr::file_extensions_by_file_type(
            static_cast<odr::FileType>(type)));
      },
      @[]);
}

+ (nullable NSString *)extensionForFileType:(ODRFileType)type
                                      error:(NSError **)error {
  return guarded(error, [&]() -> NSString * {
    return to_nsstring(
        odr::file_extension_by_file_type(static_cast<odr::FileType>(type)));
  });
}

+ (ODRFileType)fileTypeForMimetype:(NSString *)mimetype {
  return guarded_value(
      [&] {
        return static_cast<ODRFileType>(
            odr::file_type_by_mimetype(to_string(mimetype)));
      },
      ODRFileTypeUnknown);
}

+ (NSArray<NSString *> *)mimetypesForFileType:(ODRFileType)type {
  return guarded_value(
      [&] {
        return to_nsarray(
            odr::mimetypes_by_file_type(static_cast<odr::FileType>(type)));
      },
      @[]);
}

+ (nullable NSString *)mimetypeForFileType:(ODRFileType)type
                                     error:(NSError **)error {
  return guarded(error, [&]() -> NSString * {
    return to_nsstring(
        odr::mimetype_by_file_type(static_cast<odr::FileType>(type)));
  });
}

+ (ODRFileCategory)fileCategoryForFileType:(ODRFileType)type {
  return guarded_value(
      [&] {
        return static_cast<ODRFileCategory>(
            odr::file_category_by_file_type(static_cast<odr::FileType>(type)));
      },
      ODRFileCategoryUnknown);
}

+ (ODRDocumentType)documentTypeForFileType:(ODRFileType)type {
  return guarded_value(
      [&] {
        return static_cast<ODRDocumentType>(
            odr::document_type_by_file_type(static_cast<odr::FileType>(type)));
      },
      ODRDocumentTypeUnknown);
}

+ (ODRFileTypeCapabilities *)capabilitiesForFileType:(ODRFileType)type {
  return guarded_value(
      [&]() -> ODRFileTypeCapabilities * {
        return [ODRFileTypeCapabilities
            capabilitiesWithHandle:odr::capabilities_by_file_type(
                                       static_cast<odr::FileType>(type))];
      },
      nil);
}

+ (NSString *)stringForFileType:(ODRFileType)type {
  return guarded_value(
      [&] {
        return to_nsstring(
            odr::file_type_to_string(static_cast<odr::FileType>(type)));
      },
      @"");
}

+ (NSString *)stringForFileCategory:(ODRFileCategory)category {
  return guarded_value(
      [&] {
        return to_nsstring(odr::file_category_to_string(
            static_cast<odr::FileCategory>(category)));
      },
      @"");
}

+ (NSString *)stringForDocumentType:(ODRDocumentType)type {
  return guarded_value(
      [&] {
        return to_nsstring(
            odr::document_type_to_string(static_cast<odr::DocumentType>(type)));
      },
      @"");
}

@end
