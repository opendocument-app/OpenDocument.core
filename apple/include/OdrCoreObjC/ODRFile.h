#import <Foundation/Foundation.h>

#import <OdrCoreObjC/ODRFilesystem.h>

NS_ASSUME_NONNULL_BEGIN

@class ODRDocument;
@class ODRLogger;

@class ODRTextFile;
@class ODRImageFile;
@class ODRArchiveFile;
@class ODRDocumentFile;
@class ODRPdfFile;
@class ODRFontFile;

/// Every file type the library knows about. Declaration order matches
/// `odr::FileType` — the two are the same values, so they must not drift.
typedef NS_ENUM(NSInteger, ODRFileType) {
  ODRFileTypeUnknown = 0,

  ODRFileTypeOpenDocumentText,
  ODRFileTypeOpenDocumentPresentation,
  ODRFileTypeOpenDocumentSpreadsheet,
  ODRFileTypeOpenDocumentGraphics,

  ODRFileTypeOfficeOpenXmlDocument,
  ODRFileTypeOfficeOpenXmlPresentation,
  ODRFileTypeOfficeOpenXmlWorkbook,
  ODRFileTypeOfficeOpenXmlEncrypted,
  ODRFileTypeExcelBinaryWorkbook,

  ODRFileTypeLegacyWordDocument,
  ODRFileTypeLegacyPowerpointPresentation,
  ODRFileTypeLegacyExcelWorksheets,

  ODRFileTypeWordPerfect,
  ODRFileTypeRichTextFormat,

  ODRFileTypePortableDocumentFormat,

  ODRFileTypeTextFile,
  ODRFileTypeCommaSeparatedValues,
  ODRFileTypeJavascriptObjectNotation,
  ODRFileTypeMarkdown,

  ODRFileTypeZip,
  ODRFileTypeCompoundFileBinaryFormat,

  ODRFileTypePortableNetworkGraphics,
  ODRFileTypeGraphicsInterchangeFormat,
  ODRFileTypeJpeg,
  ODRFileTypeBitmapImageFile,

  ODRFileTypeStarviewMetafile,

  ODRFileTypeTruetypeFont,
  ODRFileTypeOpentypeFont,

  ODRFileTypeWebp,
  ODRFileTypeTaggedImageFileFormat,
  ODRFileTypeHighEfficiencyImageFormat,
  ODRFileTypeAv1ImageFileFormat,

  ODRFileTypeMpegAudio,
  ODRFileTypeMpeg4Audio,
  ODRFileTypeOggAudio,
  ODRFileTypeWaveformAudio,
  ODRFileTypeFreeLosslessAudioCodec,

  ODRFileTypeMpeg4Video,
  ODRFileTypeQuicktimeVideo,
  ODRFileTypeThirdGenerationPartnershipVideo,
  ODRFileTypeMatroskaVideo,
  ODRFileTypeAudioVideoInterleave,

  ODRFileTypeScalableVectorGraphics,
  ODRFileTypeWindowsIcon,
  ODRFileTypeJpegXl,
  ODRFileTypeJpeg2000,
  ODRFileTypePhotoshopDocument,
  ODRFileTypeWindowsMetafile,
  ODRFileTypeEnhancedMetafile,

  ODRFileTypeXml,

  ODRFileTypeIworkPages,
  ODRFileTypeIworkNumbers,
  ODRFileTypeIworkKeynote,

  ODRFileTypeHypertextMarkupLanguage,
} NS_SWIFT_NAME(FileType);

typedef NS_ENUM(NSInteger, ODRFileCategory) {
  ODRFileCategoryUnknown = 0,
  ODRFileCategoryText,
  ODRFileCategoryImage,
  ODRFileCategoryArchive,
  ODRFileCategoryDocument,
  ODRFileCategoryFont,
  ODRFileCategoryAudio,
  ODRFileCategoryVideo,
} NS_SWIFT_NAME(FileCategory);

typedef NS_ENUM(NSInteger, ODRFileLocation) {
  ODRFileLocationUnknown = 0,
  ODRFileLocationMemory,
  ODRFileLocationDisk,
} NS_SWIFT_NAME(FileLocation);

typedef NS_ENUM(NSInteger, ODREncryptionState) {
  ODREncryptionStateUnknown = 0,
  ODREncryptionStateNotEncrypted,
  ODREncryptionStateEncrypted,
  ODREncryptionStateDecrypted,
} NS_SWIFT_NAME(EncryptionState);

/// A text encoding. Only some can be decoded — see
/// `Odr.isDecodable(textEncoding:)`; the rest can only be named.
typedef NS_ENUM(NSInteger, ODRTextEncoding) {
  ODRTextEncodingUnknown = 0,
  ODRTextEncodingUtf8,
  ODRTextEncodingUtf16le,
  ODRTextEncodingUtf16be,
  ODRTextEncodingUtf32le,
  ODRTextEncodingUtf32be,
  ODRTextEncodingIbm866,
  ODRTextEncodingIso88591,
  ODRTextEncodingIso88592,
  ODRTextEncodingIso88593,
  ODRTextEncodingIso88594,
  ODRTextEncodingIso88595,
  ODRTextEncodingIso88596,
  ODRTextEncodingIso88597,
  ODRTextEncodingIso88598,
  ODRTextEncodingIso885910,
  ODRTextEncodingIso885913,
  ODRTextEncodingIso885914,
  ODRTextEncodingIso885915,
  ODRTextEncodingIso885916,
  ODRTextEncodingKoi8R,
  ODRTextEncodingKoi8U,
  ODRTextEncodingMacintosh,
  ODRTextEncodingWindows874,
  ODRTextEncodingWindows1250,
  ODRTextEncodingWindows1251,
  ODRTextEncodingWindows1252,
  ODRTextEncodingWindows1253,
  ODRTextEncodingWindows1254,
  ODRTextEncodingWindows1255,
  ODRTextEncodingWindows1256,
  ODRTextEncodingWindows1257,
  ODRTextEncodingWindows1258,
  ODRTextEncodingXMacCyrillic,
  ODRTextEncodingBig5,
  ODRTextEncodingEucJp,
  ODRTextEncodingEucKr,
  ODRTextEncodingGb18030,
  ODRTextEncodingIso2022Jp,
  ODRTextEncodingIso2022Kr,
  ODRTextEncodingShiftJis,
} NS_SWIFT_NAME(TextEncoding);

typedef NS_ENUM(NSInteger, ODRDocumentType) {
  ODRDocumentTypeUnknown = 0,
  ODRDocumentTypeText,
  ODRDocumentTypePresentation,
  ODRDocumentTypeSpreadsheet,
  ODRDocumentTypeDrawing,
} NS_SWIFT_NAME(DocumentType);

/// What the library can do with a format — declared support, an upper bound. A
/// concrete file may still fail; ask `ODRDecodedFile` or `ODRDocument`.
NS_SWIFT_NAME(FileTypeCapabilities)
@interface ODRFileTypeCapabilities : NSObject
/// Recognised from its bytes alone.
@property(nonatomic, readonly) BOOL detectByContent;
/// A decoder exists.
@property(nonatomic, readonly) BOOL open;
/// Encrypted instances can be decrypted.
@property(nonatomic, readonly) BOOL decrypt;
/// `ODRHtml` produces output for it.
@property(nonatomic, readonly) BOOL translateHtml;
/// The view it renders as honors `ODRHtmlConfig.colorScheme`.
@property(nonatomic, readonly) BOOL colorScheme;
@property(nonatomic, readonly) BOOL edit;
@property(nonatomic, readonly) BOOL save;
/// Saving with a password is supported.
@property(nonatomic, readonly) BOOL encrypt;
/// `ODRPdfFile.annotate` is supported.
@property(nonatomic, readonly) BOOL annotate;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// Meta information about a file. The document fields mean nothing unless
/// `documentType` is set.
NS_SWIFT_NAME(FileMeta)
@interface ODRFileMeta : NSObject
@property(nonatomic, readonly) ODRFileType type;
@property(nonatomic, readonly, copy) NSString *mimetype;
@property(nonatomic, readonly) BOOL passwordEncrypted;
@property(nonatomic, readonly) ODRDocumentType documentType;
/// `nil` when the backend does not carry one.
@property(nonatomic, readonly, nullable) NSNumber *entryCount;

@property(nonatomic, readonly, nullable, copy) NSString *title;
@property(nonatomic, readonly, nullable, copy) NSString *author;
@property(nonatomic, readonly, nullable, copy) NSString *subject;
@property(nonatomic, readonly, nullable, copy) NSString *keywords;
@property(nonatomic, readonly, nullable, copy) NSString *creator;
@property(nonatomic, readonly, nullable, copy) NSString *producer;
@property(nonatomic, readonly, nullable, copy) NSString *creationDate;
@property(nonatomic, readonly, nullable, copy) NSString *modificationDate;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// How to read a csv file. An unset field is detected from the file's opening
/// bytes; a set one is taken as given.
NS_SWIFT_NAME(CsvOptions)
@interface ODRCsvOptions : NSObject
/// `nil` to detect.
@property(nonatomic, strong, nullable) NSNumber *encoding;
/// A one-character string, `nil` to detect.
@property(nonatomic, copy, nullable) NSString *separator;
/// A one-character string, `nil` to detect.
@property(nonatomic, copy, nullable) NSString *quote;
@end

/// How to decode a file. Every field is optional; the default detects
/// everything.
NS_SWIFT_NAME(DecodeOptions)
@interface ODRDecodeOptions : NSObject
/// Decode as this type, whatever detection says. `nil` to let it decide.
@property(nonatomic, strong, nullable) NSNumber *asFileType;
/// Types to prefer, most preferred first.
@property(nonatomic, copy) NSArray<NSNumber *> *fileTypePriority;
/// Format-specific overrides for a file decoded as csv.
@property(nonatomic, strong) ODRCsvOptions *csv;
@end

/// A file, decoded or not — `odr::File`.
NS_SWIFT_NAME(File)
@interface ODRFile : NSObject

/// Opens the file at `path`. Fails if it cannot be read.
- (nullable instancetype)initWithPath:(NSString *)path error:(NSError **)error;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property(nonatomic, readonly) ODRFileLocation location;
@property(nonatomic, readonly) NSUInteger size;
/// The file name, without any directory; empty where there is none.
@property(nonatomic, readonly, copy) NSString *name;
/// The path, when the file is on disk.
@property(nonatomic, readonly, nullable, copy) NSString *diskPath;

/// The whole file. Reads it into memory — for a large file prefer `copyTo:`.
- (nullable NSData *)dataWithError:(NSError **)error NS_SWIFT_NAME(data());
/// Writes the file to `path`.
- (BOOL)copyTo:(NSString *)path error:(NSError **)error;

@end

/// A decoded file — `odr::DecodedFile`. Use `as…` to reach the typed view.
NS_SWIFT_NAME(DecodedFile)
@interface ODRDecodedFile : NSObject

/// Decodes the file at `path`, detecting its type.
+ (nullable instancetype)decodePath:(NSString *)path
                              error:(NSError **)error
    NS_SWIFT_NAME(decode(path:));
/// Decodes the file at `path` as `type`, whatever detection says.
+ (nullable instancetype)decodePath:(NSString *)path
                                 as:(ODRFileType)type
                              error:(NSError **)error
    NS_SWIFT_NAME(decode(path:as:));
/// Decodes the file at `path` per `options`.
+ (nullable instancetype)decodePath:(NSString *)path
                            options:(ODRDecodeOptions *)options
                              error:(NSError **)error
    NS_SWIFT_NAME(decode(path:options:));
/// Decodes an already-open file.
+ (nullable instancetype)decodeFile:(ODRFile *)file
                              error:(NSError **)error
    NS_SWIFT_NAME(decode(file:));

/// The overloads that report progress and problems into `logger`. Every entry
/// point above uses the null logger.
+ (nullable instancetype)decodePath:(NSString *)path
                             logger:(ODRLogger *)logger
                              error:(NSError **)error
    NS_SWIFT_NAME(decode(path:logger:));
+ (nullable instancetype)decodePath:(NSString *)path
                                 as:(ODRFileType)type
                             logger:(ODRLogger *)logger
                              error:(NSError **)error
    NS_SWIFT_NAME(decode(path:as:logger:));
+ (nullable instancetype)decodeFile:(ODRFile *)file
                             logger:(ODRLogger *)logger
                              error:(NSError **)error
    NS_SWIFT_NAME(decode(file:logger:));

/// Every type `path` could plausibly be decoded as.
+ (nullable NSArray<NSNumber *> *)listFileTypesAtPath:(NSString *)path
                                                error:(NSError **)error
    NS_SWIFT_NAME(listFileTypes(path:));
+ (nullable NSArray<NSNumber *> *)listFileTypesAtPath:(NSString *)path
                                               logger:(ODRLogger *)logger
                                                error:(NSError **)error
    NS_SWIFT_NAME(listFileTypes(path:logger:));
/// The MIME type of the file at `path`.
+ (nullable NSString *)mimetypeAtPath:(NSString *)path
                                error:(NSError **)error
    NS_SWIFT_NAME(mimetype(path:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property(nonatomic, readonly) ODRFile *file;
@property(nonatomic, readonly) ODRFileType fileType;
@property(nonatomic, readonly) ODRFileCategory fileCategory;
@property(nonatomic, readonly) ODRFileMeta *fileMeta;
@property(nonatomic, readonly) BOOL isDecodable;

@property(nonatomic, readonly) BOOL isPasswordEncrypted;
@property(nonatomic, readonly) ODREncryptionState encryptionState;
/// Decrypts with `password`. Fails with `ODRErrorWrongPassword` if it is wrong.
- (nullable ODRDecodedFile *)decryptWithPassword:(NSString *)password
                                           error:(NSError **)error;

/// What can be done with *this* file. Refines the format-level answer.
@property(nonatomic, readonly) ODRFileTypeCapabilities *capabilities;

@property(nonatomic, readonly) BOOL isTextFile;
@property(nonatomic, readonly) BOOL isImageFile;
@property(nonatomic, readonly) BOOL isArchiveFile;
@property(nonatomic, readonly) BOOL isDocumentFile;
@property(nonatomic, readonly) BOOL isPdfFile;
@property(nonatomic, readonly) BOOL isFontFile;

/// The typed views. Each fails unless the matching `is…` is true.
- (nullable ODRTextFile *)asTextFileWithError:(NSError **)error
    NS_SWIFT_NAME(asTextFile());
- (nullable ODRImageFile *)asImageFileWithError:(NSError **)error
    NS_SWIFT_NAME(asImageFile());
- (nullable ODRArchiveFile *)asArchiveFileWithError:(NSError **)error
    NS_SWIFT_NAME(asArchiveFile());
- (nullable ODRDocumentFile *)asDocumentFileWithError:(NSError **)error
    NS_SWIFT_NAME(asDocumentFile());
- (nullable ODRPdfFile *)asPdfFileWithError:(NSError **)error
    NS_SWIFT_NAME(asPdfFile());
- (nullable ODRFontFile *)asFontFileWithError:(NSError **)error
    NS_SWIFT_NAME(asFontFile());

@end

/// A decoded text file — `odr::TextFile`.
NS_SWIFT_NAME(TextFile)
@interface ODRTextFile : ODRDecodedFile
/// The encoding the bytes were detected as, or decoded with.
@property(nonatomic, readonly) ODRTextEncoding encoding;
/// The detected charset, `nil` if it could not be determined.
///
/// Deprecated: read `encoding` and ask `Odr.string(textEncoding:)` for a name.
@property(nonatomic, readonly, nullable, copy) NSString *charset;
/// The decoded text.
- (nullable NSString *)textWithError:(NSError **)error NS_SWIFT_NAME(text());
@end

/// A decoded image file — `odr::ImageFile`.
NS_SWIFT_NAME(ImageFile)
@interface ODRImageFile : ODRDecodedFile
/// The image bytes, as stored.
- (nullable NSData *)dataWithError:(NSError **)error NS_SWIFT_NAME(data());
@end

/// A decoded archive — `odr::ArchiveFile`.
NS_SWIFT_NAME(ArchiveFile)
@interface ODRArchiveFile : ODRDecodedFile
- (nullable ODRArchive *)archiveWithError:(NSError **)error
    NS_SWIFT_NAME(archive());
@end

/// A decoded document file — `odr::DocumentFile`.
NS_SWIFT_NAME(DocumentFile)
@interface ODRDocumentFile : ODRDecodedFile
/// Opens the document file at `path`.
- (nullable instancetype)initWithPath:(NSString *)path error:(NSError **)error;

@property(nonatomic, readonly) ODRDocumentType documentType;
/// The preview image the package carries, `nil` where it carries none or is
/// still encrypted. Never rendered by us.
@property(nonatomic, readonly, nullable) ODRFile *thumbnail;
- (nullable ODRDocumentFile *)decryptWithPassword:(NSString *)password
                                            error:(NSError **)error;
/// Decodes the document. The expensive step.
- (nullable ODRDocument *)documentWithError:(NSError **)error
    NS_SWIFT_NAME(document());
@end

/// A decoded PDF — `odr::PdfFile`.
NS_SWIFT_NAME(PdfFile)
@interface ODRPdfFile : ODRDecodedFile
/// Applies markup annotations — the payload the rendered page's
/// `odr.annotation.getAnnotations()` collects — and returns the annotated pdf.
- (nullable NSData *)annotate:(NSString *)annotations
                        error:(NSError **)error NS_SWIFT_NAME(annotate(_:));
- (nullable ODRPdfFile *)decryptWithPassword:(NSString *)password
                                       error:(NSError **)error;
@end

/// A decoded font file — `odr::FontFile`.
NS_SWIFT_NAME(FontFile)
@interface ODRFontFile : ODRDecodedFile
/// The font bytes, as stored.
- (nullable NSData *)dataWithError:(NSError **)error NS_SWIFT_NAME(data());
@end

NS_ASSUME_NONNULL_END
