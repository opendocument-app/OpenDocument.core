#import <Foundation/Foundation.h>

#import <OdrCoreObjC/ODRFile.h>

NS_ASSUME_NONNULL_BEGIN

/// Library-level entry points — `odr/odr.hpp`.
NS_SWIFT_NAME(Odr)
@interface ODROdr : NSObject

/// The library version.
///
/// Not `version`: `NSObject` already declares `+version` returning an
/// `NSInteger`, and a class property of that name shadows it with an
/// incompatible type — Swift resolves `Odr.version` to `NSObject`'s method
/// instead, silently.
@property(class, nonatomic, readonly, copy) NSString *libraryVersion;
/// The commit the library was built from. A release build reports its tag.
@property(class, nonatomic, readonly, copy) NSString *commitHash;
/// Whether the working tree had uncommitted changes at build time.
@property(class, nonatomic, readonly) BOOL isDirty;
/// Whether this is a debug build.
@property(class, nonatomic, readonly) BOOL isDebug;
/// Version, commit and build flavour in one string, for logs and bug reports.
@property(class, nonatomic, readonly, copy) NSString *identification;

/// Every file type the library knows about, in declaration order, including
/// `ODRFileTypeUnknown`.
@property(class, nonatomic, readonly) NSArray<NSNumber *> *allFileTypes;

/// The type an extension maps to, `ODRFileTypeUnknown` if none does. Without a
/// leading dot.
+ (ODRFileType)fileTypeForExtension:(NSString *)extension
    NS_SWIFT_NAME(fileType(extension:));
/// Every extension accepted for a type, canonical one first, without leading
/// dots. Empty for types carried by another format's extension, e.g.
/// `ODRFileTypeOfficeOpenXmlEncrypted`.
+ (NSArray<NSString *> *)extensionsForFileType:(ODRFileType)type
    NS_SWIFT_NAME(extensions(fileType:));
/// The canonical extension. Fails for a type that has none of its own.
+ (nullable NSString *)extensionForFileType:(ODRFileType)type
                                      error:(NSError **)error
    NS_SWIFT_NAME(extension(fileType:));

/// The type a MIME type maps to, `ODRFileTypeUnknown` if none does.
+ (ODRFileType)fileTypeForMimetype:(NSString *)mimetype
    NS_SWIFT_NAME(fileType(mimetype:));
/// Every MIME type accepted for a type, canonical one first.
+ (NSArray<NSString *> *)mimetypesForFileType:(ODRFileType)type
    NS_SWIFT_NAME(mimetypes(fileType:));
/// The canonical MIME type. Fails for a type that has none.
+ (nullable NSString *)mimetypeForFileType:(ODRFileType)type
                                     error:(NSError **)error
    NS_SWIFT_NAME(mimetype(fileType:));

+ (ODRFileCategory)fileCategoryForFileType:(ODRFileType)type
    NS_SWIFT_NAME(fileCategory(fileType:));
+ (ODRDocumentType)documentTypeForFileType:(ODRFileType)type
    NS_SWIFT_NAME(documentType(fileType:));

/// What the library can do with a format. Declared support, an upper bound —
/// ask `ODRDecodedFile.capabilities` about a file you actually hold.
+ (ODRFileTypeCapabilities *)capabilitiesForFileType:(ODRFileType)type
    NS_SWIFT_NAME(capabilities(fileType:));

+ (NSString *)stringForFileType:(ODRFileType)type
    NS_SWIFT_NAME(string(fileType:));
+ (NSString *)stringForFileCategory:(ODRFileCategory)category
    NS_SWIFT_NAME(string(fileCategory:));
+ (NSString *)stringForDocumentType:(ODRDocumentType)type
    NS_SWIFT_NAME(string(documentType:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
