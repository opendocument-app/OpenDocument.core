#import <OdrCoreObjC/ODRHtml.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/html.hpp>

#include <optional>
#include <sstream>
#include <vector>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::to_nsstring;
using odr::apple::to_string;

ODR_SAME_ENUM(ODRHtmlResourceTypeHtmlFragment,
              odr::HtmlResourceType::html_fragment);
ODR_SAME_ENUM(ODRHtmlResourceTypeCss, odr::HtmlResourceType::css);
ODR_SAME_ENUM(ODRHtmlResourceTypeJs, odr::HtmlResourceType::js);
ODR_SAME_ENUM(ODRHtmlResourceTypeImage, odr::HtmlResourceType::image);
ODR_SAME_ENUM(ODRHtmlResourceTypeFont, odr::HtmlResourceType::font);
ODR_SAME_ENUM(ODRHtmlResourceTypeMedia, odr::HtmlResourceType::media);

ODR_SAME_ENUM(ODRHtmlTableGridlinesNone, odr::HtmlTableGridlines::none);
ODR_SAME_ENUM(ODRHtmlTableGridlinesSoft, odr::HtmlTableGridlines::soft);
ODR_SAME_ENUM(ODRHtmlTableGridlinesHard, odr::HtmlTableGridlines::hard);

ODR_SAME_ENUM(ODRHtmlViewportModeAutomatic, odr::HtmlViewportMode::automatic);
ODR_SAME_ENUM(ODRHtmlViewportModeFitWidth, odr::HtmlViewportMode::fit_width);
ODR_SAME_ENUM(ODRHtmlViewportModeActualSize,
              odr::HtmlViewportMode::actual_size);
ODR_SAME_ENUM(ODRHtmlViewportModeNone, odr::HtmlViewportMode::none);

ODR_SAME_ENUM(ODRPdfTextModeDualLayer, odr::PdfTextMode::dual_layer);
ODR_SAME_ENUM(ODRPdfTextModeSingleLayer, odr::PdfTextMode::single_layer);

namespace {

NSArray<NSString *> *to_nsarray(const std::vector<std::string> &strings) {
  NSMutableArray<NSString *> *const result =
      [NSMutableArray arrayWithCapacity:strings.size()];
  for (const std::string &string : strings) {
    [result addObject:to_nsstring(string)];
  }
  return result;
}

std::vector<std::string> to_strings(NSArray<NSString *> *strings) {
  std::vector<std::string> result;
  result.reserve(strings.count);
  for (NSString *const string in strings) {
    result.push_back(to_string(string));
  }
  return result;
}

} // namespace

#pragma mark - ODRHtmlConfig

@implementation ODRHtmlConfig

- (instancetype)init {
  return [self initWithNativeConfig:odr::HtmlConfig()];
}

- (instancetype)initWithOutputPath:(NSString *)outputPath {
  return [self initWithNativeConfig:odr::HtmlConfig(to_string(outputPath))];
}

- (instancetype)initWithNativeConfig:(const odr::HtmlConfig &)config {
  if ((self = [super init]) == nil) {
    return nil;
  }
  _documentOutputFileName = to_nsstring(config.document_output_file_name);
  _slideOutputFileName = to_nsstring(config.slide_output_file_name);
  _sheetOutputFileName = to_nsstring(config.sheet_output_file_name);
  _pageOutputFileName = to_nsstring(config.page_output_file_name);
  _embedImages = config.embed_images ? YES : NO;
  _embedShippedResources = config.embed_shipped_resources ? YES : NO;
  _resourcePath = to_nsstring(config.resource_path);
  _relativeResourcePaths = config.relative_resource_paths ? YES : NO;
  _editable = config.editable ? YES : NO;
  _textDocumentMargin = config.text_document_margin ? YES : NO;
  if (config.spreadsheet_limit.has_value()) {
    const ODRTableDimensions limit = ODRTableDimensionsMake(
        config.spreadsheet_limit->rows, config.spreadsheet_limit->columns);
    _spreadsheetLimit = [NSValue valueWithBytes:&limit
                                       objCType:@encode(ODRTableDimensions)];
  }
  _spreadsheetLimitByContent = config.spreadsheet_limit_by_content ? YES : NO;
  _spreadsheetGridlines =
      static_cast<ODRHtmlTableGridlines>(config.spreadsheet_gridlines);
  _viewportMode = static_cast<ODRHtmlViewportMode>(config.viewport_mode);
  _spreadsheetViewportMode =
      config.spreadsheet_viewport_mode.has_value()
          ? @(static_cast<NSInteger>(*config.spreadsheet_viewport_mode))
          : nil;
  _viewportContent = config.viewport_content.has_value()
                         ? to_nsstring(*config.viewport_content)
                         : nil;
  _formatHtml = config.format_html ? YES : NO;
  _htmlIndent = config.html_indent;
  _htmlIndentString = to_nsstring(config.html_indent_string);
  _backgroundImageFormat = to_nsstring(config.background_image_format);
  _backgroundImageDpi = config.background_image_dpi;
  _pageRangeBegin = config.page_range_begin;
  _pageRangeEnd = config.page_range_end.has_value()
                      ? @(static_cast<unsigned int>(*config.page_range_end))
                      : nil;
  _pdfTextMode = static_cast<ODRPdfTextMode>(config.pdf_text_mode);
  _pdfDualLayerFallbackFonts = to_nsarray(config.pdf_dual_layer_fallback_fonts);
  _pdfDualLayerFallbackFontSizeAdjust =
      config.pdf_dual_layer_fallback_font_size_adjust;
  _noDrm = config.no_drm ? YES : NO;
  _embedOutline = config.embed_outline ? YES : NO;
  _outputPath =
      config.output_path.has_value() ? to_nsstring(*config.output_path) : nil;
  return self;
}

- (odr::HtmlConfig)nativeConfig {
  // Starts from a default-constructed config so anything not bound here — the
  // resource locator, which is a function pointer we deliberately do not carry
  // across, as in the JNI bindings — keeps odrcore's own value.
  odr::HtmlConfig config;
  config.document_output_file_name = to_string(_documentOutputFileName);
  config.slide_output_file_name = to_string(_slideOutputFileName);
  config.sheet_output_file_name = to_string(_sheetOutputFileName);
  config.page_output_file_name = to_string(_pageOutputFileName);
  config.embed_images = _embedImages == YES;
  config.embed_shipped_resources = _embedShippedResources == YES;
  // A nil resource path means "keep odrcore's default", which is what the
  // framework's bootstrap already set — writing an empty string here would
  // silently unset it.
  if (_resourcePath != nil) {
    config.resource_path = to_string(_resourcePath);
  }
  config.relative_resource_paths = _relativeResourcePaths == YES;
  config.editable = _editable == YES;
  config.text_document_margin = _textDocumentMargin == YES;
  if (_spreadsheetLimit != nil) {
    ODRTableDimensions limit{};
    [_spreadsheetLimit getValue:&limit size:sizeof(limit)];
    config.spreadsheet_limit = odr::TableDimensions(limit.rows, limit.columns);
  } else {
    config.spreadsheet_limit.reset();
  }
  config.spreadsheet_limit_by_content = _spreadsheetLimitByContent == YES;
  config.spreadsheet_gridlines =
      static_cast<odr::HtmlTableGridlines>(_spreadsheetGridlines);
  config.viewport_mode = static_cast<odr::HtmlViewportMode>(_viewportMode);
  if (_spreadsheetViewportMode != nil) {
    config.spreadsheet_viewport_mode = static_cast<odr::HtmlViewportMode>(
        _spreadsheetViewportMode.integerValue);
  } else {
    config.spreadsheet_viewport_mode.reset();
  }
  if (_viewportContent != nil) {
    config.viewport_content = to_string(_viewportContent);
  } else {
    config.viewport_content.reset();
  }
  config.format_html = _formatHtml == YES;
  config.html_indent = _htmlIndent;
  config.html_indent_string = to_string(_htmlIndentString);
  config.background_image_format = to_string(_backgroundImageFormat);
  config.background_image_dpi = _backgroundImageDpi;
  config.page_range_begin = _pageRangeBegin;
  if (_pageRangeEnd != nil) {
    config.page_range_end =
        static_cast<std::uint32_t>(_pageRangeEnd.unsignedIntValue);
  } else {
    config.page_range_end.reset();
  }
  config.pdf_text_mode = static_cast<odr::PdfTextMode>(_pdfTextMode);
  config.pdf_dual_layer_fallback_fonts = to_strings(_pdfDualLayerFallbackFonts);
  config.pdf_dual_layer_fallback_font_size_adjust =
      _pdfDualLayerFallbackFontSizeAdjust;
  config.no_drm = _noDrm == YES;
  config.embed_outline = _embedOutline == YES;
  if (_outputPath != nil) {
    config.output_path = to_string(_outputPath);
  } else {
    config.output_path.reset();
  }
  return config;
}

@end

#pragma mark - ODRHtmlResource

@implementation ODRHtmlResource {
  std::optional<odr::HtmlResource> _handle;
  // `HtmlResourceLocation` is itself a `std::optional`, so it needs no wrapper
  odr::HtmlResourceLocation _location;
}

+ (instancetype)resourceWithHandle:(odr::HtmlResource)handle
                          location:(odr::HtmlResourceLocation)location {
  ODRHtmlResource *const result = [[ODRHtmlResource alloc] init];
  result->_handle = std::move(handle);
  result->_location = std::move(location);
  return result;
}

- (ODRHtmlResourceType)type {
  return guarded_value(
      [&] { return static_cast<ODRHtmlResourceType>(_handle->type()); },
      ODRHtmlResourceTypeHtmlFragment);
}

- (NSString *)mimeType {
  return guarded_value([&] { return to_nsstring(_handle->mime_type()); }, @"");
}

- (NSString *)name {
  return guarded_value([&] { return to_nsstring(_handle->name()); }, @"");
}

- (NSString *)path {
  return guarded_value([&] { return to_nsstring(_handle->path()); }, @"");
}

- (nullable ODRFile *)file {
  return guarded_value(
      [&]() -> ODRFile * {
        const std::optional<odr::File> &file = _handle->file();
        return file.has_value() ? [ODRFile fileWithHandle:*file] : nil;
      },
      nil);
}

- (BOOL)isShipped {
  return guarded_value([&] { return _handle->is_shipped() ? YES : NO; }, NO);
}

- (BOOL)isExternal {
  return guarded_value([&] { return _handle->is_external() ? YES : NO; }, NO);
}

- (BOOL)isAccessible {
  return guarded_value([&] { return _handle->is_accessible() ? YES : NO; }, NO);
}

- (nullable NSString *)location {
  return _location.has_value() ? to_nsstring(*_location) : nil;
}

- (nullable NSData *)dataWithError:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    std::ostringstream out;
    _handle->write_resource(out);
    const std::string bytes = out.str();
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
  });
}

@end

#pragma mark - ODRHtmlPage / ODRHtml

@implementation ODRHtmlPage

+ (instancetype)pageWithHandle:(const odr::HtmlPage &)handle {
  ODRHtmlPage *const result = [[ODRHtmlPage alloc] init];
  result->_name = to_nsstring(handle.name);
  result->_path = to_nsstring(handle.path);
  return result;
}

@end

@implementation ODRHtml

+ (instancetype)htmlWithHandle:(const odr::Html &)handle {
  ODRHtml *const result = [[ODRHtml alloc] init];
  NSMutableArray<ODRHtmlPage *> *const pages =
      [NSMutableArray arrayWithCapacity:handle.pages().size()];
  for (const odr::HtmlPage &page : handle.pages()) {
    [pages addObject:[ODRHtmlPage pageWithHandle:page]];
  }
  result->_pages = pages;
  return result;
}

@end

#pragma mark - ODRHtmlView

namespace {

NSArray<ODRHtmlResource *> *to_nsarray(const odr::HtmlResources &resources) {
  NSMutableArray<ODRHtmlResource *> *const result =
      [NSMutableArray arrayWithCapacity:resources.size()];
  for (const auto &[resource, location] : resources) {
    [result addObject:[ODRHtmlResource resourceWithHandle:resource
                                                 location:location]];
  }
  return result;
}

} // namespace

@implementation ODRHtmlView {
  std::optional<odr::HtmlView> _handle;
}

+ (instancetype)viewWithHandle:(odr::HtmlView)handle {
  ODRHtmlView *const result = [[ODRHtmlView alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::HtmlView &)handle {
  return *_handle;
}

- (NSString *)name {
  return guarded_value([&] { return to_nsstring(_handle->name()); }, @"");
}

- (NSUInteger)index {
  return guarded_value(
      [&] { return static_cast<NSUInteger>(_handle->index()); }, NSUInteger{0});
}

- (NSString *)path {
  return guarded_value([&] { return to_nsstring(_handle->path()); }, @"");
}

- (nullable NSString *)writeHtmlWithResources:
                           (NSArray<ODRHtmlResource *> **)resources
                                        error:(NSError **)error {
  return guarded(error, [&]() -> NSString * {
    std::ostringstream out;
    const odr::HtmlResources used = _handle->write_html(out);
    if (resources != nullptr) {
      *resources = to_nsarray(used);
    }
    return to_nsstring(out.str());
  });
}

- (nullable ODRHtml *)bringOfflineTo:(NSString *)outputPath
                               error:(NSError **)error {
  return guarded(error, [&]() -> ODRHtml * {
    return
        [ODRHtml htmlWithHandle:_handle->bring_offline(to_string(outputPath))];
  });
}

@end

#pragma mark - ODRHtmlService

@implementation ODRHtmlService {
  std::optional<odr::HtmlService> _handle;
}

+ (instancetype)serviceWithHandle:(odr::HtmlService)handle {
  ODRHtmlService *const result = [[ODRHtmlService alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::HtmlService &)handle {
  return *_handle;
}

- (NSArray<ODRHtmlView *> *)views {
  return guarded_value(
      [&]() -> NSArray<ODRHtmlView *> * {
        const odr::HtmlViews &views = _handle->list_views();
        NSMutableArray<ODRHtmlView *> *const result =
            [NSMutableArray arrayWithCapacity:views.size()];
        for (const odr::HtmlView &view : views) {
          [result addObject:[ODRHtmlView viewWithHandle:view]];
        }
        return result;
      },
      @[]);
}

- (BOOL)warmupWithError:(NSError **)error {
  return guarded(error, [&] {
    _handle->warmup();
    return YES;
  });
}

- (BOOL)existsAtPath:(NSString *)path {
  return guarded_value(
      [&] { return _handle->exists(to_string(path)) ? YES : NO; }, NO);
}

- (nullable NSString *)mimetypeAtPath:(NSString *)path error:(NSError **)error {
  return guarded(error, [&]() -> NSString * {
    return to_nsstring(_handle->mimetype(to_string(path)));
  });
}

- (nullable NSData *)dataAtPath:(NSString *)path error:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    std::ostringstream out;
    _handle->write(to_string(path), out);
    const std::string bytes = out.str();
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
  });
}

- (nullable ODRHtml *)bringOfflineTo:(NSString *)outputPath
                               error:(NSError **)error {
  return guarded(error, [&]() -> ODRHtml * {
    return
        [ODRHtml htmlWithHandle:_handle->bring_offline(to_string(outputPath))];
  });
}

- (nullable ODRHtml *)bringOfflineTo:(NSString *)outputPath
                               views:(NSArray<ODRHtmlView *> *)views
                               error:(NSError **)error {
  return guarded(error, [&]() -> ODRHtml * {
    std::vector<odr::HtmlView> native;
    native.reserve(views.count);
    for (ODRHtmlView *const view in views) {
      native.push_back(view.handle);
    }
    return [ODRHtml
        htmlWithHandle:_handle->bring_offline(to_string(outputPath), native)];
  });
}

@end

#pragma mark - ODRHtmlTranslator

@implementation ODRHtmlTranslator

+ (nullable ODRHtmlService *)translateFile:(ODRDecodedFile *)file
                                 cachePath:(NSString *)cachePath
                                    config:(ODRHtmlConfig *)config
                                     error:(NSError **)error {
  return [ODRHtmlTranslator translateFile:file
                                cachePath:cachePath
                                   config:config
                                   logger:ODRLogger.null
                                    error:error];
}

+ (nullable ODRHtmlService *)translateFile:(ODRDecodedFile *)file
                                 cachePath:(NSString *)cachePath
                                    config:(ODRHtmlConfig *)config
                                    logger:(ODRLogger *)logger
                                     error:(NSError **)error {
  return guarded(error, [&]() -> ODRHtmlService * {
    return [ODRHtmlService
        serviceWithHandle:odr::html::translate(
                              file.handle, to_string(cachePath),
                              config.nativeConfig, logger.handle)];
  });
}

+ (nullable ODRHtmlService *)translateDocument:(ODRDocument *)document
                                     cachePath:(NSString *)cachePath
                                        config:(ODRHtmlConfig *)config
                                         error:(NSError **)error {
  return guarded(error, [&]() -> ODRHtmlService * {
    return [ODRHtmlService
        serviceWithHandle:odr::html::translate(document.handle,
                                               to_string(cachePath),
                                               config.nativeConfig)];
  });
}

+ (nullable ODRHtmlService *)translateFilesystem:(ODRFilesystem *)filesystem
                                       cachePath:(NSString *)cachePath
                                          config:(ODRHtmlConfig *)config
                                           error:(NSError **)error {
  return guarded(error, [&]() -> ODRHtmlService * {
    return [ODRHtmlService
        serviceWithHandle:odr::html::translate(filesystem.handle,
                                               to_string(cachePath),
                                               config.nativeConfig)];
  });
}

+ (nullable ODRHtmlService *)translateArchive:(ODRArchive *)archive
                                    cachePath:(NSString *)cachePath
                                       config:(ODRHtmlConfig *)config
                                        error:(NSError **)error {
  return guarded(error, [&]() -> ODRHtmlService * {
    return [ODRHtmlService
        serviceWithHandle:odr::html::translate(archive.handle,
                                               to_string(cachePath),
                                               config.nativeConfig)];
  });
}

+ (BOOL)editDocument:(ODRDocument *)document
                diff:(NSString *)diff
               error:(NSError **)error {
  return guarded(error, [&] {
    odr::html::edit(document.handle, to_string(diff));
    return YES;
  });
}

@end
