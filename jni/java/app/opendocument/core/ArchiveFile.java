package app.opendocument.core;

/** A decoded archive file. Mirrors {@code odr::ArchiveFile}. */
public final class ArchiveFile extends DecodedFile {
  ArchiveFile(long handle) {
    super(handle);
  }

  public Archive archive() {
    return new Archive(archiveNative(handle()));
  }

  private native long archiveNative(long handle);
}
