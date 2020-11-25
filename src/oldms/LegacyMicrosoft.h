#ifndef ODR_OLDMS_LEGACYMICROSOFT_H
#define ODR_OLDMS_LEGACYMICROSOFT_H

#include <common/Document.h>
#include <odr/File.h>
#include <memory>

namespace odr {
namespace access {
class ReadStorage;
}

namespace oldms {

class LegacyMicrosoft final : public common::Document {
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

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

private:
  FileMeta meta_;
};

} // namespace oldms
} // namespace odr

#endif // ODR_OLDMS_LEGACYMICROSOFT_H
