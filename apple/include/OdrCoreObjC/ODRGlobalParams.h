#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Runtime paths odrcore no longer needs.
///
/// Nothing here has any effect: the renderer's css and js are part of the
/// library, and detection needs no database. Both properties still store and
/// return what is set, so a caller that configures them keeps working.
NS_SWIFT_NAME(GlobalParams)
@interface ODRGlobalParams : NSObject

/// Where the css and js of the HTML renderer used to be read from.
///
/// Deprecated and inert: they are written into the generated HTML now.
@property(class, nonatomic, copy) NSString *odrCoreDataPath;
/// The libmagic database (`magic.mgc`).
///
/// Deprecated and inert: libmagic is gone and nothing reads this. It still
/// returns whatever is set, so a caller that sets it keeps working — detection
/// is odrcore's own now and needs no database.
@property(class, nonatomic, copy) NSString *libmagicDatabasePath;

/// Points `odrCoreDataPath` at this framework's bundle.
///
/// Deprecated and inert: the framework carries no resources any more. Kept so
/// a caller that set the path up itself keeps compiling.
+ (void)bootstrapFromFrameworkBundle;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
