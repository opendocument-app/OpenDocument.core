#ifndef ODR_ZIPSTORAGE_H
#define ODR_ZIPSTORAGE_H

#include <access/Storage.h>
#include <exception>

namespace odr {

class ZipWriter;

class NoZipFileException : public std::exception {
public:
  explicit NoZipFileException(std::string path) : path(std::move(path)) {}
  const std::string &getPath() const { return path; }
  const char *what() const noexcept override { return "not a zip file"; }

private:
  std::string path;
};

class ZipReader final : public ReadStorage {
public:
  ZipReader(const void *, std::uint64_t size);
  ZipReader(const std::string &zip, bool dummy);
  explicit ZipReader(const Path &);
  ~ZipReader() final;

  bool isSomething(const Path &) const final;
  bool isFile(const Path &) const final;
  bool isFolder(const Path &) const final;
  bool isReadable(const Path &) const final;

  std::uint64_t size(const Path &) const final;

  void visit(Visitor) const final;

  std::unique_ptr<Source> read(const Path &) const final;

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

  std::uint64_t size(const Path &) const final { return 0; }

  bool remove(const Path &) const final { return false; }
  bool copy(const Path &, const Path &) const final { return false; }
  bool copy(const ZipReader &, const Path &) const;
  bool move(const Path &, const Path &) const final { return false; }

  std::unique_ptr<Sink> write(const Path &) const final;

private:
  class Impl;
  const std::unique_ptr<Impl> impl;
};

} // namespace odr

#endif // ODR_ZIPSTORAGE_H
