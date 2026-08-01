import Foundation

extension HttpServer {
  /// Binds, serves, and stops when the returned handle is released or
  /// cancelled.
  ///
  /// `listen()` blocks its thread until `stop()`, which is a shape no Swift
  /// caller wants to manage by hand — and getting it wrong deadlocks, because
  /// `stop()` waits for `listen()` to return and so must never be called from
  /// the thread that is inside it. This runs `listen()` on a detached thread of
  /// its own and hands back the port. Returns only once the server is actually
  /// serving, and throws rather than hand back a handle if it never gets there.
  ///
  /// Bind `127.0.0.1` on iOS. `0.0.0.0` trips the Local Network permission
  /// prompt, and nothing off the device needs to reach a server that exists to
  /// feed a web view.
  public func serve(
    host: String = "127.0.0.1",
    port: UInt32 = 0
  ) throws -> ServerHandle {
    var bound: UInt32 = 0
    try bind(host: host, port: port, boundPort: &bound)

    let failure = ErrorBox()
    let thread = Thread { [self] in
      do {
        try listen()
      } catch {
        failure.store(error)
      }
    }
    thread.name = "app.opendocument.OdrCore.HttpServer"
    thread.start()

    // `listen()` runs on that thread, so `serve()` would otherwise return
    // before the server is actually serving and `isRunning` would be false to
    // the caller that just started it. Connections queue in the backlog from
    // `bind()` onward, so this is about the observable state being honest
    // rather than about correctness of the first request. Bounded, because a
    // server that was already stopped never starts running at all.
    let deadline = Date().addingTimeInterval(5)
    while !isRunning, failure.stored == nil, Date() < deadline {
      Thread.sleep(forTimeInterval: 0.005)
    }

    // Nothing is serving: `listen()` failed, or returned right away because the
    // server had already been stopped. A handle would claim otherwise.
    guard isRunning else {
      throw failure.stored
        ?? NSError(
          domain: ODRErrorDomain, code: ODRError.unknown.rawValue,
          userInfo: [NSLocalizedDescriptionKey: "the server did not start serving"])
    }

    return ServerHandle(server: self, host: host, port: bound)
  }

  /// Keeps a served server alive and stops it exactly once.
  public final class ServerHandle {
    /// The host the server bound.
    public let host: String

    /// The port the server actually bound, which is what you asked for unless
    /// you asked for 0.
    public let port: UInt32

    private let server: HttpServer
    private var stopped = false
    private let lock = NSLock()

    fileprivate init(server: HttpServer, host: String, port: UInt32) {
      self.server = server
      self.host = host
      self.port = port
    }

    /// The base URL a connected service's views are served under.
    ///
    /// The `/file/` segment is part of the route, not something a caller adds.
    public func url(prefix: String) -> URL {
      // A bare `::1` would parse as an empty host and a port of `:1`.
      let authority = host.contains(":") ? "[\(host)]" : host
      return URL(string: "http://\(authority):\(port)/file/\(prefix)/")!
    }

    /// Stops the server, blocking until it is no longer serving. Idempotent.
    ///
    /// - Warning: takes ~5 seconds once anything has been served
    ///   (opendocument-app/OpenDocument.core#641), so keep it off the main
    ///   thread. `deinit` calls this, which means releasing the last reference
    ///   to a handle blocks too.
    public func stop() {
      lock.lock()
      defer { lock.unlock() }
      guard !stopped else { return }
      stopped = true
      try? server.stop()
    }

    deinit {
      stop()
    }
  }
}

/// `listen()` fails on the thread it was started on, where there is nobody to
/// catch it; this carries it back to the `serve()` still waiting to return.
private final class ErrorBox {
  private var error: Error?
  private let lock = NSLock()

  func store(_ error: Error) {
    lock.lock()
    defer { lock.unlock() }
    self.error = error
  }

  var stored: Error? {
    lock.lock()
    defer { lock.unlock() }
    return error
  }
}
