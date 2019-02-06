#ifndef OPENDOCUMENT_ZIPFILE_H
#define OPENDOCUMENT_ZIPFILE_H

#include <string>
#include <memory>
#include "Storage.h"

namespace opendocument {

class ZipFile : public Storage {
public:
    struct Entry {
        bool directory;
        std::string name;
        std::string comment;
        size_t size_compressed;
        size_t size_uncompressed;
    };

    static std::unique_ptr<ZipFile> open(const std::string &path);

    ~ZipFile() override = default;

    bool exists(const Path &) override = 0;
    bool isFile(const Path &) override = 0;
    bool isDirectory(const Path &) override = 0;
    Size getSize(const Path &) override = 0;
    std::unique_ptr<Source> read(const Path &) override = 0;
    void close() override = 0;
};

}

#endif //OPENDOCUMENT_ZIPFILE_H
