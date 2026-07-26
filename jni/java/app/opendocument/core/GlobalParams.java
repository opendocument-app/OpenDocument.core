package app.opendocument.core;

/** Global resource paths of the library. Mirrors {@code odr::GlobalParams}. */
public final class GlobalParams {
  static {
    NativeLibrary.load();
  }

  public static native String odrCoreDataPath();

  public static native String libmagicDatabasePath();

  public static native void setOdrCoreDataPath(String path);

  public static native void setLibmagicDatabasePath(String path);

  private GlobalParams() {}
}
