package app.opendocument.core;

/**
 * What this library can do with a file format. Mirrors {@code
 * odr::FileTypeCapabilities}.
 *
 * <p>Declared, format-level support — an <em>upper bound</em>. A concrete file may still fail
 * (corrupt, encrypted, an unsupported sub-variant); ask {@link DecodedFile} or {@link Document} for
 * the precise answer. The point of the static query is the decisions a caller has to make
 * <em>before</em> it holds a file, e.g. which MIME types to advertise to the system file picker.
 */
public final class FileTypeCapabilities {
  /** Recognised from its bytes alone, without a file name or MIME type. */
  public final boolean detectByContent;

  /** A decoder exists; {@link Odr#open(String)} can decode it. */
  public final boolean open;

  /** Encrypted instances can be decrypted. */
  public final boolean decrypt;

  /** {@link Html#translate} produces output. */
  public final boolean translateHtml;

  /** {@link Document#isEditable} can be {@code true}. */
  public final boolean edit;

  /** {@link Document#save} is supported. */
  public final boolean save;

  /** {@link Document#save} with a password is supported. */
  public final boolean encrypt;

  FileTypeCapabilities(
      boolean detectByContent,
      boolean open,
      boolean decrypt,
      boolean translateHtml,
      boolean edit,
      boolean save,
      boolean encrypt) {
    this.detectByContent = detectByContent;
    this.open = open;
    this.decrypt = decrypt;
    this.translateHtml = translateHtml;
    this.edit = edit;
    this.save = save;
    this.encrypt = encrypt;
  }
}
