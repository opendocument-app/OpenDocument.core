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

/// Applies the operations our browser-side editor produces, in order.
///
/// Editing a single element in process is `ODRText.setContent:` and needs none
/// of this.
- (BOOL)edit:(NSString *)operations
       error:(NSError **)error NS_SWIFT_NAME(edit(operations:));

- (BOOL)saveTo:(NSString *)path error:(NSError **)error;
- (BOOL)saveTo:(NSString *)path
      password:(NSString *)password
         error:(NSError **)error;

/// The saved document as bytes.
- (nullable NSData *)saveToMemoryWithError:(NSError **)error
    NS_SWIFT_NAME(saveToMemory());
- (nullable NSData *)saveToMemoryWithPassword:(NSString *)password
                                        error:(NSError **)error
    NS_SWIFT_NAME(saveToMemory(password:));

/// The document's parts as a filesystem.
- (nullable ODRFilesystem *)filesystemWithError:(NSError **)error
    NS_SWIFT_NAME(filesystem());

/// The root of the element tree. Elements keep this document alive.
- (nullable ODRElement *)rootElementWithError:(NSError **)error
    NS_SWIFT_NAME(rootElement());

/// The element `ODRElement.identifier` handed out, or `nil` where this
/// document holds no such id. Not an error, so it does not throw — an id that
/// is gone is the ordinary answer.
- (nullable ODRElement *)elementWithIdentifier:(uint64_t)identifier
    NS_SWIFT_NAME(element(identifier:));

#pragma mark - Structural edits

/// Each fails where the engine cannot write, and for an element of another
/// document.

/// Removes an element and its subtree; its identifier stays taken.
- (BOOL)removeElement:(ODRElement *)element
                error:(NSError **)error NS_SWIFT_NAME(remove(_:));

/// A run before `anchor`, in the same parent, so it takes the same style.
- (nullable ODRText *)insertTextBefore:(ODRText *)anchor
                                  text:(NSString *)text
                                 error:(NSError **)error
    NS_SWIFT_NAME(insertText(before:text:));

/// A run after `anchor`, in the same parent.
- (nullable ODRText *)insertTextAfter:(ODRText *)anchor
                                 text:(NSString *)text
                                error:(NSError **)error
    NS_SWIFT_NAME(insertText(after:text:));

/// A run as the last child of `parent`.
- (nullable ODRText *)appendTextTo:(ODRElement *)parent
                              text:(NSString *)text
                             error:(NSError **)error
    NS_SWIFT_NAME(appendText(to:text:));

/// Splits `paragraph` after `after` — one of its descendants — into a new
/// paragraph of the same style. A `nil` `after` moves every child.
- (nullable ODRParagraph *)splitParagraph:(ODRParagraph *)paragraph
                                    after:(nullable ODRElement *)after
                                    error:(NSError **)error
    NS_SWIFT_NAME(splitParagraph(_:after:));

/// `paragraph` takes the children of the paragraph after it, which then goes.
- (BOOL)mergeParagraphWithNext:(ODRParagraph *)paragraph
                         error:(NSError **)error
    NS_SWIFT_NAME(mergeParagraphWithNext(_:));

/// An empty paragraph after `paragraph`, of the same style.
- (nullable ODRParagraph *)insertParagraphAfter:(ODRParagraph *)paragraph
                                          error:(NSError **)error
    NS_SWIFT_NAME(insertParagraph(after:));

@end

NS_ASSUME_NONNULL_END
