package app.opendocument.core;

/** Mirrors {@code odr::TextEncoding}; constant order must match the C++ declaration. */
public enum TextEncoding {
  UNKNOWN,
  UTF8,
  UTF16LE,
  UTF16BE,
  UTF32LE,
  UTF32BE,
  IBM866,
  ISO_8859_1,
  ISO_8859_2,
  ISO_8859_3,
  ISO_8859_4,
  ISO_8859_5,
  ISO_8859_6,
  ISO_8859_7,
  ISO_8859_8,
  ISO_8859_10,
  ISO_8859_13,
  ISO_8859_14,
  ISO_8859_15,
  ISO_8859_16,
  KOI8_R,
  KOI8_U,
  MACINTOSH,
  WINDOWS_874,
  WINDOWS_1250,
  WINDOWS_1251,
  WINDOWS_1252,
  WINDOWS_1253,
  WINDOWS_1254,
  WINDOWS_1255,
  WINDOWS_1256,
  WINDOWS_1257,
  WINDOWS_1258,
  X_MAC_CYRILLIC,
  BIG5,
  EUC_JP,
  EUC_KR,
  GB18030,
  ISO_2022_JP,
  ISO_2022_KR,
  SHIFT_JIS;

  static TextEncoding fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }

  /**
   * The canonical name, a label a browser accepts.
   *
   * @throws OdrException for {@link #UNKNOWN}, which has no name
   */
  public String canonicalName() {
    return toStringNative(toNative());
  }

  /** Every accepted name, canonical first. */
  public String[] names() {
    return namesNative(toNative());
  }

  /** Whether the library can decode this encoding, as opposed to merely naming it. */
  public boolean isDecodable() {
    return isDecodableNative(toNative());
  }

  /**
   * The encoding for a name, {@link #UNKNOWN} if none. Case and any {@code -}, {@code _} or
   * space are ignored.
   */
  public static TextEncoding byName(String name) {
    return fromNative(byNameNative(name));
  }

  /** Every encoding the library knows about, excluding {@link #UNKNOWN}. */
  public static TextEncoding[] all() {
    int[] codes = allNative();
    TextEncoding[] result = new TextEncoding[codes.length];
    for (int i = 0; i < codes.length; i++) {
      result[i] = fromNative(codes[i]);
    }
    return result;
  }

  static {
    NativeLibrary.load();
  }

  private static native String toStringNative(int encoding);

  private static native String[] namesNative(int encoding);

  private static native boolean isDecodableNative(int encoding);

  private static native int byNameNative(String name);

  private static native int[] allNative();
}
