import Foundation

extension HttpServer {
  /// Binds, serves, and stops when the returned handle is released or
  /// cancelled.
  ///
  /// `listen()` blocks its thread until `stop()`, which is a shape no Swift
  /// caller wants to manage by hand — and getting it wrong deadlocks, because
  /// `stop()` waits for `listen()` to return and so must never be called from
  /// the thread that is inside it. This runs `listen()` on a detached thread of
  /// its own and hands back the port.
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

    let thread = Thread { [self] in
      // A failure here is not the caller's to catch — it has already been
      // handed its port and moved on. The logger the server was built with is
      // where this belongs.
      try? listen()
    }
    thread.name = "app.opendocument.OdrCore.HttpServer"
    thread.start()

    return ServerHandle(server: self, port: bound)
  }

  /// Keeps a served server alive and stops it exactly once.
  public final class ServerHandle {
    /// The port the server actually bound, which is what you asked for unless
    /// you asked for 0.
    public let port: UInt32

    private let server: HttpServer
    private var stopped = false
    private let lock = NSLock()

    fileprivate init(server: HttpServer, port: UInt32) {
      self.server = server
      self.port = port
    }

    /// The base URL a connected service's views are served under.
    ///
    /// The `/file/` segment is part of the route, not something a caller adds.
    public func url(prefix: String) -> URL {
      URL(string: "http://127.0.0.1:\(port)/file/\(prefix)/")!
    }

    /// Stops the server, blocking until it is no longer serving. Idempotent.
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
