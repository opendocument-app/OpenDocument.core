#import <Foundation/Foundation.h>

#import <OdrCoreObjC/ODRDocumentElement.h>
#import <OdrCoreObjC/ODRFile.h>
#import <OdrCoreObjC/ODRFilesystem.h>

NS_ASSUME_NONNULL_BEGIN

/// A decoded document — `odr::Document`. Obtained from `ODRDocumentFile`.
NS_SWIFT_NAME(Document)
@interface ODRDocument : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property(nonatomic, readonly) ODRFileType fileType;
@property(nonatomic, readonly) ODRDocumentType documentType;

/// Whether edits can be applied back to this document.
@property(nonatomic, readonly) BOOL isEditable;
/// Whether `saveTo:` works. Ask separately for the encrypted case.
@property(nonatomic, readonly) BOOL isSavable;
/// Whether `saveTo:password:` works.
@property(nonatomic, readonly) BOOL isSavableEncrypted;

- (BOOL)saveTo:(NSString *)path error:(NSError **)error;
- (BOOL)saveTo:(NSString *)path
      password:(NSString *)password
         error:(NSError **)error;

/// The document's parts as a filesystem.
- (nullable ODRFilesystem *)filesystemWithError:(NSError **)error
    NS_SWIFT_NAME(filesystem());

/// The root of the element tree. Elements keep this document alive.
- (nullable ODRElement *)rootElementWithError:(NSError **)error
    NS_SWIFT_NAME(rootElement());

@end

NS_ASSUME_NONNULL_END
