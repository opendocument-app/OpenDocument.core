#ifndef ODR_STORAGE_H
#define ODR_STORAGE_H

#include <memory>
#include <functional>
#include "Stream.h"

namespace odr {

class Path;
class Source;
class Sink;

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

}

#endif //ODR_STORAGE_H
