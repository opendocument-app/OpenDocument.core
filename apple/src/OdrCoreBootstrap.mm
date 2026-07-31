#import <OdrCoreObjC/ODRGlobalParams.h>

/// Points odrcore at the resources this framework carries, before `main`.
///
/// `+load` rather than lazy initialisation on first use, because
/// `HtmlConfig::init()` *snapshots* `GlobalParams::odr_core_data_path()` when
/// it is constructed: a caller reaching odrcore's C++ directly — which
/// OpenDocument.ios does today — would otherwise get a config built from an
/// empty path. It is safe this far up: Foundation is in this image's
/// `LC_LOAD_DYLIB`, so `NSBundle` is live by the time dyld runs `+load`, and
/// `GlobalParams::instance()` is a function-local static, so nothing needs to
/// have been constructed first.
///
/// This only works because the framework is a *dynamic* one. In a static
/// framework nothing references this translation unit, the linker drops it,
/// and `+load` never runs.
@interface OdrCoreBootstrap : NSObject
@end

@implementation OdrCoreBootstrap

+ (void)load {
  [ODRGlobalParams bootstrapFromFrameworkBundle];
}

@end
