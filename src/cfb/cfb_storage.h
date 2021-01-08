#ifndef ODR_CFB_STORAGE_H
#define ODR_CFB_STORAGE_H

#include <abstract/storage.h>

namespace odr::cfb {

class CfbReader final : public abstract::ReadStorage {
public:
  explicit CfbReader(const common::Path &);
  ~CfbReader() final;

  bool isSomething(const common::Path &) const final;
  bool isFile(const common::Path &) const final;
  bool isDirectory(const common::Path &) const final;
  bool isReadable(const common::Path &) const final;

  std::uint64_t size(const common::Path &) const final;

  void visit(Visitor) const final;

  std::unique_ptr<std::istream> read(const common::Path &) const final;

private:
  class Impl;
  const std::unique_ptr<Impl> impl;
};

} // namespace odr::cfb

#endif // ODR_CFB_STORAGE_H
