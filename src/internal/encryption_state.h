#ifndef ODR_INTERNAL_ENCRYPTION_STATE_H
#define ODR_INTERNAL_ENCRYPTION_STATE_H

namespace odr::internal {

enum class EncryptionState {
  UNKNOWN,
  NOT_ENCRYPTED,
  ENCRYPTED,
  DECRYPTED,
};

}

#endif // ODR_INTERNAL_ENCRYPTION_STATE_H
