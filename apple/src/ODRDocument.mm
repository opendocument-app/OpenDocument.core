#import <OdrCoreObjC/ODRDocument.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/document.hpp>

#include <optional>

using odr::apple::guarded;
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
  return _handle->is_editable() ? YES : NO;
}

- (BOOL)isSavable {
  return _handle->is_savable(false) ? YES : NO;
}

- (BOOL)isSavableEncrypted {
  return _handle->is_savable(true) ? YES : NO;
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

@end
