#ifndef ODR_SYSTEMSTORAGE_H
#define ODR_SYSTEMSTORAGE_H

#include "Storage.h"

namespace odr {

class SystemStorage : public Storage {
public:
    static const SystemStorage &instance();

    SystemStorage(const SystemStorage &) = delete;
    void operator=(const SystemStorage &) = delete;

    bool isSomething(const Path &) const final;
    bool isFile(const Path &) const final;
    bool isFolder(const Path &) const final;
    bool isReadable(const Path &) const final;
    bool isWriteable(const Path &) const final;

    std::uint64_t size(const Path &) const final;

    bool remove(const Path &) const final;
    bool copy(const Path &, const Path &) const final;
    bool move(const Path &, const Path &) const final;

    void visit(const Path &, Visitor) const final;

    std::unique_ptr<Source> read(const Path &) const final;
    std::unique_ptr<Sink> write(const Path &) const final;

private:
    SystemStorage() = default;
};

}

#endif //ODR_SYSTEMSTORAGE_H
