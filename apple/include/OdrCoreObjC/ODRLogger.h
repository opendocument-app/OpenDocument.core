#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ODRLogLevel) {
  ODRLogLevelVerbose = 0,
  ODRLogLevelDebug,
  ODRLogLevelInfo,
  ODRLogLevelWarning,
  ODRLogLevelError,
  ODRLogLevelFatal,
} NS_SWIFT_NAME(LogLevel);

/// A log sink — implement to route odrcore's logging into your own logger.
///
/// `willLog:` gates every call, so `log…` only sees levels you enabled. Calls
/// arrive on whatever thread the library is working on, which is not the main
/// one; an exception thrown out of a sink is described and swallowed rather
/// than allowed to derail the operation it was reporting on.
NS_SWIFT_NAME(LogSink)
@protocol ODRLogSink <NSObject>
- (BOOL)willLog:(ODRLogLevel)level;
/// `file` and `line` are the C++ source location the message came from.
- (void)logLevel:(ODRLogLevel)level
         message:(NSString *)message
            file:(NSString *)file
            line:(NSUInteger)line NS_SWIFT_NAME(log(_:message:file:line:));
- (void)flush;
@end

/// A handle to a log sink — `odr::Logger`. Copies share the sink.
NS_SWIFT_NAME(Logger)
@interface ODRLogger : NSObject

/// Discards everything. All instances share one sink.
@property(class, nonatomic, readonly) ODRLogger *null;

/// Writes to standard output.
+ (instancetype)stdioWithName:(NSString *)name
                        level:(ODRLogLevel)level
    NS_SWIFT_NAME(stdio(name:level:));

/// Routes into `sink`. The logger holds a strong reference to it.
+ (instancetype)loggerWithSink:(id<ODRLogSink>)sink NS_SWIFT_NAME(init(sink:));

/// Fans out to all of `loggers`. Fails if `loggers` is empty.
+ (nullable instancetype)teeWithLoggers:(NSArray<ODRLogger *> *)loggers
                                  error:(NSError **)error
    NS_SWIFT_NAME(tee(_:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (BOOL)willLog:(ODRLogLevel)level;
- (void)logLevel:(ODRLogLevel)level
         message:(NSString *)message NS_SWIFT_NAME(log(_:message:));
- (void)flush;

@end

NS_ASSUME_NONNULL_END
