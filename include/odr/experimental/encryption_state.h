#ifndef ODR_ENCRYPTION_STATE_H
#define ODR_ENCRYPTION_STATE_H

namespace odr::experimental {

enum class EncryptionState {
  UNKNOWN,
  NOT_ENCRYPTED,
  ENCRYPTED,
  DECRYPTED,
};

}

#endif // ODR_ENCRYPTION_STATE_H
