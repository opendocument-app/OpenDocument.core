#ifndef ODR_ACCESS_SYSTEM_STORAGE_H
#define ODR_ACCESS_SYSTEM_STORAGE_H

#include <access/Storage.h>

namespace odr {
namespace access {

class SystemStorage : public Storage {
public:
  static const SystemStorage &instance();

  SystemStorage(const SystemStorage &) = delete;
  void operator=(const SystemStorage &) = delete;

  bool isSomething(const Path &) const final;
  bool isFile(const Path &) const final;
  bool isDirectory(const Path &) const final;
  bool isReadable(const Path &) const final;
  bool isWriteable(const Path &) const final;

  std::uint64_t size(const Path &) const final;

  bool remove(const Path &) const final;
  bool copy(const Path &, const Path &) const final;
  bool move(const Path &, const Path &) const final;

  bool createDirectory(const Path &) const final;

  void visit(Visitor) const final;

  std::unique_ptr<std::istream> read(const Path &) const final;
  std::unique_ptr<std::ostream> write(const Path &) const final;

private:
  SystemStorage() = default;
};

} // namespace access
} // namespace odr

#endif // ODR_ACCESS_SYSTEM_STORAGE_H
