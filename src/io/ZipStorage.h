#ifndef ODR_ZIPSTORAGE_H
#define ODR_ZIPSTORAGE_H

#include "Storage.h"

namespace odr {

class ZipWriter;

class ZipReader final : public Storage {
public:
    explicit ZipReader(const Path &);
    ~ZipReader() final;

    bool isSomething(const Path &) const final;
    bool isFile(const Path &) const final;
    bool isFolder(const Path &) const final;
    bool isReadable(const Path &) const final;
    bool isWriteable(const Path &) const final { return false; }

    std::uint64_t size(const Path &) const final;

    bool remove(const Path &) const final { return false; }
    bool copy(const Path &, const Path &) const final { return false; }
    bool move(const Path &, const Path &) const final { return false; }

    void visit(Visiter) const;
    void visit(const Path &, Visiter) const final;

    std::unique_ptr<Source> read(const Path &) const final;
    std::unique_ptr<Sink> write(const Path &) const final { return nullptr; }

private:
    class Impl;
    const std::unique_ptr<Impl> impl;

    friend ZipWriter;
};

class ZipWriter final : public Storage {
public:
    explicit ZipWriter(const Path &);
    ~ZipWriter() final;

    bool isSomething(const Path &) const final { return false; }
    bool isFile(const Path &) const final { return false; }
    bool isFolder(const Path &) const final { return false; }
    bool isReadable(const Path &) const final { return false; }
    bool isWriteable(const Path &) const final { return true; }

    std::uint64_t size(const Path &) const final { return 0; }

    bool remove(const Path &) const final { return false; }
    bool copy(const Path &, const Path &) const final { return false; }
    bool copy(const ZipReader &, const Path &) const;
    bool move(const Path &, const Path &) const final { return false; }

    void visit(const Path &, Visiter) const final {}

    std::unique_ptr<Source> read(const Path &) const final { return nullptr; }
    std::unique_ptr<Sink> write(const Path &) const final;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_ZIPSTORAGE_H
