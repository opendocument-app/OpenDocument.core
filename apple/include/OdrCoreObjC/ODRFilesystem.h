#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class ODRFile;
@class ODRFilesystem;

/// A cursor over a filesystem tree — `odr::FileWalker`.
///
/// Starts at the given path and is advanced by hand:
///
///     let walker = try filesystem.walker(path: "/")
///     while !walker.isAtEnd {
///       if walker.isFile { print(walker.path) }
///       walker.next()
///     }
NS_SWIFT_NAME(FileWalker)
@interface ODRFileWalker : NSObject

/// No more entries; every other property is meaningless once this is true.
@property(nonatomic, readonly, getter=isAtEnd) BOOL atEnd;
/// How far below the starting path the cursor is.
///
/// @warning Always 0 on a document or archive filesystem — see
/// opendocument-app/OpenDocument.core#639.
@property(nonatomic, readonly) uint32_t depth;
@property(nonatomic, readonly, copy) NSString *path;
@property(nonatomic, readonly, getter=isFile) BOOL file;
@property(nonatomic, readonly, getter=isDirectory) BOOL directory;

/// The next entry, descending into directories. The only one of the three that
/// works on a document or archive filesystem.
- (void)next;

/// Up one level.
///
/// @warning Does nothing on a document or archive filesystem, and `depth` is
/// always 0 there — see opendocument-app/OpenDocument.core#639.
- (void)pop;

/// The next entry at this level, skipping over a directory's contents.
///
/// @warning Does nothing on a document or archive filesystem, so
/// `while !isAtEnd { flatNext() }` never terminates — see
/// opendocument-app/OpenDocument.core#639. Use `next` until that is fixed.
- (void)flatNext;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// A readable filesystem — `odr::Filesystem`. A document's parts, or an
/// archive's contents.
NS_SWIFT_NAME(Filesystem)
@interface ODRFilesystem : NSObject

- (BOOL)existsAtPath:(NSString *)path NS_SWIFT_NAME(exists(path:));
- (BOOL)isFileAtPath:(NSString *)path NS_SWIFT_NAME(isFile(path:));
- (BOOL)isDirectoryAtPath:(NSString *)path NS_SWIFT_NAME(isDirectory(path:));

/// A cursor starting at `path`.
- (nullable ODRFileWalker *)walkerAtPath:(NSString *)path
                                   error:(NSError **)error
    NS_SWIFT_NAME(walker(path:));

/// Opens one entry.
- (nullable ODRFile *)openPath:(NSString *)path
                         error:(NSError **)error NS_SWIFT_NAME(open(path:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/// An archive — `odr::Archive`.
NS_SWIFT_NAME(Archive)
@interface ODRArchive : NSObject

/// The archive's contents as a filesystem.
@property(nonatomic, readonly) ODRFilesystem *filesystem;

/// Serialises the archive.
- (nullable NSData *)dataWithError:(NSError **)error NS_SWIFT_NAME(data());

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

NS_ASSUME_NONNULL_END
