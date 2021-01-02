#ifndef ODR_ZIP_STORAGE_H
#define ODR_ZIP_STORAGE_H

#include <common/storage.h>
#include <exception>

namespace odr::zip {
class ZipWriter;

class ZipReader final : public common::ReadStorage {
public:
  ZipReader(const void *, std::uint64_t size);
  ZipReader(const std::string &zip, bool dummy);
  explicit ZipReader(const common::Path &);
  ~ZipReader() final;

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

  friend ZipWriter;
};

class ZipWriter final : public common::WriteStorage {
public:
  explicit ZipWriter(const common::Path &);
  ~ZipWriter() final;

  bool isWriteable(const common::Path &) const final { return true; }

  bool remove(const common::Path &) const final { return false; }
  bool copy(const common::Path &, const common::Path &) const final {
    return false;
  }
  bool copy(const ZipReader &, const common::Path &) const;
  bool move(const common::Path &, const common::Path &) const final {
    return false;
  }

  bool createDirectory(const common::Path &) const final;

  std::unique_ptr<std::ostream> write(const common::Path &) const final;
  std::unique_ptr<std::ostream> write(const common::Path &,
                                      int compression) const;

private:
  class Impl;
  const std::unique_ptr<Impl> impl;
};

} // namespace odr::zip

#endif // ODR_ZIP_STORAGE_H
