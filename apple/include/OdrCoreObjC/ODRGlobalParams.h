#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Where odrcore looks for the files it needs at runtime.
///
/// The framework points `odrCoreDataPath` at its own bundle before `main` runs,
/// so an app that ships this framework unmodified never has to call anything
/// here. Set it only to override that — from
/// `application:didFinishLaunching...`, which is late enough to win.
NS_SWIFT_NAME(GlobalParams)
@interface ODRGlobalParams : NSObject

/// The css and js of the HTML renderer.
@property(class, nonatomic, copy) NSString *odrCoreDataPath;
/// The libmagic database (`magic.mgc`).
///
/// Deprecated and inert: libmagic is gone and nothing reads this. It still
/// returns whatever is set, so a caller that sets it keeps working — detection
/// is odrcore's own now and needs no database.
@property(class, nonatomic, copy) NSString *libmagicDatabasePath;

/// Points `odrCoreDataPath` at this framework's bundle. Runs automatically at
/// load; public because a consumer who relocated the resources — or reset the
/// path and wants the default back — needs a way to redo it.
+ (void)bootstrapFromFrameworkBundle;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
