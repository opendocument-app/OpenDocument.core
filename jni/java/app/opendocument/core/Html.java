package app.opendocument.core;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Entry points for HTML translation (mirrors the {@code odr::html} namespace)
 * and the offline output of {@link HtmlService#bringOffline}.
 */
public final class Html {
  static {
    NativeLibrary.load();
  }

  private final HtmlConfig config;
  private final List<HtmlPage> pages;

  Html(HtmlConfig config, HtmlPage[] pages) {
    this.config = config;
    this.pages = Collections.unmodifiableList(Arrays.asList(pages));
  }

  public HtmlConfig config() {
    return config;
  }

  public List<HtmlPage> pages() {
    return pages;
  }

  /** Rendered HTML of a single view plus the resources it references. */
  public static final class Content {
    public final String html;
    public final List<LocatedResource> resources;

    Content(String html, LocatedResource[] resources) {
      this.html = html;
      this.resources = Collections.unmodifiableList(Arrays.asList(resources));
    }
  }

  /** A resource together with its location; {@code location} may be {@code null}. */
  public static final class LocatedResource {
    public final HtmlResource resource;
    public final String location;

    LocatedResource(HtmlResource resource, String location) {
      this.resource = resource;
      this.location = location;
    }
  }

  // The handles below go into a static native as arguments, so no receiver holds
  // the wrapper for the duration - keepAlive() does.

  /** Translates a decoded file to HTML. */
  public static HtmlService translate(DecodedFile file, String cachePath, HtmlConfig config) {
    try {
      return new HtmlService(translateFile(file.handle(), cachePath, config), file);
    } finally {
      file.keepAlive();
    }
  }

  /** Translates a document to HTML. */
  public static HtmlService translate(Document document, String cachePath, HtmlConfig config) {
    try {
      return new HtmlService(translateDocument(document.handle(), cachePath, config), document);
    } finally {
      document.keepAlive();
    }
  }

  /** Translates a filesystem to HTML. */
  public static HtmlService translate(Filesystem filesystem, String cachePath, HtmlConfig config) {
    try {
      return new HtmlService(
          translateFilesystem(filesystem.handle(), cachePath, config), filesystem);
    } finally {
      filesystem.keepAlive();
    }
  }

  /** Applies a diff (produced by the browser-side editor) to a document. */
  public static void edit(Document document, String diff) {
    document.edit(diff);
  }

  private static native long translateFile(long fileHandle, String cachePath, HtmlConfig config);

  private static native long translateDocument(
      long documentHandle, String cachePath, HtmlConfig config);

  private static native long translateFilesystem(
      long filesystemHandle, String cachePath, HtmlConfig config);
}
