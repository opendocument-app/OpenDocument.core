package app.opendocument.core;

/**
 * How to read a csv file. Mirrors {@code odr::CsvOptions}. An unset field is detected from the
 * file's opening bytes; a set one is taken as given.
 */
public final class CsvOptions {
  /** {@code null} to detect. */
  public TextEncoding encoding;

  /** {@code null} to detect. */
  public Character separator;

  /** {@code null} to detect. */
  public Character quote;

  // Flattened for the native layer: -1 for an unset field.
  int encodingNative() {
    return encoding == null ? -1 : encoding.toNative();
  }

  int separatorNative() {
    return separator == null ? -1 : separator;
  }

  int quoteNative() {
    return quote == null ? -1 : quote;
  }
}
