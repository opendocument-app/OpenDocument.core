#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class ODRHtmlService;
@class ODRLogger;

/// Serves connected `ODRHtmlService`s over HTTP — `odr::HttpServer`.
///
/// `listen` blocks, so it runs on a thread of yours. `stop` — and releasing the
/// last reference, which stops the server too — returns only once that thread
/// is out of `listen` again, so neither may be called from a request handler.
///
/// On iOS: bind `127.0.0.1`, not `0.0.0.0`. The latter trips the Local Network
/// permission prompt, and nothing off the device needs to reach this. A thread
/// blocked in `listen` is also subject to the app being suspended in the
/// background.
NS_SWIFT_NAME(HttpServer)
@interface ODRHttpServer : NSObject

- (instancetype)init;
- (instancetype)initWithLogger:(ODRLogger *)logger;

/// Serves `service` below `prefix`, which must match `[a-zA-Z0-9_-]+`.
///
/// A view of the service is then served at `/file/<prefix>/<view.path>` — the
/// `/file/` segment is part of the route, not something you add.
- (BOOL)connectService:(ODRHtmlService *)service
                prefix:(NSString *)prefix
                 error:(NSError **)error NS_SWIFT_NAME(connect(_:prefix:));

/// Binds the socket and reports the port it got. Pass 0 for any free one.
/// Connections land in the backlog from here on, before `listen` runs.
- (BOOL)bindToHost:(NSString *)host
              port:(uint32_t)port
         boundPort:(uint32_t *_Nullable)boundPort
             error:(NSError **)error NS_SWIFT_NAME(bind(host:port:boundPort:));

/// The same, with the socket options spelled out. POSIX only — Windows keeps
/// cpp-httplib's exclusive-address defaults, where these mean the opposite.
///
/// `reuseAddress` is `SO_REUSEADDR`, so a port held only by sockets in
/// TIME_WAIT can be bound again; on by default. `reusePort` is `SO_REUSEPORT`
/// where the platform has it, and is off by default because two live servers on
/// one port would silently share the incoming connections.
- (BOOL)bindToHost:(NSString *)host
              port:(uint32_t)port
      reuseAddress:(BOOL)reuseAddress
         reusePort:(BOOL)reusePort
         boundPort:(uint32_t *_Nullable)boundPort
             error:(NSError **)error
    NS_SWIFT_NAME(bind(host:port:reuseAddress:reusePort:boundPort:));

/// Serves what `bind` opened until `stop`. **Blocks.** Returns right away if
/// the server was already stopped.
- (BOOL)listenWithError:(NSError **)error NS_SWIFT_NAME(listen());

/// Whether a `listen` is in flight. False again once it has returned, which is
/// what `stop` waits for.
@property(nonatomic, readonly, getter=isRunning) BOOL running;

/// Drops the connected services. Files they were translated into are yours and
/// are left alone.
- (BOOL)clearWithError:(NSError **)error NS_SWIFT_NAME(clear());

/// Stops `listen` and releases the socket, blocking until `listen` has
/// returned — nothing is serving any more once this returns.
- (BOOL)stopWithError:(NSError **)error NS_SWIFT_NAME(stop());

@end

NS_ASSUME_NONNULL_END
