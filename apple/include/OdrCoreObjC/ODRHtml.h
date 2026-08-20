#import <Foundation/Foundation.h>

#import <OdrCoreObjC/ODRFilesystem.h>
#import <OdrCoreObjC/ODRTable.h>

NS_ASSUME_NONNULL_BEGIN

@class ODRDecodedFile;
@class ODRDocument;
@class ODRFile;
@class ODRLogger;

typedef NS_ENUM(NSInteger, ODRHtmlResourceType) {
  ODRHtmlResourceTypeHtmlFragment = 0,
  ODRHtmlResourceTypeCss,
  ODRHtmlResourceTypeJs,
  ODRHtmlResourceTypeImage,
  ODRHtmlResourceTypeFont,
  ODRHtmlResourceTypeMedia,
  ODRHtmlResourceTypeFile,
} NS_SWIFT_NAME(HtmlResourceType);

typedef NS_ENUM(NSInteger, ODRHtmlTableGridlines) {
  ODRHtmlTableGridlinesNone = 0,
  ODRHtmlTableGridlinesSoft,
  ODRHtmlTableGridlinesHard,
} NS_SWIFT_NAME(HtmlTableGridlines);

/// The colors the emitted HTML renders against. `ODRFileTypeCapabilities`
/// says which views honor it.
typedef NS_ENUM(NSInteger, ODRHtmlColorScheme) {
  /// A white page, carrying the colors the document gives its content.
  ODRHtmlColorSchemeLight = 0,
  /// A dark page, which the document's own text and fill colors give way to.
  ODRHtmlColorSchemeDark,
  /// Light or dark, by the reader's `prefers-color-scheme`.
  ODRHtmlColorSchemeSystem,
} NS_SWIFT_NAME(HtmlColorScheme);

/// Initial zoom of the emitted HTML on mobile (the viewport meta tag). Desktop
/// browsers ignore it entirely.
typedef NS_ENUM(NSInteger, ODRHtmlViewportMode) {
  /// Fixed-size paged content fits the width; reflowing content is actual size.
  ODRHtmlViewportModeAutomatic = 0,
  ODRHtmlViewportModeFitWidth,
  ODRHtmlViewportModeActualSize,
  /// No viewport meta tag at all.
  ODRHtmlViewportModeNone,
} NS_SWIFT_NAME(HtmlViewportMode);

/// How text is emitted in PDF→HTML output.
typedef NS_ENUM(NSInteger, ODRPdfTextMode) {
  /// A visual layer plus a transparent selection layer, like pdf.js.
  ODRPdfTextModeDualLayer = 0,
  /// One combined layer with glyphs mapped to Unicode, like pdf2htmlEX.
  ODRPdfTextModeSingleLayer,
} NS_SWIFT_NAME(PdfTextMode);

/// How to render — `odr::HtmlConfig`. Every property starts at the C++ default.
NS_SWIFT_NAME(HtmlConfig)
@interface ODRHtmlConfig : NSObject

/// The defaults, including the odr core data path resolved at construction.
- (instancetype)init;
/// The defaults, with `outputPath` set.
- (instancetype)initWithOutputPath:(NSString *)outputPath;

@property(nonatomic, copy) NSString *documentOutputFileName;
@property(nonatomic, copy) NSString *slideOutputFileName;
@property(nonatomic, copy) NSString *sheetOutputFileName;
@property(nonatomic, copy) NSString *pageOutputFileName;

@property(nonatomic) BOOL embedImages;
@property(nonatomic) BOOL embedShippedResources;

/// Where the shipped css/js live. `nil` keeps odrcore's own data path, which is
/// what the framework's bootstrap already points at — do not set it to the
/// empty string.
@property(nonatomic, copy, nullable) NSString *resourcePath;
@property(nonatomic) BOOL relativeResourcePaths;

@property(nonatomic) BOOL editable;
@property(nonatomic) BOOL textDocumentMargin;

@property(nonatomic) ODRHtmlColorScheme colorScheme;

/// `nil` for no limit.
@property(nonatomic, strong, nullable) NSValue *spreadsheetLimit;
@property(nonatomic) BOOL spreadsheetLimitByContent;
@property(nonatomic) ODRHtmlTableGridlines spreadsheetGridlines;

@property(nonatomic) ODRHtmlViewportMode viewportMode;
/// Overrides `viewportMode` for spreadsheets when set.
@property(nonatomic, strong, nullable) NSNumber *spreadsheetViewportMode;
/// Raw `content` for the viewport meta tag; overrides the modes above.
@property(nonatomic, copy, nullable) NSString *viewportContent;
/// The width the output will be shown at, in css pixels. When set, paged
/// content wider than it is is scaled to fit at render time, needing neither
/// the viewport meta tag nor a script; `nil` makes the output measure itself
/// at load and on every resize instead.
@property(nonatomic, strong, nullable) NSNumber *viewportWidth;

@property(nonatomic) BOOL formatHtml;
/// Repeated `htmlIndentString` per nesting level; 0 disables indentation.
@property(nonatomic) uint8_t htmlIndent;
@property(nonatomic, copy) NSString *htmlIndentString;

/// @deprecated Inert.
@property(nonatomic, copy) NSString *backgroundImageFormat;
/// @deprecated Inert.
@property(nonatomic) double backgroundImageDpi;

/// Render only pages `[begin, end)`, 0-based. `nil` end means to the last page.
@property(nonatomic) uint32_t pageRangeBegin;
@property(nonatomic, strong, nullable) NSNumber *pageRangeEnd;

@property(nonatomic) ODRPdfTextMode pdfTextMode;
@property(nonatomic, copy) NSArray<NSString *> *pdfDualLayerFallbackFonts;
@property(nonatomic) double pdfDualLayerFallbackFontSizeAdjust;

/// @deprecated Inert.
@property(nonatomic) BOOL noDrm;
/// @deprecated Inert.
@property(nonatomic) BOOL embedOutline;

@property(nonatomic, copy, nullable) NSString *outputPath;

@end

/// A file the rendered HTML refers to — `odr::HtmlResource`.
NS_SWIFT_NAME(HtmlResource)
@interface ODRHtmlResource : NSObject
@property(nonatomic, readonly) ODRHtmlResourceType type;
@property(nonatomic, readonly, copy) NSString *mimeType;
@property(nonatomic, readonly, copy) NSString *name;
@property(nonatomic, readonly, copy) NSString *path;
/// The backing file, when the resource has one.
@property(nonatomic, readonly, nullable) ODRFile *file;
/// Shipped with odrcore rather than extracted from the document.
@property(nonatomic, readonly) BOOL isShipped;
@property(nonatomic, readonly) BOOL isExternal;
@property(nonatomic, readonly) BOOL isAccessible;
/// Where the renderer decided to put it; `nil` if it was not relocated.
@property(nonatomic, readonly, nullable, copy) NSString *location;

/// The resource bytes.
- (nullable NSData *)dataWithError:(NSError **)error NS_SWIFT_NAME(data());

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// One emitted HTML file — `odr::HtmlPage`.
NS_SWIFT_NAME(HtmlPage)
@interface ODRHtmlPage : NSObject
@property(nonatomic, readonly, copy) NSString *name;
@property(nonatomic, readonly, copy) NSString *path;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Materialised HTML output — `odr::Html`.
NS_SWIFT_NAME(Html)
@interface ODRHtml : NSObject
@property(nonatomic, readonly) NSArray<ODRHtmlPage *> *pages;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// One renderable view of a document — a slide, a sheet, a page, or the whole
/// thing. `odr::HtmlView`.
NS_SWIFT_NAME(HtmlView)
@interface ODRHtmlView : NSObject
@property(nonatomic, readonly, copy) NSString *name;
@property(nonatomic, readonly) NSUInteger index;
/// The path this view is served at.
@property(nonatomic, readonly, copy) NSString *path;

/// Renders the view. The resources it refers to come back alongside it.
- (nullable NSString *)writeHtmlWithResources:
                           (NSArray<ODRHtmlResource *> *_Nullable *_Nullable)
                               resources
                                        error:(NSError **)error
    NS_SWIFT_NAME(writeHtml(resources:));

/// Writes this view and everything it needs below `outputPath`.
- (nullable ODRHtml *)bringOfflineTo:(NSString *)outputPath
                               error:(NSError **)error
    NS_SWIFT_NAME(bringOffline(to:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// The result of translating a file — `odr::HtmlService`.
///
/// Renders on demand: nothing is produced until a view is written or the
/// service is asked for a path, which is what makes serving it over HTTP cheap.
NS_SWIFT_NAME(HtmlService)
@interface ODRHtmlService : NSObject

@property(nonatomic, readonly) NSArray<ODRHtmlView *> *views;

/// Does the up-front work now instead of on the first request.
- (BOOL)warmupWithError:(NSError **)error NS_SWIFT_NAME(warmup());

- (BOOL)existsAtPath:(NSString *)path NS_SWIFT_NAME(exists(path:));
- (nullable NSString *)mimetypeAtPath:(NSString *)path
                                error:(NSError **)error
    NS_SWIFT_NAME(mimetype(path:));

/// The bytes served at `path` — HTML, an image, a font, whatever it is.
- (nullable NSData *)dataAtPath:(NSString *)path
                          error:(NSError **)error NS_SWIFT_NAME(data(path:));

/// Writes every view and its resources below `outputPath`.
- (nullable ODRHtml *)bringOfflineTo:(NSString *)outputPath
                               error:(NSError **)error
    NS_SWIFT_NAME(bringOffline(to:));
/// The same, for a subset of the views.
- (nullable ODRHtml *)bringOfflineTo:(NSString *)outputPath
                               views:(NSArray<ODRHtmlView *> *)views
                               error:(NSError **)error
    NS_SWIFT_NAME(bringOffline(to:views:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Translating to HTML — `odr::html`.
NS_SWIFT_NAME(HtmlTranslator)
@interface ODRHtmlTranslator : NSObject

/// Translates a decoded file. `cachePath` is a directory for temporary output.
+ (nullable ODRHtmlService *)translateFile:(ODRDecodedFile *)file
                                 cachePath:(NSString *)cachePath
                                    config:(ODRHtmlConfig *)config
                                     error:(NSError **)error
    NS_SWIFT_NAME(translate(file:cachePath:config:));
+ (nullable ODRHtmlService *)translateFile:(ODRDecodedFile *)file
                                 cachePath:(NSString *)cachePath
                                    config:(ODRHtmlConfig *)config
                                    logger:(ODRLogger *)logger
                                     error:(NSError **)error
    NS_SWIFT_NAME(translate(file:cachePath:config:logger:));

/// Translates an already-decoded document.
+ (nullable ODRHtmlService *)translateDocument:(ODRDocument *)document
                                     cachePath:(NSString *)cachePath
                                        config:(ODRHtmlConfig *)config
                                         error:(NSError **)error
    NS_SWIFT_NAME(translate(document:cachePath:config:));

/// Translates a filesystem — a document's parts, or an archive's contents.
+ (nullable ODRHtmlService *)translateFilesystem:(ODRFilesystem *)filesystem
                                       cachePath:(NSString *)cachePath
                                          config:(ODRHtmlConfig *)config
                                           error:(NSError **)error
    NS_SWIFT_NAME(translate(filesystem:cachePath:config:));

/// Translates an archive.
+ (nullable ODRHtmlService *)translateArchive:(ODRArchive *)archive
                                    cachePath:(NSString *)cachePath
                                       config:(ODRHtmlConfig *)config
                                        error:(NSError **)error
    NS_SWIFT_NAME(translate(archive:cachePath:config:));

/// Applies a diff produced by the browser-side JavaScript back to `document`.
+ (BOOL)editDocument:(ODRDocument *)document
                diff:(NSString *)diff
               error:(NSError **)error NS_SWIFT_NAME(edit(document:diff:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
