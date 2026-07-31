#import <OdrCoreObjC/ODRDocument.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/document.hpp>

#include <optional>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::to_string;

@implementation ODRDocument {
  std::optional<odr::Document> _handle;
}

+ (instancetype)documentWithHandle:(odr::Document)handle {
  ODRDocument *const result = [[ODRDocument alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::Document &)handle {
  return *_handle;
}

- (ODRFileType)fileType {
  return static_cast<ODRFileType>(_handle->file_type());
}

- (ODRDocumentType)documentType {
  return static_cast<ODRDocumentType>(_handle->document_type());
}

- (BOOL)isEditable {
  return guarded_value([&] { return _handle->is_editable() ? YES : NO; }, NO);
}

- (BOOL)isSavable {
  return guarded_value([&] { return _handle->is_savable(false) ? YES : NO; },
                       NO);
}

- (BOOL)isSavableEncrypted {
  return guarded_value([&] { return _handle->is_savable(true) ? YES : NO; },
                       NO);
}

- (BOOL)saveTo:(NSString *)path error:(NSError **)error {
  return guarded(error, [&] {
    _handle->save(to_string(path));
    return YES;
  });
}

- (BOOL)saveTo:(NSString *)path
      password:(NSString *)password
         error:(NSError **)error {
  return guarded(error, [&] {
    _handle->save(to_string(path), to_string(password));
    return YES;
  });
}

- (nullable ODRElement *)rootElementWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRElement * {
    return [ODRElement elementWithHandle:_handle->root_element() owner:self];
  });
}

- (nullable ODRFilesystem *)filesystemWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRFilesystem * {
    return [ODRFilesystem filesystemWithHandle:_handle->as_filesystem()];
  });
}

@end
