#ifndef OPENDOCUMENT_STORAGE_H
#define OPENDOCUMENT_STORAGE_H

#include "InputOutput.h"
#include <string>
#include <memory>
#include <unordered_set>

namespace opendocument {

typedef std::string Path;

class Storage : public Closeable {
public:
    struct Entry {
        enum class Type {
            FILE, DIRECTORY
        };

        Path path;
        Type type;
        Size size;
    };

    static std::unique_ptr<Storage> standard(const std::string &root);

    Storage() = default;
    Storage(const Storage &) = delete;
    ~Storage() override = default;
    Storage &operator=(const Storage &) = delete;

    virtual bool exists(const Path &) = 0;
    virtual const Entry *getEntry(const Path &) = 0;
    virtual std::unique_ptr<Source> read(const Path &) = 0;
    void close() override = 0;
};

}

#endif //OPENDOCUMENT_STORAGE_H
