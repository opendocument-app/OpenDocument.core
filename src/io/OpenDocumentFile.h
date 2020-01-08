#ifndef ODR_OPENDOCUMENTFILE_H
#define ODR_OPENDOCUMENTFILE_H

#include <memory>
#include <string>
#include <exception>
#include "odr/FileMeta.h"
#include "Storage.h"

namespace odr {

class UnsupportedCryptoAlgorithmException : public std::exception {
public:
    UnsupportedCryptoAlgorithmException() : name() {};
    explicit UnsupportedCryptoAlgorithmException(const std::string &name) : name(name) {}
    const std::string &getName() const { return name; }
    const char *what() const noexcept override { return "unsupported crypto algorithm"; }
private:
    std::string name;
};

class WrongPasswordException : public std::exception {
public:
    const char *what() const noexcept override { return "wrong password"; }
};

class OpenDocumentFile final : public ReadStorage {
public:
    OpenDocumentFile(const Path &, const std::string &password);
    OpenDocumentFile(const Storage &, const Path &, const std::string &password);
    explicit OpenDocumentFile(std::unique_ptr<Storage>);

    const FileMeta &getMeta() const { return meta; }

    bool isSomething(const Path &p) const final { return parent->isSomething(p); }
    bool isFile(const Path &p) const final { return parent->isSomething(p); }
    bool isFolder(const Path &p) const final { return parent->isSomething(p); }
    bool isReadable(const Path &p) const final { return isFile(p); }

    std::uint64_t size(const Path &p) const final { return parent->size(p); }

    void visit(const Path &p, Visiter v) const final { parent->visit(p, v); }

    std::unique_ptr<Source> read(const Path &p) const final { return parent->read(p); }

private:
    std::unique_ptr<Storage> parent;
    FileMeta meta;

    void parseMeta();
};

}

#endif //ODR_OPENDOCUMENTFILE_H
