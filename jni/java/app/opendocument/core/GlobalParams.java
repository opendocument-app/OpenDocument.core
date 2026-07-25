package app.opendocument.core;

/** Global resource paths of the library. Mirrors {@code odr::GlobalParams}. */
public final class GlobalParams {
  static {
    NativeLibrary.load();
  }

  public static native String odrCoreDataPath();

  public static native String fontconfigDataPath();

  public static native String popplerDataPath();

  public static native String pdf2htmlexDataPath();

  public static native String libmagicDatabasePath();

  public static native void setOdrCoreDataPath(String path);

  public static native void setFontconfigDataPath(String path);

  public static native void setPopplerDataPath(String path);

  public static native void setPdf2htmlexDataPath(String path);

  public static native void setLibmagicDatabasePath(String path);

  private GlobalParams() {}
}
