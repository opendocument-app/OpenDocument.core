package app.opendocument.core;

/**
 * Serves translated files over HTTP. Mirrors {@code odr::HttpServer}. Only
 * available when the native library was built with the HTTP server
 * ({@link Odr#hasHttpServer()}).
 */
public final class HttpServer extends NativeResource {
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

  public HttpServer(Config config) {
    super(create(), null, HttpServer::destroy);
  }

  /** Hosts the service under {@code /<prefix>/<view path>}. */
  public void connectService(HtmlService service, String prefix) {
    connectServiceNative(handle(), service.handle(), prefix);
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

  /** Blocks serving requests until {@link #stop()} is called from another thread. */
  public void listen() {
    listenNative(handle());
  }

  public void clear() {
    clearNative(handle());
  }

  public void stop() {
    stopNative(handle());
  }

  private static native long create();

  private static native void destroy(long handle);

  private native void connectServiceNative(long handle, long serviceHandle, String prefix);

  private native int bindNative(
      long handle, String host, int port, boolean reuseAddress, boolean reusePort);

  private native void listenNative(long handle);

  private native void clearNative(long handle);

  private native void stopNative(long handle);
}
