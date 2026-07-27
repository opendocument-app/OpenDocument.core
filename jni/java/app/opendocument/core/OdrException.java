package app.opendocument.core;

/**
 * Base class for exceptions thrown by the native library. The subclasses
 * mirror the typed exceptions in {@code odr/exceptions.hpp}; native errors
 * without a dedicated subclass are thrown as plain {@code OdrException}.
 */
public class OdrException extends RuntimeException {
  private static final long serialVersionUID = 1L;

  public OdrException(String message) {
    super(message);
  }

  public static final class UnsupportedOperation extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnsupportedOperation(String message) {
      super(message);
    }
  }

  public static final class FileNotFound extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileNotFound(String message) {
      super(message);
    }
  }

  public static final class UnknownFileType extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnknownFileType(String message) {
      super(message);
    }
  }

  public static final class UnsupportedFileType extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnsupportedFileType(String message) {
      super(message);
    }
  }

  public static final class FileReadError extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileReadError(String message) {
      super(message);
    }
  }

  public static final class FileWriteError extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileWriteError(String message) {
      super(message);
    }
  }

  public static final class NoDocumentFile extends OdrException {
    private static final long serialVersionUID = 1L;

    public NoDocumentFile(String message) {
      super(message);
    }
  }

  public static final class UnknownDocumentType extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnknownDocumentType(String message) {
      super(message);
    }
  }

  public static final class UnsupportedCryptoAlgorithm extends OdrException {
    private static final long serialVersionUID = 1L;

    public UnsupportedCryptoAlgorithm(String message) {
      super(message);
    }
  }

  public static final class WrongPassword extends OdrException {
    private static final long serialVersionUID = 1L;

    public WrongPassword(String message) {
      super(message);
    }
  }

  public static final class DecryptionFailed extends OdrException {
    private static final long serialVersionUID = 1L;

    public DecryptionFailed(String message) {
      super(message);
    }
  }

  public static final class NotEncrypted extends OdrException {
    private static final long serialVersionUID = 1L;

    public NotEncrypted(String message) {
      super(message);
    }
  }

  public static final class FileEncrypted extends OdrException {
    private static final long serialVersionUID = 1L;

    public FileEncrypted(String message) {
      super(message);
    }
  }

  public static final class DocumentCopyProtected extends OdrException {
    private static final long serialVersionUID = 1L;

    public DocumentCopyProtected(String message) {
      super(message);
    }
  }
}
