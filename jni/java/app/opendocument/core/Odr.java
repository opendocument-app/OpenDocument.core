package app.opendocument.core;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Library-level functions. Mirrors the free functions in {@code odr/odr.hpp}. */
public final class Odr {
  static {
    NativeLibrary.load();
  }

  /** Version of the underlying odrcore library. */
  public static native String version();

  /** Git commit hash of the underlying odrcore library. */
  public static native String commitHash();

  public static native boolean isDirty();

  public static native boolean isDebug();

  /** Identification string of the underlying odrcore library. */
  public static native String identify();

  /** Whether the native library was built with the HTTP server. */
  public static native boolean hasHttpServer();

  /** Every file type this library knows about. */
  public static List<FileType> allFileTypes() {
    List<FileType> result = new ArrayList<>();
    for (int code : allFileTypesNative()) {
      result.add(FileType.fromNative(code));
    }
    return result;
  }

  public static FileType fileTypeByFileExtension(String extension) {
    return FileType.fromNative(fileTypeByFileExtensionNative(extension));
  }

  /** Every file extension accepted for the file type; the canonical one first. */
  public static List<String> fileExtensionsByFileType(FileType type) {
    return Arrays.asList(fileExtensionsByFileTypeNative(type.toNative()));
  }

  /** Canonical file extension for the file type, without a leading dot. */
  public static String fileExtensionByFileType(FileType type) {
    return fileExtensionByFileTypeNative(type.toNative());
  }

  public static FileCategory fileCategoryByFileType(FileType type) {
    return FileCategory.fromNative(fileCategoryByFileTypeNative(type.toNative()));
  }

  public static DocumentType documentTypeByFileType(FileType type) {
    return DocumentType.fromNative(documentTypeByFileTypeNative(type.toNative()));
  }

  public static String fileTypeToString(FileType type) {
    return fileTypeToStringNative(type.toNative());
  }

  public static String fileCategoryToString(FileCategory category) {
    return fileCategoryToStringNative(category.toNative());
  }

  public static String documentTypeToString(DocumentType type) {
    return documentTypeToStringNative(type.toNative());
  }

  public static FileType fileTypeByMimetype(String mimetype) {
    return FileType.fromNative(fileTypeByMimetypeNative(mimetype));
  }

  /** Canonical MIME type for the file type. */
  public static String mimetypeByFileType(FileType type) {
    return mimetypeByFileTypeNative(type.toNative());
  }

  /** Every MIME type accepted for the file type; the canonical one first. */
  public static List<String> mimetypesByFileType(FileType type) {
    return Arrays.asList(mimetypesByFileTypeNative(type.toNative()));
  }

  /** What this library can do with the file type. Format-level, i.e. an upper bound. */
  public static FileTypeCapabilities capabilitiesByFileType(FileType type) {
    return capabilitiesByFileTypeNative(type.toNative());
  }

  /** Determines the possible file types of a file. */
  public static List<FileType> listFileTypes(String path) {
    List<FileType> result = new ArrayList<>();
    for (int code : listFileTypesNative(path)) {
      result.add(FileType.fromNative(code));
    }
    return result;
  }

  /** Determines the MIME type of a file. */
  public static native String mimetype(String path);

  /** Opens and decodes a file. */
  public static DecodedFile open(String path) {
    return new DecodedFile(openNative(path));
  }

  /** Opens and decodes a file, reporting diagnostics to {@code logger}. */
  public static DecodedFile open(String path, Logger logger) {
    return new DecodedFile(openWithLoggerNative(path, logger.handle()));
  }

  /** Opens and decodes a file as the given file type. */
  public static DecodedFile open(String path, FileType as) {
    return new DecodedFile(openAsNative(path, as.toNative()));
  }

  /** Opens and decodes a file with a decode preference. */
  public static DecodedFile open(String path, DecodePreference preference) {
    return new DecodedFile(
        openWithPreferenceNative(
            path,
            preference.asFileTypeNative(),
            preference.fileTypePriorityNative()));
  }

  private static native int[] allFileTypesNative();

  private static native int fileTypeByFileExtensionNative(String extension);

  private static native String[] fileExtensionsByFileTypeNative(int type);

  private static native String fileExtensionByFileTypeNative(int type);

  private static native int fileCategoryByFileTypeNative(int type);

  private static native int documentTypeByFileTypeNative(int type);

  private static native String fileTypeToStringNative(int type);

  private static native String fileCategoryToStringNative(int category);

  private static native String documentTypeToStringNative(int type);

  private static native int fileTypeByMimetypeNative(String mimetype);

  private static native String mimetypeByFileTypeNative(int type);

  private static native String[] mimetypesByFileTypeNative(int type);

  private static native FileTypeCapabilities capabilitiesByFileTypeNative(int type);

  private static native int[] listFileTypesNative(String path);

  private static native long openNative(String path);

  private static native long openWithLoggerNative(String path, long logger);

  private static native long openAsNative(String path, int as);

  private static native long openWithPreferenceNative(
      String path, int asFileType, int[] fileTypePriority);

  private Odr() {}
}
