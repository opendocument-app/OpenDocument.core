#pragma once

#import <OdrCoreObjC/ODRDocument.h>
#import <OdrCoreObjC/ODRDocumentElement.h>
#import <OdrCoreObjC/ODRFile.h>
#import <OdrCoreObjC/ODRFilesystem.h>
#import <OdrCoreObjC/ODRHtml.h>
#import <OdrCoreObjC/ODRLogger.h>
#import <OdrCoreObjC/ODRStyle.h>

#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <optional>

/// Cross-translation-unit access to the C++ value each wrapper owns.
///
/// Each `@implementation` holds its handle as an ivar, destroyed by ARC's
/// `.cxx_destruct` — no `long` handles as in `jni/`. Most handles own a
/// `shared_ptr`, so a wrapper holding one needs no keep-alive; the exceptions
/// are `ODRElement` and `ODRHtmlView` below.
///
/// Categories cannot add ivars, so each class declares its own accessors here
/// and implements them next to its `@implementation`.

NS_ASSUME_NONNULL_BEGIN

@interface ODRFile (Private)
+ (instancetype)fileWithHandle:(odr::File)handle;
- (const odr::File &)handle;
@end

@interface ODRDecodedFile (Private)
/// Wraps `handle` in the most derived class it qualifies for, so a caller that
/// asks for a decoded file gets an `ODRDocumentFile` when it is one.
+ (instancetype)decodedFileWithHandle:(odr::DecodedFile)handle;
- (const odr::DecodedFile &)handle;
@end

@interface ODRDocumentFile (Private)
/// By value, not by reference: like the JNI bindings, a typed view is
/// re-derived from the base handle on each call rather than stored, so there is
/// never a derived subobject sitting behind a base-typed handle.
- (odr::DocumentFile)documentHandle;
@end

@interface ODRDocument (Private)
+ (instancetype)documentWithHandle:(odr::Document)handle;
- (const odr::Document &)handle;
@end

@interface ODRLogger (Private)
+ (instancetype)loggerWithHandle:(odr::Logger)handle;
- (const odr::Logger &)handle;
@end

@interface ODRFileWalker (Private)
+ (instancetype)walkerWithHandle:(odr::FileWalker)handle;
@end

@interface ODRFilesystem (Private)
+ (instancetype)filesystemWithHandle:(odr::Filesystem)handle;
- (const odr::Filesystem &)handle;
@end

@interface ODRArchive (Private)
+ (instancetype)archiveWithHandle:(odr::Archive)handle;
- (const odr::Archive &)handle;
@end

@interface ODRElement (Private)
/// `nil` for an element that does not exist, so a caller never holds a handle
/// to nothing. `owner` is what keeps the document alive behind the bare
/// adapter pointer inside `odr::Element`.
+ (nullable ODRElement *)elementWithHandle:(odr::Element)handle owner:(id)owner;
- (const odr::Element &)handle;
- (id)owner;
/// Wraps a navigation result, carrying this element's owner along.
- (nullable ODRElement *)derive:(odr::Element)handle;
@end

@interface ODRMeasure (Private)
+ (instancetype)measureWithHandle:(const odr::Measure &)handle;
- (const std::optional<odr::Measure> &)handle;
@end

@interface ODRDirectionalMeasure (Private)
+ (instancetype)directionalWithHandle:
    (const odr::DirectionalStyle<odr::Measure> &)handle;
- (odr::DirectionalStyle<odr::Measure>)handle;
@end

@interface ODRDirectionalString (Private)
+ (instancetype)directionalWithHandle:
    (const odr::DirectionalStyle<std::string> &)handle;
@end

@interface ODRTextStyle (Private)
+ (instancetype)styleWithHandle:(const odr::TextStyle &)handle;
@end

@interface ODRParagraphStyle (Private)
+ (instancetype)styleWithHandle:(const odr::ParagraphStyle &)handle;
@end

@interface ODRTableStyle (Private)
+ (instancetype)styleWithHandle:(const odr::TableStyle &)handle;
@end

@interface ODRTableColumnStyle (Private)
+ (instancetype)styleWithHandle:(const odr::TableColumnStyle &)handle;
@end

@interface ODRTableRowStyle (Private)
+ (instancetype)styleWithHandle:(const odr::TableRowStyle &)handle;
@end

@interface ODRTableCellStyle (Private)
+ (instancetype)styleWithHandle:(const odr::TableCellStyle &)handle;
@end

@interface ODRGraphicStyle (Private)
+ (instancetype)styleWithHandle:(const odr::GraphicStyle &)handle;
@end

@interface ODRPageLayout (Private)
+ (instancetype)layoutWithHandle:(const odr::PageLayout &)handle;
@end

namespace odr::apple {
/// Boxing helpers shared by the style and element bindings.
NSValue *_Nullable box(const std::optional<odr::Color> &color);
ODRMeasure *_Nullable box(const std::optional<odr::Measure> &measure);
} // namespace odr::apple

@interface ODRHtmlConfig (Private)
- (instancetype)initWithNativeConfig:(const odr::HtmlConfig &)config;
- (odr::HtmlConfig)nativeConfig;
@end

@interface ODRHtmlResource (Private)
+ (instancetype)resourceWithHandle:(odr::HtmlResource)handle
                          location:(odr::HtmlResourceLocation)location;
@end

@interface ODRHtmlPage (Private)
+ (instancetype)pageWithHandle:(const odr::HtmlPage &)handle;
@end

@interface ODRHtml (Private)
+ (instancetype)htmlWithHandle:(const odr::Html &)handle;
@end

@interface ODRHtmlSheetCut (Private)
+ (instancetype)cutWithHandle:(const odr::HtmlSheetCut &)handle;
@end

@interface ODRHtmlView (Private)
+ (instancetype)viewWithHandle:(odr::HtmlView)handle owner:(id)owner;
- (const odr::HtmlView &)handle;
@end

@interface ODRHtmlService (Private)
+ (instancetype)serviceWithHandle:(odr::HtmlService)handle;
- (const odr::HtmlService &)handle;
@end

@interface ODRFileMeta (Private)
+ (instancetype)metaWithHandle:(const odr::FileMeta &)handle;
@end

@interface ODRFileTypeCapabilities (Private)
+ (instancetype)capabilitiesWithHandle:
    (const odr::FileTypeCapabilities &)handle;
@end

NS_ASSUME_NONNULL_END
