#import <OdrCoreObjC/ODRFile.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <istream>
#include <optional>
#include <vector>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::to_nsdata;
using odr::apple::to_nsstring;
using odr::apple::to_string;

ODR_SAME_ENUM(ODRFileTypeUnknown, odr::FileType::unknown);
ODR_SAME_ENUM(ODRFileTypeOpenDocumentText, odr::FileType::opendocument_text);
ODR_SAME_ENUM(ODRFileTypeOpenDocumentPresentation,
              odr::FileType::opendocument_presentation);
ODR_SAME_ENUM(ODRFileTypeOpenDocumentSpreadsheet,
              odr::FileType::opendocument_spreadsheet);
ODR_SAME_ENUM(ODRFileTypeOpenDocumentGraphics,
              odr::FileType::opendocument_graphics);
ODR_SAME_ENUM(ODRFileTypeOfficeOpenXmlDocument,
              odr::FileType::office_open_xml_document);
ODR_SAME_ENUM(ODRFileTypeOfficeOpenXmlPresentation,
              odr::FileType::office_open_xml_presentation);
ODR_SAME_ENUM(ODRFileTypeOfficeOpenXmlWorkbook,
              odr::FileType::office_open_xml_workbook);
ODR_SAME_ENUM(ODRFileTypeOfficeOpenXmlEncrypted,
              odr::FileType::office_open_xml_encrypted);
ODR_SAME_ENUM(ODRFileTypeExcelBinaryWorkbook,
              odr::FileType::excel_binary_workbook);
ODR_SAME_ENUM(ODRFileTypeLegacyWordDocument,
              odr::FileType::legacy_word_document);
ODR_SAME_ENUM(ODRFileTypeLegacyPowerpointPresentation,
              odr::FileType::legacy_powerpoint_presentation);
ODR_SAME_ENUM(ODRFileTypeLegacyExcelWorksheets,
              odr::FileType::legacy_excel_worksheets);
ODR_SAME_ENUM(ODRFileTypeWordPerfect, odr::FileType::word_perfect);
ODR_SAME_ENUM(ODRFileTypeRichTextFormat, odr::FileType::rich_text_format);
ODR_SAME_ENUM(ODRFileTypePortableDocumentFormat,
              odr::FileType::portable_document_format);
ODR_SAME_ENUM(ODRFileTypeTextFile, odr::FileType::text_file);
ODR_SAME_ENUM(ODRFileTypeCommaSeparatedValues,
              odr::FileType::comma_separated_values);
ODR_SAME_ENUM(ODRFileTypeJavascriptObjectNotation,
              odr::FileType::javascript_object_notation);
ODR_SAME_ENUM(ODRFileTypeMarkdown, odr::FileType::markdown);
ODR_SAME_ENUM(ODRFileTypeZip, odr::FileType::zip);
ODR_SAME_ENUM(ODRFileTypeCompoundFileBinaryFormat,
              odr::FileType::compound_file_binary_format);
ODR_SAME_ENUM(ODRFileTypePortableNetworkGraphics,
              odr::FileType::portable_network_graphics);
ODR_SAME_ENUM(ODRFileTypeGraphicsInterchangeFormat,
              odr::FileType::graphics_interchange_format);
ODR_SAME_ENUM(ODRFileTypeJpeg, odr::FileType::jpeg);
ODR_SAME_ENUM(ODRFileTypeBitmapImageFile, odr::FileType::bitmap_image_file);
ODR_SAME_ENUM(ODRFileTypeStarviewMetafile, odr::FileType::starview_metafile);
ODR_SAME_ENUM(ODRFileTypeTruetypeFont, odr::FileType::truetype_font);
ODR_SAME_ENUM(ODRFileTypeOpentypeFont, odr::FileType::opentype_font);
ODR_SAME_ENUM(ODRFileTypeWebp, odr::FileType::webp);
ODR_SAME_ENUM(ODRFileTypeTaggedImageFileFormat,
              odr::FileType::tagged_image_file_format);
ODR_SAME_ENUM(ODRFileTypeHighEfficiencyImageFormat,
              odr::FileType::high_efficiency_image_format);
ODR_SAME_ENUM(ODRFileTypeAv1ImageFileFormat,
              odr::FileType::av1_image_file_format);
ODR_SAME_ENUM(ODRFileTypeMpegAudio, odr::FileType::mpeg_audio);
ODR_SAME_ENUM(ODRFileTypeMpeg4Audio, odr::FileType::mpeg4_audio);
ODR_SAME_ENUM(ODRFileTypeOggAudio, odr::FileType::ogg_audio);
ODR_SAME_ENUM(ODRFileTypeWaveformAudio, odr::FileType::waveform_audio);
ODR_SAME_ENUM(ODRFileTypeFreeLosslessAudioCodec,
              odr::FileType::free_lossless_audio_codec);
ODR_SAME_ENUM(ODRFileTypeMpeg4Video, odr::FileType::mpeg4_video);
ODR_SAME_ENUM(ODRFileTypeQuicktimeVideo, odr::FileType::quicktime_video);
ODR_SAME_ENUM(ODRFileTypeThirdGenerationPartnershipVideo,
              odr::FileType::third_generation_partnership_video);
ODR_SAME_ENUM(ODRFileTypeMatroskaVideo, odr::FileType::matroska_video);
ODR_SAME_ENUM(ODRFileTypeAudioVideoInterleave,
              odr::FileType::audio_video_interleave);
ODR_SAME_ENUM(ODRFileTypeScalableVectorGraphics,
              odr::FileType::scalable_vector_graphics);
ODR_SAME_ENUM(ODRFileTypeWindowsIcon, odr::FileType::windows_icon);
ODR_SAME_ENUM(ODRFileTypeJpegXl, odr::FileType::jpeg_xl);
ODR_SAME_ENUM(ODRFileTypeJpeg2000, odr::FileType::jpeg_2000);
ODR_SAME_ENUM(ODRFileTypePhotoshopDocument, odr::FileType::photoshop_document);
ODR_SAME_ENUM(ODRFileTypeWindowsMetafile, odr::FileType::windows_metafile);
ODR_SAME_ENUM(ODRFileTypeEnhancedMetafile, odr::FileType::enhanced_metafile);
ODR_SAME_ENUM(ODRFileTypeXml, odr::FileType::xml);

ODR_SAME_ENUM(ODRFileTypeIworkPages, odr::FileType::iwork_pages);
ODR_SAME_ENUM(ODRFileTypeIworkNumbers, odr::FileType::iwork_numbers);
ODR_SAME_ENUM(ODRFileTypeIworkKeynote, odr::FileType::iwork_keynote);
ODR_SAME_ENUM(ODRFileTypeHypertextMarkupLanguage,
              odr::FileType::hypertext_markup_language);

ODR_SAME_ENUM(ODRFileCategoryUnknown, odr::FileCategory::unknown);
ODR_SAME_ENUM(ODRFileCategoryText, odr::FileCategory::text);
ODR_SAME_ENUM(ODRFileCategoryImage, odr::FileCategory::image);
ODR_SAME_ENUM(ODRFileCategoryArchive, odr::FileCategory::archive);
ODR_SAME_ENUM(ODRFileCategoryDocument, odr::FileCategory::document);
ODR_SAME_ENUM(ODRFileCategoryFont, odr::FileCategory::font);
ODR_SAME_ENUM(ODRFileCategoryAudio, odr::FileCategory::audio);
ODR_SAME_ENUM(ODRFileCategoryVideo, odr::FileCategory::video);

ODR_SAME_ENUM(ODRFileLocationUnknown, odr::FileLocation::unknown);
ODR_SAME_ENUM(ODRFileLocationMemory, odr::FileLocation::memory);
ODR_SAME_ENUM(ODRFileLocationDisk, odr::FileLocation::disk);

ODR_SAME_ENUM(ODREncryptionStateUnknown, odr::EncryptionState::unknown);
ODR_SAME_ENUM(ODREncryptionStateNotEncrypted,
              odr::EncryptionState::not_encrypted);
ODR_SAME_ENUM(ODREncryptionStateEncrypted, odr::EncryptionState::encrypted);
ODR_SAME_ENUM(ODREncryptionStateDecrypted, odr::EncryptionState::decrypted);

ODR_SAME_ENUM(ODRDocumentTypeUnknown, odr::DocumentType::unknown);
ODR_SAME_ENUM(ODRDocumentTypeText, odr::DocumentType::text);
ODR_SAME_ENUM(ODRDocumentTypePresentation, odr::DocumentType::presentation);
ODR_SAME_ENUM(ODRDocumentTypeSpreadsheet, odr::DocumentType::spreadsheet);
ODR_SAME_ENUM(ODRDocumentTypeDrawing, odr::DocumentType::drawing);

namespace {

NSArray<NSNumber *> *to_nsarray(const std::vector<odr::FileType> &types) {
  NSMutableArray<NSNumber *> *const result =
      [NSMutableArray arrayWithCapacity:types.size()];
  for (const odr::FileType type : types) {
    [result addObject:@(static_cast<NSInteger>(type))];
  }
  return result;
}

std::vector<odr::FileType> to_file_types(NSArray<NSNumber *> *numbers) {
  std::vector<odr::FileType> result;
  result.reserve(numbers.count);
  for (NSNumber *const number in numbers) {
    result.push_back(static_cast<odr::FileType>(number.integerValue));
  }
  return result;
}

/// `nil` for an unset `std::optional`, the way a Java binding would use -1.
NSString *_Nullable to_nsstring(const std::optional<std::string> &value) {
  return value.has_value() ? to_nsstring(*value) : nil;
}

} // namespace

#pragma mark - ODRFileTypeCapabilities

@implementation ODRFileTypeCapabilities

+ (instancetype)capabilitiesWithHandle:
    (const odr::FileTypeCapabilities &)handle {
  ODRFileTypeCapabilities *const result =
      [[ODRFileTypeCapabilities alloc] init];
  result->_detectByContent = handle.detect_by_content ? YES : NO;
  result->_open = handle.open ? YES : NO;
  result->_decrypt = handle.decrypt ? YES : NO;
  result->_translateHtml = handle.translate_html ? YES : NO;
  result->_colorScheme = handle.color_scheme ? YES : NO;
  result->_edit = handle.edit ? YES : NO;
  result->_save = handle.save ? YES : NO;
  result->_encrypt = handle.encrypt ? YES : NO;
  return result;
}

@end

#pragma mark - ODRFileMeta

@implementation ODRFileMeta

+ (instancetype)metaWithHandle:(const odr::FileMeta &)handle {
  ODRFileMeta *const result = [[ODRFileMeta alloc] init];
  result->_type = static_cast<ODRFileType>(handle.type);
  result->_mimetype = to_nsstring(handle.mimetype);
  result->_passwordEncrypted = handle.password_encrypted ? YES : NO;
  result->_documentType = static_cast<ODRDocumentType>(handle.document_type);
  result->_entryCount =
      handle.entry_count.has_value() ? @(*handle.entry_count) : nil;
  result->_title = to_nsstring(handle.title);
  result->_author = to_nsstring(handle.author);
  result->_subject = to_nsstring(handle.subject);
  result->_keywords = to_nsstring(handle.keywords);
  result->_creator = to_nsstring(handle.creator);
  result->_producer = to_nsstring(handle.producer);
  result->_creationDate = to_nsstring(handle.creation_date);
  result->_modificationDate = to_nsstring(handle.modification_date);
  return result;
}

@end

#pragma mark - ODRDecodePreference

@implementation ODRDecodePreference

- (instancetype)init {
  if ((self = [super init]) != nil) {
    _fileTypePriority = @[];
  }
  return self;
}

@end

#pragma mark - ODRFile

@implementation ODRFile {
  // `odr::File` is default-constructible, but every handle here is held the
  // same way so the pattern does not have to be re-derived per class.
  std::optional<odr::File> _handle;
}

+ (instancetype)fileWithHandle:(odr::File)handle {
  ODRFile *const result = [[ODRFile alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::File &)handle {
  return *_handle;
}

- (nullable instancetype)initWithPath:(NSString *)path error:(NSError **)error {
  if ((self = [super init]) == nil) {
    return nil;
  }
  const bool ok = guarded(error, [&] {
    _handle = odr::File(to_string(path));
    return true;
  });
  return ok ? self : nil;
}

- (ODRFileLocation)location {
  return static_cast<ODRFileLocation>(_handle->location());
}

- (NSUInteger)size {
  return guarded_value([&] { return static_cast<NSUInteger>(_handle->size()); },
                       NSUInteger{0});
}

- (NSString *)name {
  return guarded_value([&] { return to_nsstring(_handle->name()); }, @"");
}

- (nullable NSString *)diskPath {
  return guarded_value(
      [&]() -> NSString * {
        const std::optional<std::string> path = _handle->disk_path();
        return path.has_value() ? to_nsstring(*path) : nil;
      },
      nil);
}

- (nullable NSData *)dataWithError:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    const std::unique_ptr<std::istream> stream = _handle->stream();
    return to_nsdata(*stream);
  });
}

- (BOOL)copyTo:(NSString *)path error:(NSError **)error {
  return guarded(error, [&] {
    _handle->copy(to_string(path));
    return YES;
  });
}

@end

#pragma mark - ODRDecodedFile

@implementation ODRDecodedFile {
  std::optional<odr::DecodedFile> _handle;
}

+ (instancetype)decodedFileWithHandle:(odr::DecodedFile)handle {
  // The most derived wrapper the file qualifies for, so a caller never has to
  // downcast something it already knows the type of. The predicates are
  // mutually exclusive — a PDF is not an `is_document_file`, despite reporting
  // `FileCategory::document`.
  Class klass = [ODRDecodedFile class];
  if (handle.is_pdf_file()) {
    klass = [ODRPdfFile class];
  } else if (handle.is_document_file()) {
    klass = [ODRDocumentFile class];
  } else if (handle.is_text_file()) {
    klass = [ODRTextFile class];
  } else if (handle.is_image_file()) {
    klass = [ODRImageFile class];
  } else if (handle.is_archive_file()) {
    klass = [ODRArchiveFile class];
  } else if (handle.is_font_file()) {
    klass = [ODRFontFile class];
  }

  ODRDecodedFile *const result = [[klass alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::DecodedFile &)handle {
  return *_handle;
}

+ (nullable instancetype)decodePath:(NSString *)path error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    return [ODRDecodedFile
        decodedFileWithHandle:odr::DecodedFile(to_string(path))];
  });
}

+ (nullable instancetype)decodePath:(NSString *)path
                                 as:(ODRFileType)type
                              error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    return [ODRDecodedFile
        decodedFileWithHandle:odr::DecodedFile(
                                  to_string(path),
                                  static_cast<odr::FileType>(type))];
  });
}

+ (nullable instancetype)decodePath:(NSString *)path
                         preference:(ODRDecodePreference *)preference
                              error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    odr::DecodePreference native;
    if (preference.asFileType != nil) {
      native.as_file_type =
          static_cast<odr::FileType>(preference.asFileType.integerValue);
    }
    native.file_type_priority = to_file_types(preference.fileTypePriority);
    return [ODRDecodedFile
        decodedFileWithHandle:odr::DecodedFile(to_string(path), native)];
  });
}

+ (nullable instancetype)decodeFile:(ODRFile *)file error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    return [ODRDecodedFile decodedFileWithHandle:odr::DecodedFile(file.handle)];
  });
}

+ (nullable instancetype)decodePath:(NSString *)path
                             logger:(ODRLogger *)logger
                              error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    return [ODRDecodedFile
        decodedFileWithHandle:odr::DecodedFile(to_string(path), logger.handle)];
  });
}

+ (nullable instancetype)decodePath:(NSString *)path
                                 as:(ODRFileType)type
                             logger:(ODRLogger *)logger
                              error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    return [ODRDecodedFile
        decodedFileWithHandle:odr::DecodedFile(to_string(path),
                                               static_cast<odr::FileType>(type),
                                               logger.handle)];
  });
}

+ (nullable instancetype)decodeFile:(ODRFile *)file
                             logger:(ODRLogger *)logger
                              error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    return [ODRDecodedFile
        decodedFileWithHandle:odr::DecodedFile(file.handle, logger.handle)];
  });
}

+ (nullable NSArray<NSNumber *> *)listFileTypesAtPath:(NSString *)path
                                               logger:(ODRLogger *)logger
                                                error:(NSError **)error {
  return guarded(error, [&]() -> NSArray<NSNumber *> * {
    return to_nsarray(
        odr::DecodedFile::list_file_types(to_string(path), logger.handle));
  });
}

+ (nullable NSArray<NSNumber *> *)listFileTypesAtPath:(NSString *)path
                                                error:(NSError **)error {
  return guarded(error, [&]() -> NSArray<NSNumber *> * {
    return to_nsarray(odr::DecodedFile::list_file_types(to_string(path)));
  });
}

+ (nullable NSString *)mimetypeAtPath:(NSString *)path error:(NSError **)error {
  return guarded(error, [&]() -> NSString * {
    return to_nsstring(odr::DecodedFile::mimetype(to_string(path)));
  });
}

- (ODRFile *)file {
  return guarded_value(
      [&]() -> ODRFile * { return [ODRFile fileWithHandle:_handle->file()]; },
      nil);
}

- (ODRFileType)fileType {
  return static_cast<ODRFileType>(_handle->file_type());
}

- (ODRFileCategory)fileCategory {
  return static_cast<ODRFileCategory>(_handle->file_category());
}

- (ODRFileMeta *)fileMeta {
  return [ODRFileMeta metaWithHandle:_handle->file_meta()];
}

- (BOOL)isDecodable {
  return _handle->is_decodable() ? YES : NO;
}

- (BOOL)isPasswordEncrypted {
  return guarded_value([&] { return _handle->password_encrypted() ? YES : NO; },
                       NO);
}

- (ODREncryptionState)encryptionState {
  return guarded_value(
      [&] {
        return static_cast<ODREncryptionState>(_handle->encryption_state());
      },
      ODREncryptionStateUnknown);
}

- (nullable ODRDecodedFile *)decryptWithPassword:(NSString *)password
                                           error:(NSError **)error {
  return guarded(error, [&]() -> ODRDecodedFile * {
    return [ODRDecodedFile
        decodedFileWithHandle:_handle->decrypt(to_string(password))];
  });
}

- (ODRFileTypeCapabilities *)capabilities {
  return guarded_value(
      [&]() -> ODRFileTypeCapabilities * {
        return [ODRFileTypeCapabilities
            capabilitiesWithHandle:_handle->capabilities()];
      },
      nil);
}

- (BOOL)isTextFile {
  return guarded_value([&] { return _handle->is_text_file() ? YES : NO; }, NO);
}
- (BOOL)isImageFile {
  return guarded_value([&] { return _handle->is_image_file() ? YES : NO; }, NO);
}
- (BOOL)isArchiveFile {
  return guarded_value([&] { return _handle->is_archive_file() ? YES : NO; },
                       NO);
}
- (BOOL)isDocumentFile {
  return guarded_value([&] { return _handle->is_document_file() ? YES : NO; },
                       NO);
}
- (BOOL)isPdfFile {
  return guarded_value([&] { return _handle->is_pdf_file() ? YES : NO; }, NO);
}
- (BOOL)isFontFile {
  return guarded_value([&] { return _handle->is_font_file() ? YES : NO; }, NO);
}

// The `as…` conversions go through the same factory, so they return the
// already-correct wrapper class; the C++ call is what validates the type.
- (nullable ODRTextFile *)asTextFileWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRTextFile * {
    return static_cast<ODRTextFile *>(
        [ODRDecodedFile decodedFileWithHandle:_handle->as_text_file()]);
  });
}

- (nullable ODRImageFile *)asImageFileWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRImageFile * {
    return static_cast<ODRImageFile *>(
        [ODRDecodedFile decodedFileWithHandle:_handle->as_image_file()]);
  });
}

- (nullable ODRArchiveFile *)asArchiveFileWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRArchiveFile * {
    return static_cast<ODRArchiveFile *>(
        [ODRDecodedFile decodedFileWithHandle:_handle->as_archive_file()]);
  });
}

- (nullable ODRDocumentFile *)asDocumentFileWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRDocumentFile * {
    return static_cast<ODRDocumentFile *>(
        [ODRDecodedFile decodedFileWithHandle:_handle->as_document_file()]);
  });
}

- (nullable ODRPdfFile *)asPdfFileWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRPdfFile * {
    return static_cast<ODRPdfFile *>(
        [ODRDecodedFile decodedFileWithHandle:_handle->as_pdf_file()]);
  });
}

- (nullable ODRFontFile *)asFontFileWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRFontFile * {
    return static_cast<ODRFontFile *>(
        [ODRDecodedFile decodedFileWithHandle:_handle->as_font_file()]);
  });
}

@end

#pragma mark - typed views

@implementation ODRTextFile

- (nullable NSString *)charset {
  return guarded_value(
      [&]() -> NSString * {
        const odr::TextEncoding encoding =
            self.handle.as_text_file().encoding();
        return encoding == odr::TextEncoding::unknown
                   ? nil
                   : to_nsstring(
                         std::string(odr::text_encoding_to_string(encoding)));
      },
      nil);
}

- (nullable NSString *)textWithError:(NSError **)error {
  return guarded(error, [&]() -> NSString * {
    return to_nsstring(self.handle.as_text_file().text());
  });
}

@end

@implementation ODRImageFile

- (nullable NSData *)dataWithError:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    const std::unique_ptr<std::istream> stream =
        self.handle.as_image_file().stream();
    return to_nsdata(*stream);
  });
}

@end

@implementation ODRArchiveFile

- (nullable ODRArchive *)archiveWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRArchive * {
    return
        [ODRArchive archiveWithHandle:self.handle.as_archive_file().archive()];
  });
}

@end

@implementation ODRDocumentFile

- (nullable instancetype)initWithPath:(NSString *)path error:(NSError **)error {
  ODRDecodedFile *const decoded = [ODRDecodedFile decodePath:path error:error];
  if (decoded == nil) {
    return nil;
  }
  return [decoded asDocumentFileWithError:error];
}

- (odr::DocumentFile)documentHandle {
  return self.handle.as_document_file();
}

- (ODRDocumentType)documentType {
  return guarded_value(
      [&] {
        return static_cast<ODRDocumentType>(
            self.documentHandle.document_type());
      },
      ODRDocumentTypeUnknown);
}

- (nullable ODRFile *)thumbnail {
  return guarded_value(
      [&]() -> ODRFile * {
        const std::optional<odr::File> thumbnail =
            self.documentHandle.thumbnail();
        return thumbnail.has_value() ? [ODRFile fileWithHandle:*thumbnail]
                                     : nil;
      },
      nil);
}

- (nullable ODRDocumentFile *)decryptWithPassword:(NSString *)password
                                            error:(NSError **)error {
  return guarded(error, [&]() -> ODRDocumentFile * {
    return static_cast<ODRDocumentFile *>(
        [ODRDecodedFile decodedFileWithHandle:self.documentHandle.decrypt(
                                                  to_string(password))]);
  });
}

- (nullable ODRDocument *)documentWithError:(NSError **)error {
  return guarded(error, [&]() -> ODRDocument * {
    return [ODRDocument documentWithHandle:self.documentHandle.document()];
  });
}

@end

@implementation ODRPdfFile

- (nullable ODRPdfFile *)decryptWithPassword:(NSString *)password
                                       error:(NSError **)error {
  return guarded(error, [&]() -> ODRPdfFile * {
    return static_cast<ODRPdfFile *>(
        [ODRDecodedFile decodedFileWithHandle:self.handle.as_pdf_file().decrypt(
                                                  to_string(password))]);
  });
}

@end

@implementation ODRFontFile

- (nullable NSData *)dataWithError:(NSError **)error {
  return guarded(error, [&]() -> NSData * {
    const std::unique_ptr<std::istream> stream =
        self.handle.as_font_file().stream();
    return to_nsdata(*stream);
  });
}

@end
