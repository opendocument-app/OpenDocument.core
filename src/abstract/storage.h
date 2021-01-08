#ifndef ODR_ABSTRACT_STORAGE_H
#define ODR_ABSTRACT_STORAGE_H

#include <exception>
#include <functional>
#include <iostream>
#include <memory>

namespace odr::common {
class Path;
}

namespace odr::abstract {

class ReadStorage {
public:
  typedef std::function<void(const common::Path &)> Visitor;

  virtual ~ReadStorage() = default;

  virtual bool isSomething(const common::Path &) const = 0;
  virtual bool isFile(const common::Path &) const = 0;
  virtual bool isDirectory(const common::Path &) const = 0;
  virtual bool isReadable(const common::Path &) const = 0;

  virtual std::uint64_t size(const common::Path &) const = 0;

  // TODO only list for subdir? harder in case of zip
  virtual void visit(Visitor) const = 0;

  virtual std::unique_ptr<std::istream> read(const common::Path &) const = 0;
};

class WriteStorage {
public:
  virtual ~WriteStorage() = default;

  virtual bool isWriteable(const common::Path &) const = 0;

  virtual bool remove(const common::Path &) const = 0;
  virtual bool copy(const common::Path &from, const common::Path &to) const = 0;
  virtual bool move(const common::Path &from, const common::Path &to) const = 0;

  virtual bool createDirectory(const common::Path &) const = 0;

  virtual std::unique_ptr<std::ostream> write(const common::Path &) const = 0;
};

class Storage : public ReadStorage, public WriteStorage {
public:
  ~Storage() override = default;

  bool isSomething(const common::Path &) const override = 0;
  bool isFile(const common::Path &) const override = 0;
  bool isDirectory(const common::Path &) const override = 0;
  bool isReadable(const common::Path &) const override = 0;
  bool isWriteable(const common::Path &) const override = 0;

  std::uint64_t size(const common::Path &) const override = 0;

  bool remove(const common::Path &) const override = 0;
  bool copy(const common::Path &from,
            const common::Path &to) const override = 0;
  bool move(const common::Path &from,
            const common::Path &to) const override = 0;

  bool createDirectory(const common::Path &) const override = 0;

  void visit(Visitor) const override = 0;

  std::unique_ptr<std::istream> read(const common::Path &) const override = 0;
  std::unique_ptr<std::ostream> write(const common::Path &) const override = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_STORAGE_H
