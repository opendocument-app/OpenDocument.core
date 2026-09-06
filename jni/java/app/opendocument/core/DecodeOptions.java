package app.opendocument.core;

import java.util.ArrayList;
import java.util.List;

/**
 * How to decode a file. Mirrors {@code odr::DecodeOptions}. Every field is optional; the default
 * detects everything.
 */
public final class DecodeOptions {
  /** Decode as this file type; {@code null} to detect. */
  public FileType asFileType;

  /** Preferred types, most preferred first, among those detected. */
  public List<FileType> fileTypePriority = new ArrayList<>();

  /** Format-specific overrides for a file decoded as csv. */
  public CsvOptions csv = new CsvOptions();

  // Flattened for the native layer.
  int asFileTypeNative() {
    return asFileType == null ? -1 : asFileType.toNative();
  }

  int[] fileTypePriorityNative() {
    return fileTypePriority.stream().mapToInt(FileType::toNative).toArray();
  }
}
