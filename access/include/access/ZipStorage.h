#ifndef ODR_ACCESS_ZIP_STORAGE_H
#define ODR_ACCESS_ZIP_STORAGE_H

#include <access/Storage.h>
#include <exception>

namespace odr {
namespace access {

class ZipWriter;

class NoZipFileException : public std::exception {
public:
  explicit NoZipFileException(std::string path) : path_(std::move(path)) {}
  const std::string &path() const { return path_; }
  const char *what() const noexcept override { return "not a zip file"; }

private:
  std::string path_;
};

class ZipReader final : public ReadStorage {
public:
  ZipReader(const void *, std::uint64_t size);
  ZipReader(const std::string &zip, bool dummy);
  explicit ZipReader(const Path &);
  ~ZipReader() final;

  bool isSomething(const Path &) const final;
  bool isFile(const Path &) const final;
  bool isDirectory(const Path &) const final;
  bool isReadable(const Path &) const final;

  std::uint64_t size(const Path &) const final;

  void visit(Visitor) const final;

  std::unique_ptr<std::istream> read(const Path &) const final;

private:
  class Impl;
  const std::unique_ptr<Impl> impl;

  friend ZipWriter;
};

class ZipWriter final : public WriteStorage {
public:
  explicit ZipWriter(const Path &);
  ~ZipWriter() final;

  bool isWriteable(const Path &) const final { return true; }

  bool remove(const Path &) const final { return false; }
  bool copy(const Path &, const Path &) const final { return false; }
  bool copy(const ZipReader &, const Path &) const;
  bool move(const Path &, const Path &) const final { return false; }

  bool createDirectory(const Path &) const final;

  std::unique_ptr<std::ostream> write(const Path &) const final;
  std::unique_ptr<std::ostream> write(const Path &, int compression) const;

private:
  class Impl;
  const std::unique_ptr<Impl> impl;
};

} // namespace access
} // namespace odr

#endif // ODR_ACCESS_ZIP_STORAGE_H
