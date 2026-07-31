#import <OdrCoreObjC/ODRLogger.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/logger.hpp>

#include <memory>
#include <optional>
#include <source_location>
#include <vector>

using odr::apple::guarded;
using odr::apple::guarded_value;
using odr::apple::guarded_void;
using odr::apple::to_nsstring;
using odr::apple::to_string;

ODR_SAME_ENUM(ODRLogLevelVerbose, odr::LogLevel::verbose);
ODR_SAME_ENUM(ODRLogLevelDebug, odr::LogLevel::debug);
ODR_SAME_ENUM(ODRLogLevelInfo, odr::LogLevel::info);
ODR_SAME_ENUM(ODRLogLevelWarning, odr::LogLevel::warning);
ODR_SAME_ENUM(ODRLogLevelError, odr::LogLevel::error);
ODR_SAME_ENUM(ODRLogLevelFatal, odr::LogLevel::fatal);

namespace {

/// Routes `odr::ILogger` into an ObjC sink, the analogue of `jni_logger.cpp`'s
/// `JavaLogger`.
///
/// Holds the sink strongly: the C++ logger can outlive every ObjC reference the
/// caller kept, and a sink collected out from under it would be a use after
/// free on a background thread.
class SinkLogger final : public odr::ILogger {
public:
  explicit SinkLogger(id<ODRLogSink> sink) : m_sink{sink} {}

  [[nodiscard]] bool will_log(const odr::LogLevel level) const final {
    @autoreleasepool {
      return [m_sink willLog:static_cast<ODRLogLevel>(level)] == YES;
    }
  }

  void log(const Time, const odr::LogLevel level, const std::string &message,
           const std::source_location &location) final {
    // Log calls arrive on whatever thread the library works on, so each one
    // gets its own pool rather than leaking into the caller's.
    @autoreleasepool {
      @try {
        [m_sink logLevel:static_cast<ODRLogLevel>(level)
                 message:odr::apple::to_nsstring(message)
                    file:odr::apple::to_nsstring(
                             std::string_view(location.file_name()))
                    line:location.line()];
      } @catch (NSException *const exception) {
        // A logger must not derail the operation it is reporting on.
        NSLog(@"odr: log sink threw %@: %@", exception.name, exception.reason);
      }
    }
  }

  void flush() final {
    @autoreleasepool {
      @try {
        [m_sink flush];
      } @catch (NSException *const exception) {
        NSLog(@"odr: log sink threw %@: %@", exception.name, exception.reason);
      }
    }
  }

private:
  id<ODRLogSink> m_sink;
};

} // namespace

@implementation ODRLogger {
  std::optional<odr::Logger> _handle;
}

+ (instancetype)loggerWithHandle:(odr::Logger)handle {
  ODRLogger *const result = [[ODRLogger alloc] init];
  result->_handle = std::move(handle);
  return result;
}

- (const odr::Logger &)handle {
  return *_handle;
}

+ (ODRLogger *)null {
  return [ODRLogger loggerWithHandle:odr::Logger::null()];
}

+ (instancetype)stdioWithName:(NSString *)name level:(ODRLogLevel)level {
  return [ODRLogger
      loggerWithHandle:odr::Logger::create_stdio(
                           to_string(name), static_cast<odr::LogLevel>(level))];
}

+ (instancetype)loggerWithSink:(id<ODRLogSink>)sink {
  return [ODRLogger
      loggerWithHandle:odr::Logger(std::make_shared<SinkLogger>(sink))];
}

+ (nullable instancetype)teeWithLoggers:(NSArray<ODRLogger *> *)loggers
                                  error:(NSError **)error {
  return guarded(error, [&]() -> ODRLogger * {
    std::vector<odr::Logger> native;
    native.reserve(loggers.count);
    for (ODRLogger *const logger in loggers) {
      native.push_back(logger.handle);
    }
    return [ODRLogger loggerWithHandle:odr::Logger::create_tee(native)];
  });
}

- (BOOL)willLog:(ODRLogLevel)level {
  return guarded_value(
      [&] {
        return _handle->will_log(static_cast<odr::LogLevel>(level)) ? YES : NO;
      },
      NO);
}

- (void)logLevel:(ODRLogLevel)level message:(NSString *)message {
  guarded_void([&] {
    _handle->log(static_cast<odr::LogLevel>(level), to_string(message));
  });
}

- (void)flush {
  guarded_void([&] { _handle->flush(); });
}

@end
