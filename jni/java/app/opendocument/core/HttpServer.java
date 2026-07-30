package app.opendocument.core;

/**
 * Serves translated files over HTTP. Mirrors {@code odr::HttpServer}. Only
 * available when the native library was built with the HTTP server
 * ({@link Odr#hasHttpServer()}).
 *
 * <p>{@link #listen()} blocks and so runs on a thread of the caller's. {@link #stop()}
 * and {@link #close()} return only once it is back out of it, so tearing the server
 * down needs no join or timeout of its own, and {@code try (HttpServer server = new
 * HttpServer())} is safe around a listen thread.
 */
public final class HttpServer extends GuardedNativeResource {
  /**
   * Mirrors {@code odr::HttpServer::Config}. Empty since the cache path went with
   * {@code serveFile}: what a service was translated into belongs to whoever
   * translated it.
   */
  public static final class Config {}

  /** Mirrors {@code odr::HttpServer::Options}: socket options for {@link #bind}. */
  public static final class Options {
    /** SO_REUSEADDR, so a port held only by sockets in TIME_WAIT can be bound again. */
    public boolean reuseAddress = true;

    /**
     * SO_REUSEPORT where the platform has it. Two live servers on one port share the
     * incoming connections between them, hence off.
     */
    public boolean reusePort = false;
  }

  public HttpServer() {
    this(new Config());
  }

  public HttpServer(Config config) {
    super(create(), null, HttpServer::destroy);
  }

  /** Hosts the service under {@code /<prefix>/<view path>}. */
  public void connectService(HtmlService service, String prefix) {
    try {
      connectServiceNative(handle(), service.handle(), prefix);
    } finally {
      service.keepAlive();
    }
  }

  /**
   * Binds the socket and returns the port it got - pass {@code 0} for any free one.
   * Connections are accepted into the backlog from here on, before {@link #listen()}
   * runs, so a caller can hand out the port as soon as this returns.
   */
  public int bind(String host, int port) {
    return bind(host, port, new Options());
  }

  /** Binds with explicit socket options. */
  public int bind(String host, int port, Options options) {
    return bindNative(handle(), host, port, options.reuseAddress, options.reusePort);
  }

  /**
   * Blocks serving requests until {@link #stop()} is called from another thread.
   * Returns right away if the server has already been stopped or closed.
   *
   * <p>Guarded: this is the one call in the bindings that is meant to be made on a
   * thread of its own while another closes the object it runs on, so the handle it
   * takes has to stay valid until it hands it back.
   */
  public void listen() {
    guarded(this::listenNative);
  }

  /**
   * Whether a {@link #listen()} is in flight. False again once it has returned,
   * which is what {@link #stop()} waits for.
   */
  public boolean isRunning() {
    return isRunningNative(handle());
  }

  public void clear() {
    clearNative(handle());
  }

  /**
   * Stops {@link #listen()} and releases the socket. Blocks until {@code listen}
   * has returned, so nothing is serving any more once this returns - do not call
   * it from a request handler.
   */
  public void stop() {
    stopNative(handle());
  }

  /**
   * What {@link #close()} calls to bring a {@link #listen()} back: closing the
   * server is how that call is meant to end, and nothing else would end it.
   */
  @Override
  protected void unblock() {
    stop();
  }

  private static native long create();

  private static native void destroy(long handle);

  private native void connectServiceNative(long handle, long serviceHandle, String prefix);

  private native int bindNative(
      long handle, String host, int port, boolean reuseAddress, boolean reusePort);

  private native void listenNative(long handle);

  private native boolean isRunningNative(long handle);

  private native void clearNative(long handle);

  private native void stopNative(long handle);
}
