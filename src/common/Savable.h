#ifndef ODR_COMMON_SAVABLE_H
#define ODR_COMMON_SAVABLE_H

#include <string>

namespace odr {

namespace access {
class Path;
}

namespace common {

class PossiblySavable {
public:
  virtual ~PossiblySavable() = default;

  virtual bool savable() const noexcept = 0;
  virtual void save(const access::Path &) const = 0;
};

class PossiblyPasswordEncryptedSavable : public PossiblySavable {
public:
  virtual ~PossiblyPasswordEncryptedSavable() = default;

  bool savable() const noexcept override { return savable(false); }
  virtual bool savable(bool encrypted) const noexcept = 0;
  virtual void save(const access::Path &,
                    const std::string &password) const = 0;
};

}
}

#endif // ODR_COMMON_SAVABLE_H
