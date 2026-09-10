package app.opendocument.core;

/**
 * Base class for exceptions thrown by the native library. The subclasses are
 * named after {@code odr::ErrorCode}; a code with no subclass here arrives as
 * plain {@code OdrException}.
 */
public class OdrException extends RuntimeException {
  private static final long serialVersionUID = 1L;

  /** {@code odr::ErrorCode::unknown}. */
  private static final int UNKNOWN = 1;

  private final int code;

  public OdrException(String message) {
    this(message, UNKNOWN);
  }

  public OdrException(String message, int code) {
    super(message);
    this.code = code;
  }

  /**
   * The {@code odr::ErrorCode}, the same number the rendered page reports
   * through {@code odr.onError} and {@code odr.onEditRefused}.
   */
  public int getCode() {
    return code;
  }

  public static final class UnsupportedOperation extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnsupportedOperation(String message) {
      super(message);
    }

    public UnsupportedOperation(String message, int code) {
      super(message, code);
    }
  }

  public static final class FileNotFound extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileNotFound(String message) {
      super(message);
    }

    public FileNotFound(String message, int code) {
      super(message, code);
    }
  }

  public static final class UnknownFileType extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnknownFileType(String message) {
      super(message);
    }

    public UnknownFileType(String message, int code) {
      super(message, code);
    }
  }

  public static final class UnsupportedFileType extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnsupportedFileType(String message) {
      super(message);
    }

    public UnsupportedFileType(String message, int code) {
      super(message, code);
    }
  }

  public static final class FileReadError extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileReadError(String message) {
      super(message);
    }

    public FileReadError(String message, int code) {
      super(message, code);
    }
  }

  public static final class FileWriteError extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileWriteError(String message) {
      super(message);
    }

    public FileWriteError(String message, int code) {
      super(message, code);
    }
  }

  public static final class NoDocumentFile extends OdrException {
    private static final long serialVersionUID = 1L;

    public NoDocumentFile(String message) {
      super(message);
    }

    public NoDocumentFile(String message, int code) {
      super(message, code);
    }
  }

  public static final class UnknownDocumentType extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnknownDocumentType(String message) {
      super(message);
    }

    public UnknownDocumentType(String message, int code) {
      super(message, code);
    }
  }

  public static final class UnsupportedCryptoAlgorithm extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnsupportedCryptoAlgorithm(String message) {
      super(message);
    }

    public UnsupportedCryptoAlgorithm(String message, int code) {
      super(message, code);
    }
  }

  public static final class WrongPassword extends OdrException {
    private static final long serialVersionUID = 1L;

    public WrongPassword(String message) {
      super(message);
    }

    public WrongPassword(String message, int code) {
      super(message, code);
    }
  }

  public static final class DecryptionFailed extends OdrException {
    private static final long serialVersionUID = 1L;

    public DecryptionFailed(String message) {
      super(message);
    }

    public DecryptionFailed(String message, int code) {
      super(message, code);
    }
  }

  public static final class NotEncrypted extends OdrException {
    private static final long serialVersionUID = 1L;

    public NotEncrypted(String message) {
      super(message);
    }

    public NotEncrypted(String message, int code) {
      super(message, code);
    }
  }

  public static final class FileEncrypted extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileEncrypted(String message) {
      super(message);
    }

    public FileEncrypted(String message, int code) {
      super(message, code);
    }
  }

  public static final class DocumentCopyProtected extends OdrException {
    private static final long serialVersionUID = 1L;

    public DocumentCopyProtected(String message) {
      super(message);
    }

    public DocumentCopyProtected(String message, int code) {
      super(message, code);
    }
  }
}
