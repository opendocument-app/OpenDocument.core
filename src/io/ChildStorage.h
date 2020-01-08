#ifndef ODR_CHILDSTORAGE_H
#define ODR_CHILDSTORAGE_H

#include "Storage.h"
#include "Path.h"

namespace odr {

class ChildStorage final : public Storage {
public:
    ChildStorage(const Storage &parent, Path prefix);

    bool isSomething(const Path &) const final;
    bool isFile(const Path &) const final;
    bool isFolder(const Path &) const final;
    bool isReadable(const Path &) const final;
    bool isWriteable(const Path &) const final;

    std::uint64_t size(const Path &) const final;

    bool remove(const Path &) const final;
    bool copy(const Path &, const Path &) const final;
    bool move(const Path &, const Path &) const final;

    void visit(const Path &, Visiter) const final;

    std::unique_ptr<Source> read(const Path &) const final;
    std::unique_ptr<Sink> write(const Path &) const final;

private:
    const Storage &parent;
    const Path prefix;
};

}

#endif //ODR_CHILDSTORAGE_H
