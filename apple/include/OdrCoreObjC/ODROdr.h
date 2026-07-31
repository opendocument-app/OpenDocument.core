#import <Foundation/Foundation.h>

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

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
