#ifndef ODR_OPENDOCUMENTFILE_H
#define ODR_OPENDOCUMENTFILE_H

#include <memory>
#include <string>
#include <exception>

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

struct FileMeta;
class Path;
class ZipReader;

class FileMetaNotFoundException : public std::exception {
public:
    explicit FileMetaNotFoundException(const std::string &path) : path(path) {}
    const std::string &getPath() const { return path; }
    const char *what() const noexcept override { return "file meta not found"; }
private:
    std::string path;
};

class UnsupportedCryptoAlgorithmException : public std::exception {
public:
    explicit UnsupportedCryptoAlgorithmException(const std::string &name) : name(name) {}
    const std::string &getName() const { return name; }
    const char *what() const noexcept override { return "unsupported crypto algorithm"; }
private:
    std::string name;
};

class NoXmlFileException : public std::exception {
public:
    explicit NoXmlFileException(const std::string &path) : path(path) {}
    const std::string &getPath() const { return path; }
    const char *what() const noexcept override { return "no xml file"; }
private:
    std::string path;
};

class NotDecryptedException : public std::exception {
public:
    const char *what() const noexcept override { return "not decrypted"; }
};

class CannotDecryptException : public std::exception {
public:
    const char *what() const noexcept override { return "cannot decrypt"; }
};

class DecryptionFailedException : public std::exception {
public:
    const char *what() const noexcept override { return "decryption failed"; }
};

// TODO storage
class OpenDocumentFile final {
public:
    OpenDocumentFile();
    ~OpenDocumentFile();

    bool open(const Path &);
    bool decrypt(const std::string &);
    void close();

    bool isOpen() const;
    bool isDecrypted() const;
    const FileMeta &getMeta() const;
    bool isFile(const Path &) const;

    std::string loadEntry(const Path &);
    std::unique_ptr<tinyxml2::XMLDocument> loadXml(const Path &);

    // TODO hacky
    const ZipReader &getZipReader() const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

}

#endif //ODR_OPENDOCUMENTFILE_H
