#ifndef ODR_CFB_STORAGE_H
#define ODR_CFB_STORAGE_H

#include <common/storage.h>

namespace odr::cfb {

struct CfbException : public std::runtime_error {
  explicit CfbException(const char *desc) : std::runtime_error(desc) {}
};
struct NoCfbFileException : public CfbException {
  NoCfbFileException() : CfbException("no cfb file") {}
};
struct CfbFileCorruptedException : public CfbException {
  CfbFileCorruptedException() : CfbException("cfb file corrupted") {}
};

class CfbReader final : public common::ReadStorage {
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
