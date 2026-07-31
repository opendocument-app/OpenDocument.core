#import <Foundation/Foundation.h>

#import <OdrCoreObjC/ODRFile.h>

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

// `rootElement` and `filesystem` land with ODRDocumentElement.h and
// ODRFilesystem.h. They are deliberately absent rather than declared against a
// forward declaration: Swift cannot import a method whose return type is an
// incomplete ObjC class, so such a method is invisible to the callers who need
// it most, while still looking bound from ObjC.

@end

NS_ASSUME_NONNULL_END
