#pragma once

#import <OdrCoreObjC/ODRDocument.h>
#import <OdrCoreObjC/ODRFile.h>
#import <OdrCoreObjC/ODRHtml.h>
#import <OdrCoreObjC/ODRLogger.h>

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>

/// Cross-translation-unit access to the C++ value each wrapper owns.
///
/// The handle model is much lighter than the JNI one (`jni/AGENTS.md`): an
/// ObjC++ `@implementation` can hold the C++ handle as an ivar directly, and
/// ARC's `.cxx_construct`/`.cxx_destruct` run its constructor and destructor.
/// No `long` handles, no `destroy` natives, no reaper thread.
///
/// Nor is there a keep-alive chain to maintain for these: the public C++
/// handles hold a `shared_ptr` to the implementation, so a wrapper owning one
/// by value already keeps it alive on its own.
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

@interface ODRHtmlView (Private)
+ (instancetype)viewWithHandle:(odr::HtmlView)handle;
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
