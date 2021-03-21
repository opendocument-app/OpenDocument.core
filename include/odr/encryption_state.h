#ifndef ODR_STATE_H
#define ODR_STATE_H

namespace odr {

enum class EncryptionState {
  UNKNOWN,
  NOT_ENCRYPTED,
  ENCRYPTED,
  DECRYPTED,
};

}

#endif // ODR_STATE_H
