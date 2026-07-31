#import <OdrCoreObjC/ODRHttpServer.h>

#import "ODRInternal.h"
#import "ODRPrivate.h"

#include <odr/http_server.hpp>

#include <optional>

using odr::apple::guarded;
using odr::apple::to_string;

@implementation ODRHttpServer {
  std::optional<odr::HttpServer> _handle;
}

- (instancetype)init {
  return [self initWithLogger:ODRLogger.null];
}

- (instancetype)initWithLogger:(ODRLogger *)logger {
  if ((self = [super init]) == nil) {
    return nil;
  }
  _handle.emplace(odr::HttpServerConfig{}, logger.handle);
  return self;
}

- (BOOL)connectService:(ODRHtmlService *)service
                prefix:(NSString *)prefix
                 error:(NSError **)error {
  return guarded(error, [&] {
    _handle->connect_service(service.handle, to_string(prefix));
    return YES;
  });
}

- (BOOL)bindToHost:(NSString *)host
              port:(uint32_t)port
         boundPort:(uint32_t *)boundPort
             error:(NSError **)error {
  const odr::HttpServerOptions defaults;
  return [self bindToHost:host
                     port:port
             reuseAddress:defaults.reuse_address ? YES : NO
                reusePort:defaults.reuse_port ? YES : NO
                boundPort:boundPort
                    error:error];
}

- (BOOL)bindToHost:(NSString *)host
              port:(uint32_t)port
      reuseAddress:(BOOL)reuseAddress
         reusePort:(BOOL)reusePort
         boundPort:(uint32_t *)boundPort
             error:(NSError **)error {
  return guarded(error, [&] {
    odr::HttpServerOptions native;
    native.reuse_address = reuseAddress == YES;
    native.reuse_port = reusePort == YES;
    const std::uint32_t bound = _handle->bind(to_string(host), port, native);
    if (boundPort != nullptr) {
      *boundPort = bound;
    }
    return YES;
  });
}

- (BOOL)listenWithError:(NSError **)error {
  return guarded(error, [&] {
    _handle->listen();
    return YES;
  });
}

- (BOOL)isRunning {
  return _handle->is_running() ? YES : NO;
}

- (BOOL)clearWithError:(NSError **)error {
  return guarded(error, [&] {
    _handle->clear();
    return YES;
  });
}

- (BOOL)stopWithError:(NSError **)error {
  return guarded(error, [&] {
    _handle->stop();
    return YES;
  });
}

@end
