package app.opendocument.core;

/**
 * Loads the {@code odr_jni} native library once per JVM. The library is
 * resolved from {@code java.library.path}; the system property
 * {@code app.opendocument.core.library} overrides it with an absolute path.
 */
final class NativeLibrary {
  private static boolean loaded;

  static synchronized void load() {
    if (loaded) {
      return;
    }
    String explicit = System.getProperty("app.opendocument.core.library");
    if (explicit != null) {
      System.load(explicit);
    } else {
      System.loadLibrary("odr_jni");
    }
    loaded = true;
  }

  private NativeLibrary() {}
}
