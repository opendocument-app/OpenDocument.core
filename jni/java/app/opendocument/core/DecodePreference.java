package app.opendocument.core;

import java.util.ArrayList;
import java.util.List;

/** Preference for decoding files. Mirrors {@code odr::DecodePreference}. */
public final class DecodePreference {
  /** Decode as this file type; {@code null} to detect. */
  public FileType asFileType;
  /** Decode with this engine; {@code null} to choose automatically. */
  public DecoderEngine withEngine;

  public List<FileType> fileTypePriority = new ArrayList<>();
  public List<DecoderEngine> enginePriority = new ArrayList<>();

  // Flattened for the native layer.
  int asFileTypeNative() {
    return asFileType == null ? -1 : asFileType.toNative();
  }

  int withEngineNative() {
    return withEngine == null ? -1 : withEngine.toNative();
  }

  int[] fileTypePriorityNative() {
    return fileTypePriority.stream().mapToInt(FileType::toNative).toArray();
  }

  int[] enginePriorityNative() {
    return enginePriority.stream().mapToInt(DecoderEngine::toNative).toArray();
  }
}
