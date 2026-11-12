package app.opendocument.core;

/** A bitmap fill. Mirrors {@code odr::BitmapFill}. */
public final class BitmapFill {
  public final File file;
  /** Where the document holds the image; empty where it is embedded. */
  public final String href;
  public final BitmapRepeat repeat;

  BitmapFill(long fileHandle, String href, int repeat) {
    this.file = new File(fileHandle);
    this.href = href;
    this.repeat = BitmapRepeat.fromNative(repeat);
  }
}
