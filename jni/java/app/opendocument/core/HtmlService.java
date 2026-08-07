package app.opendocument.core;

import java.util.ArrayList;
import java.util.List;

/**
 * Lazily renders a translated file; views materialize on access. Mirrors
 * {@code odr::HtmlService}. Obtained via {@link Html#translate}.
 */
public final class HtmlService extends NativeResource {
  HtmlService(long handle, Object owner) {
    super(handle, owner, HtmlService::destroy);
  }

  public HtmlConfig config() {
    return configNative(handle());
  }

  public List<HtmlView> listViews() {
    long[] handles = listViewsNative(handle());
    List<HtmlView> result = new ArrayList<>(handles.length);
    for (long h : handles) {
      result.add(new HtmlView(h, this));
    }
    return result;
  }

  /** Renders everything into the cache eagerly. */
  public void warmup() {
    warmupNative(handle());
  }

  public boolean exists(String path) {
    return existsNative(handle(), path);
  }

  public String mimetype(String path) {
    return mimetypeNative(handle(), path);
  }

  /** Renders one view path into a byte array. */
  public byte[] write(String path) {
    return writeNative(handle(), path);
  }

  /** Renders one view path; returns the HTML and its resources. */
  public Html.Content writeHtml(String path) {
    return writeHtmlNative(handle(), path);
  }

  /** Renders all views into {@code outputPath} as self-contained files. */
  public Html bringOffline(String outputPath) {
    return bringOfflineNative(handle(), outputPath);
  }

  /** Renders the given views into {@code outputPath} as self-contained files. */
  public Html bringOffline(String outputPath, List<HtmlView> views) {
    long[] handles = new long[views.size()];
    for (int i = 0; i < handles.length; i++) {
      handles[i] = views.get(i).handle();
    }
    try {
      return bringOfflineViewsNative(handle(), outputPath, handles);
    } finally {
      // the handles went in as arguments, so nothing else keeps the views alive
      for (HtmlView view : views) {
        view.keepAlive();
      }
    }
  }

  private static native void destroy(long handle);

  private native HtmlConfig configNative(long handle);

  private native long[] listViewsNative(long handle);

  private native void warmupNative(long handle);

  private native boolean existsNative(long handle, String path);

  private native String mimetypeNative(long handle, String path);

  private native byte[] writeNative(long handle, String path);

  private native Html.Content writeHtmlNative(long handle, String path);

  private native Html bringOfflineNative(long handle, String outputPath);

  private native Html bringOfflineViewsNative(long handle, String outputPath, long[] viewHandles);
}
