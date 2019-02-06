#include "Storage.h"
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>

namespace opendocument {

class StorageStandardImpl final : public Storage {
public:
    explicit StorageStandardImpl(const std::string &);
    ~StorageStandardImpl() override = default;

    bool exists(const Path &) override;
    bool isFile(const Path &) override;
    bool isDirectory(const Path &) override;
    Size getSize(const Path &) override;
    std::unique_ptr<Source> read(const Path &) override;
    void close() override;

private:
    std::string getPath(const Path &) const;

    const std::string _root;
};

class SourceStandardImpl final : public Source {
public:
    explicit SourceStandardImpl(const std::string &);
    ~SourceStandardImpl() override = default;

    Size read(Byte *buffer, Size size) override;
    void close() override;

private:
    std::ifstream _is;
};

std::unique_ptr<Storage> Storage::standard(const std::string &root) {
    return std::make_unique<StorageStandardImpl>(root);
}

StorageStandardImpl::StorageStandardImpl(const std::string &root)
        : _root(root) {
    // TODO: throw if not directory
}

bool StorageStandardImpl::exists(const opendocument::Path &path) {
    std::string full = getPath(path);
    struct stat info;
    return stat(full.c_str(), &info) == 0;
}

bool StorageStandardImpl::isFile(const Path &path) {
    std::string full = getPath(path);
    struct stat info;
    if (stat(full.c_str(), &info) != 0) {
        return false;
    }
    return info.st_mode & S_IFREG;
}

bool StorageStandardImpl::isDirectory(const Path &path) {
    std::string full = getPath(path);
    struct stat info;
    if (stat(full.c_str(), &info) != 0) {
        return false;
    }
    return info.st_mode & S_IFDIR;
}

Size StorageStandardImpl::getSize(const Path &path) {
    std::string full = getPath(path);
    struct stat info;
    if (stat(full.c_str(), &info) != 0) {
        return -1;
    }
    return info.st_size;
}

std::unique_ptr<Source> StorageStandardImpl::read(const Path &path) {
    return std::make_unique<SourceStandardImpl>(path);
}

std::string StorageStandardImpl::getPath(const Path &path) const {
    return _root + "/" + path;
}

void StorageStandardImpl::close() {
}

SourceStandardImpl::SourceStandardImpl(const std::string &path) {
    _is.open(path, std::ios::in | std::ios::binary);
}

Size SourceStandardImpl::read(Byte *buffer, Size size) {
    auto pos = _is.tellg();
    _is.read(buffer, size);
    return _is.tellg() - pos;
}

void SourceStandardImpl::close() {
    _is.close();
}

}
