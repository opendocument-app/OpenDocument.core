package app.opendocument.core;

/** Global resource paths of the library. Mirrors {@code odr::GlobalParams}. */
public final class GlobalParams {
  static {
    NativeLibrary.load();
  }

  public static native String odrCoreDataPath();

  /**
   * @deprecated Read only by a native library built with the deprecated {@code
   *     ODR_WITH_LIBMAGIC}. Detection is our own otherwise and needs no database.
   */
  @Deprecated
  public static native String libmagicDatabasePath();

  public static native void setOdrCoreDataPath(String path);

  /**
   * @deprecated See {@link #libmagicDatabasePath()}.
   */
  @Deprecated
  public static native void setLibmagicDatabasePath(String path);

  private GlobalParams() {}
}
