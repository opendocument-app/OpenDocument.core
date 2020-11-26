#ifndef ODR_COMMON_ENCRYPTED_H
#define ODR_COMMON_ENCRYPTED_H

#include <string>
#include <common/File.h>

namespace odr {
enum class EncryptionState;

namespace common {

class PasswordEncrypted {
public:
  virtual ~PasswordEncrypted() = default;

  virtual bool decrypt(const std::string &password) = 0;
};

class PossiblyEncryptedFileBase : public File {
public:
  virtual EncryptionState encryptionState() const = 0;
};

template <typename F>
class PossiblyEncryptedFile : public PossiblyEncryptedFileBase {
public:
  virtual std::unique_ptr<F> unbox() = 0;
};

template <typename F>
class PossiblyPasswordEncryptedFile : public PasswordEncrypted,
                                      public PossiblyEncryptedFile<F> {};

}
}

#endif // ODR_COMMON_ENCRYPTED_H
