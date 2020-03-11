#ifndef ODR_STORAGE_H
#define ODR_STORAGE_H

#include <access/Path.h>
#include <access/Stream.h>
#include <exception>
#include <functional>
#include <memory>

namespace odr {

class FileNotFoundException : public std::exception {
public:
  explicit FileNotFoundException(std::string path) : path(std::move(path)) {}
  const std::string &getPath() const { return path; }
  const char *what() const noexcept override { return "file not found"; }

private:
  std::string path;
};

class FileNotCreatedException : public std::exception {
public:
  explicit FileNotCreatedException(std::string path) : path(std::move(path)) {}
  const std::string &getPath() const { return path; }
  const char *what() const noexcept override { return "file not created"; }

private:
  std::string path;
};

class Storage {
public:
  typedef std::function<void(const Path &)> Visitor;

  virtual ~Storage() = default;

  virtual bool isSomething(const Path &) const = 0;
  virtual bool isFile(const Path &) const = 0;
  virtual bool isFolder(const Path &) const = 0;
  virtual bool isReadable(const Path &) const = 0;
  virtual bool isWriteable(const Path &) const = 0;

  virtual std::uint64_t size(const Path &) const = 0;

  virtual bool remove(const Path &) const = 0;
  virtual bool copy(const Path &from, const Path &to) const = 0;
  virtual bool move(const Path &from, const Path &to) const = 0;

  // TODO only list for subdir? harder in case of zip
  virtual void visit(Visitor) const = 0;

  virtual std::unique_ptr<Source> read(const Path &) const = 0;
  virtual std::unique_ptr<Sink> write(const Path &) const = 0;
};

class ReadStorage : public Storage {
public:
  ~ReadStorage() override = default;

  bool isWriteable(const Path &) const final { return false; }

  bool remove(const Path &) const final { return false; }
  bool copy(const Path &, const Path &) const final { return false; }
  bool move(const Path &, const Path &) const final { return false; }

  std::unique_ptr<Sink> write(const Path &) const final { return nullptr; }
};

class WriteStorage : public Storage {
public:
  ~WriteStorage() override = default;

  bool isSomething(const Path &) const final { return false; }
  bool isFile(const Path &) const final { return false; }
  bool isFolder(const Path &) const final { return false; }
  bool isReadable(const Path &) const final { return false; }

  void visit(Visitor) const final {}

  std::unique_ptr<Source> read(const Path &) const final { return nullptr; }
};

} // namespace odr

#endif // ODR_STORAGE_H
