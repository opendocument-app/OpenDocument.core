#import <OdrCoreObjC/ODRFilesystem.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/archive.hpp>
#include <odr/filesystem.hpp>

#include <optional>
#include <sstream>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::guarded_void;
using odr::apple::to_nsstring;
using odr::apple::to_string;

#pragma mark - ODRFileWalker

@implementation ODRFileWalker {
  std::optional<odr::FileWalker> _handle;
}

+ (instancetype)walkerWithHandle:(odr::FileWalker)handle {
  ODRFileWalker *const result = [[ODRFileWalker alloc] init];
  result->_handle.emplace(std::move(handle));
  return result;
}

- (BOOL)isAtEnd {
  // YES on failure, so a `while (!isAtEnd)` loop stops instead of spinning
  return guarded_value([&] { return _handle->end() ? YES : NO; }, YES);
}

- (uint32_t)depth {
  return guarded_value([&] { return _handle->depth(); }, 0u);
}

- (NSString *)path {
  return guarded_value([&] { return to_nsstring(_handle->path()); }, @"");
}

- (BOOL)isFile {
  return guarded_value([&] { return _handle->is_file() ? YES : NO; }, NO);
}

- (BOOL)isDirectory {
  return guarded_value([&] { return _handle->is_directory() ? YES : NO; }, NO);
}

- (void)pop {
  guarded_void([&] { _handle->pop(); });
}

- (void)next {
  guarded_void([&] { _handle->next(); });
}

- (void)flatNext {
  guarded_void([&] { _handle->flat_next(); });
}

@end

#pragma mark - ODRFilesystem

@implementation ODRFilesystem {
  std::optional<odr::Filesystem> _handle;
}

+ (instancetype)filesystemWithHandle:(odr::Filesystem)handle {
  ODRFilesystem *const result = [[ODRFilesystem alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::Filesystem &)handle {
  return *_handle;
}

- (BOOL)existsAtPath:(NSString *)path {
  return guarded_value(
      [&] { return _handle->exists(to_string(path)) ? YES : NO; }, NO);
}

- (BOOL)isFileAtPath:(NSString *)path {
  return guarded_value(
      [&] { return _handle->is_file(to_string(path)) ? YES : NO; }, NO);
}

- (BOOL)isDirectoryAtPath:(NSString *)path {
  return guarded_value(
      [&] { return _handle->is_directory(to_string(path)) ? YES : NO; }, NO);
}

- (nullable ODRFileWalker *)walkerAtPath:(NSString *)path
                                   error:(NSError **)error {
  return guarded(error, [&]() -> ODRFileWalker * {
    return
        [ODRFileWalker walkerWithHandle:_handle->file_walker(to_string(path))];
  });
}

- (nullable ODRFile *)openPath:(NSString *)path error:(NSError **)error {
  return guarded(error, [&]() -> ODRFile * {
    return [ODRFile fileWithHandle:_handle->open(to_string(path))];
  });
}

@end

#pragma mark - ODRArchive

@implementation ODRArchive {
  std::optional<odr::Archive> _handle;
}

+ (instancetype)archiveWithHandle:(odr::Archive)handle {
  ODRArchive *const result = [[ODRArchive alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::Archive &)handle {
  return *_handle;
}

- (ODRFilesystem *)filesystem {
  return guarded_value(
      [&]() -> ODRFilesystem * {
        return [ODRFilesystem filesystemWithHandle:_handle->as_filesystem()];
      },
      nil);
}

- (nullable NSData *)dataWithError:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    std::ostringstream out;
    _handle->save(out);
    const std::string bytes = out.str();
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
  });
}

@end
