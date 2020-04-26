#ifndef ODR_OLDMS_LEGACYMICROSOFT_H
#define ODR_OLDMS_LEGACYMICROSOFT_H

#include <common/Document.h>
#include <memory>

namespace odr {
namespace access {
class ReadStorage;
}

namespace oldms {

class LegacyMicrosoft : public common::Document {
public:
  explicit LegacyMicrosoft(const char *path);
  explicit LegacyMicrosoft(const std::string &path);
  explicit LegacyMicrosoft(const access::Path &path);
  explicit LegacyMicrosoft(std::unique_ptr<access::ReadStorage> &&storage);
  explicit LegacyMicrosoft(std::unique_ptr<access::ReadStorage> &storage);
  LegacyMicrosoft(const LegacyMicrosoft &) = delete;
  LegacyMicrosoft(LegacyMicrosoft &&) noexcept;
  LegacyMicrosoft &operator=(const LegacyMicrosoft &) = delete;
  LegacyMicrosoft &operator=(LegacyMicrosoft &&) noexcept;
  ~LegacyMicrosoft() final;

  const FileMeta &meta() const noexcept final;

  bool decrypted() const noexcept final;
  bool translatable() const noexcept final;
  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  bool decrypt(const std::string &password) final;

  void translate(const access::Path &path, const Config &config) final;

  void edit(const std::string &diff) final;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

private:
  FileMeta meta_;
};

} // namespace oldms
} // namespace odr

#endif // ODR_OLDMS_LEGACYMICROSOFT_H
