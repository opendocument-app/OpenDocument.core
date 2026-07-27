package app.opendocument.core;

import java.util.ArrayList;
import java.util.List;

/** Preference for decoding files. Mirrors {@code odr::DecodePreference}. */
public final class DecodePreference {
  /** Decode as this file type; {@code null} to detect. */
  public FileType asFileType;
  public List<FileType> fileTypePriority = new ArrayList<>();

  // Flattened for the native layer.
  int asFileTypeNative() {
    return asFileType == null ? -1 : asFileType.toNative();
  }

  int[] fileTypePriorityNative() {
    return fileTypePriority.stream().mapToInt(FileType::toNative).toArray();
  }
}
