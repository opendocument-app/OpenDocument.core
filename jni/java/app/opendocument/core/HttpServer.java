package app.opendocument.core;

import java.util.ArrayList;
import java.util.List;

/**
 * Serves translated files over HTTP. Mirrors {@code odr::HttpServer}. Only
 * available when the native library was built with the HTTP server
 * ({@link Odr#hasHttpServer()}).
 */
public final class HttpServer extends NativeResource {
  /** Mirrors {@code odr::HttpServer::Config}. */
  public static final class Config {
    public String cachePath = "/tmp/odr";
  }

  public HttpServer(Config config) {
    super(create(config.cachePath), null, HttpServer::destroy);
  }

  public Config config() {
    Config config = new Config();
    config.cachePath = cachePathNative(handle());
    return config;
  }

  /** Hosts the service under {@code /<prefix>/<view path>}. */
  public void connectService(HtmlService service, String prefix) {
    connectServiceNative(handle(), service.handle(), prefix);
  }

  /** Translates a decoded file and hosts it under {@code /file/<prefix>/<view path>}. */
  public List<HtmlView> serveFile(DecodedFile file, String prefix, HtmlConfig config) {
    long[] handles = serveFileNative(handle(), file.handle(), prefix, config);
    List<HtmlView> result = new ArrayList<>(handles.length);
    for (long h : handles) {
      result.add(new HtmlView(h, this));
    }
    return result;
  }

  /** Blocks serving requests until {@link #stop()} is called from another thread. */
  public void listen(String host, int port) {
    listenNative(handle(), host, port);
  }

  public void clear() {
    clearNative(handle());
  }

  public void stop() {
    stopNative(handle());
  }

  private static native long create(String cachePath);

  private static native void destroy(long handle);

  private native String cachePathNative(long handle);

  private native void connectServiceNative(long handle, long serviceHandle, String prefix);

  private native long[] serveFileNative(
      long handle, long fileHandle, String prefix, HtmlConfig config);

  private native void listenNative(long handle, String host, int port);

  private native void clearNative(long handle);

  private native void stopNative(long handle);
}
