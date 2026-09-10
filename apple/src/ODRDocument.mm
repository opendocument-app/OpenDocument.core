#import <OdrCoreObjC/ODRDocument.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/document.hpp>
#include <odr/document_element.hpp>

#include <optional>
#include <sstream>
#include <string>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::to_string;

@interface ODRDocument ()
- (nullable ODRText *)wrapText:(odr::Text)handle;
- (nullable ODRParagraph *)wrapParagraph:(odr::Paragraph)handle;
@end

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

- (BOOL)edit:(NSString *)operations error:(NSError **)error {
  return guarded(error, [&] {
    _handle->edit(to_string(operations));
    return YES;
  });
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

- (nullable NSData *)saveToMemoryWithError:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    std::ostringstream out;
    _handle->save(out);
    const std::string bytes = out.str();
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
  });
}

- (nullable NSData *)saveToMemoryWithPassword:(NSString *)password
                                        error:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    std::ostringstream out;
    _handle->save(out, to_string(password));
    const std::string bytes = out.str();
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
  });
}

- (nullable ODRElement *)rootElementWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRElement * {
    return [ODRElement elementWithHandle:_handle->root_element() owner:self];
  });
}

- (nullable ODRElement *)elementWithIdentifier:(uint64_t)identifier {
  return guarded_value(
      [&]() -> ODRElement * {
        return [ODRElement elementWithHandle:_handle->element_by_id(identifier)
                                       owner:self];
      },
      nil);
}

#pragma mark - Structural edits

- (BOOL)removeElement:(ODRElement *)element error:(NSError **)error {
  return guarded(error, [&] {
    _handle->remove(element.handle);
    return YES;
  });
}

/// The run the structural edits hand back, wrapped in the class the picker
/// gives it — `ODRText` for every one of them.
- (nullable ODRText *)wrapText:(odr::Text)handle {
  return static_cast<ODRText *>([ODRElement elementWithHandle:std::move(handle)
                                                        owner:self]);
}

- (nullable ODRParagraph *)wrapParagraph:(odr::Paragraph)handle {
  return static_cast<ODRParagraph *>(
      [ODRElement elementWithHandle:std::move(handle) owner:self]);
}

- (nullable ODRText *)insertTextBefore:(ODRText *)anchor
                                  text:(NSString *)text
                                 error:(NSError **)error {
  return guarded(error, [&]() -> ODRText * {
    return [self wrapText:_handle->insert_text_before(anchor.handle.as_text(),
                                                      to_string(text))];
  });
}

- (nullable ODRText *)insertTextAfter:(ODRText *)anchor
                                 text:(NSString *)text
                                error:(NSError **)error {
  return guarded(error, [&]() -> ODRText * {
    return [self wrapText:_handle->insert_text_after(anchor.handle.as_text(),
                                                     to_string(text))];
  });
}

- (nullable ODRText *)appendTextTo:(ODRElement *)parent
                              text:(NSString *)text
                             error:(NSError **)error {
  return guarded(error, [&]() -> ODRText * {
    return [self wrapText:_handle->append_text(parent.handle, to_string(text))];
  });
}

- (nullable ODRParagraph *)splitParagraph:(ODRParagraph *)paragraph
                                    after:(nullable ODRElement *)after
                                    error:(NSError **)error {
  return guarded(error, [&]() -> ODRParagraph * {
    return
        [self wrapParagraph:_handle->split_paragraph(
                                paragraph.handle.as_paragraph(),
                                after == nil ? odr::Element() : after.handle)];
  });
}

- (BOOL)mergeParagraphWithNext:(ODRParagraph *)paragraph
                         error:(NSError **)error {
  return guarded(error, [&] {
    _handle->merge_paragraph_with_next(paragraph.handle.as_paragraph());
    return YES;
  });
}

- (nullable ODRParagraph *)insertParagraphAfter:(ODRParagraph *)paragraph
                                          error:(NSError **)error {
  return guarded(error, [&]() -> ODRParagraph * {
    return [self wrapParagraph:_handle->insert_paragraph_after(
                                   paragraph.handle.as_paragraph())];
  });
}

- (nullable ODRFilesystem *)filesystemWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRFilesystem * {
    return [ODRFilesystem filesystemWithHandle:_handle->as_filesystem()];
  });
}

@end
