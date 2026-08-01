package app.opendocument.core;

/** Global resource paths of the library. Mirrors {@code odr::GlobalParams}. */
public final class GlobalParams {
  static {
    NativeLibrary.load();
  }

  public static native String odrCoreDataPath();

  /**
   * @deprecated Inert: libmagic is gone and nothing reads this. It still returns whatever was set,
   *     so a caller that sets it keeps working — detection is our own now and needs no database.
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
