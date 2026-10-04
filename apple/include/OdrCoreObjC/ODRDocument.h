#import <Foundation/Foundation.h>

#import <OdrCoreObjC/ODRDocumentElement.h>
#import <OdrCoreObjC/ODRFile.h>
#import <OdrCoreObjC/ODRFilesystem.h>

NS_ASSUME_NONNULL_BEGIN

/// A cell of a sheet, by the sheet's ordinal — `odr::SheetPosition`.
NS_SWIFT_NAME(SheetPosition)
@interface ODRSheetPosition : NSObject

- (instancetype)initWithSheet:(uint32_t)sheet
                       column:(uint32_t)column
                          row:(uint32_t)row NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property(nonatomic, readonly) uint32_t sheet;
@property(nonatomic, readonly) uint32_t column;
@property(nonatomic, readonly) uint32_t row;

@end

/// What `-[ODRDocument recalculateWithError:]` did — `odr::Recalculation`.
/// Each list is in reading order.
NS_SWIFT_NAME(Recalculation)
@interface ODRRecalculation : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

/// The formula cells whose result changed, or that had none before.
@property(nonatomic, readonly) NSArray<ODRSheetPosition *> *changed;
/// The cells of a cycle, which have no result.
@property(nonatomic, readonly) NSArray<ODRSheetPosition *> *circular;
/// The stale formula cells nothing here computes.
@property(nonatomic, readonly) NSArray<ODRSheetPosition *> *unevaluated;

@end

/// A decoded document — `odr::Document`. Obtained from `ODRDocumentFile`.
NS_SWIFT_NAME(Document)
@interface ODRDocument : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

/// A new document of `type`, with one empty paragraph or one empty sheet.
/// Fails for a type whose capabilities do not state `create`.
+ (nullable instancetype)createWithFileType:(ODRFileType)type
                                      error:(NSError **)error
    NS_SWIFT_NAME(create(fileType:));

@property(nonatomic, readonly) ODRFileType fileType;
@property(nonatomic, readonly) ODRDocumentType documentType;
/// The language the document states for its content, as a BCP 47 tag such as
/// `de-DE`; nil where it states none.
@property(nonatomic, readonly, nullable) NSString *locale;

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

/// Computes the stale formula cells and writes each result into the
/// document. A save does so first where an edit left one stale.
- (nullable ODRRecalculation *)recalculateWithError:(NSError **)error
    NS_SWIFT_NAME(recalculate());

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
