package app.opendocument.core;

/** Mirrors {@code odr::EncryptionState}; constant order must match the C++ declaration. */
public enum EncryptionState {
  UNKNOWN, NOT_ENCRYPTED, ENCRYPTED, DECRYPTED;

  static EncryptionState fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
