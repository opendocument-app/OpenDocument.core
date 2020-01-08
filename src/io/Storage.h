#ifndef ODR_STORAGE_H
#define ODR_STORAGE_H

#include <memory>
#include <functional>
#include <exception>
#include "Path.h"
#include "Stream.h"

namespace odr {

class FileNotFoundException : public std::exception {
public:
    explicit FileNotFoundException(const std::string &path) : path(path) {}
    const std::string &getPath() const { return path; }
    const char *what() const noexcept override { return "file not found"; }
private:
    std::string path;
};

class FileNotCreatedException : public std::exception {
public:
    explicit FileNotCreatedException(const std::string &path) : path(path) {}
    const std::string &getPath() const { return path; }
    const char *what() const noexcept override { return "file not created"; }
private:
    std::string path;
};

class Storage {
public:
    typedef std::function<void(const Path &)> Visiter;

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

    virtual void visit(const Path &, Visiter) const = 0;

    virtual std::unique_ptr<Source> read(const Path &) const = 0;
    virtual std::unique_ptr<Sink> write(const Path &) const = 0;
};

class ReadStorage : public Storage {
public:
    ~ReadStorage() override = default;

    bool isWriteable(const Path &) const final { return false; }

    bool remove(const Path &) const final { return false; }
    bool copy(const Path &from, const Path &to) const final { return false; }
    bool move(const Path &from, const Path &to) const final { return false; }

    std::unique_ptr<Sink> write(const Path &) const final { return nullptr; }
};

class WriteStorage : public Storage {
public:
    ~WriteStorage() override = default;

    bool isSomething(const Path &) const final { return false; }
    bool isFile(const Path &) const final { return false; }
    bool isFolder(const Path &) const final { return false; }
    bool isReadable(const Path &) const final { return false; }

    void visit(const Path &, Visiter) const final {}

    std::unique_ptr<Source> read(const Path &) const final { return nullptr; }
};

}

#endif //ODR_STORAGE_H
