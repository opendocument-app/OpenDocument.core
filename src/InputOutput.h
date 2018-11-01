#ifndef OPENDOCUMENT_INPUTOUTPUT_H
#define OPENDOCUMENT_INPUTOUTPUT_H

#include <cstdint>
#include <string>
#include <memory>

namespace opendocument {

typedef std::uint8_t Byte;
typedef std::size_t Size;

class Closeable {
public:
    virtual ~Closeable() = default;
    virtual void close() = 0;
};

class Source : public Closeable {
public:
    ~Source() override = default;
    virtual Size read(Byte *buffer, Size size) const = 0;
    // TODO: eof?
    void close() override = 0;
};

class Sink : public Closeable {
public:
    ~Sink() override = default;
    virtual void write(Byte *buffer, Size size) const = 0;
    void close() override = 0;
};

typedef std::string Path;

// TODO: directories?

class ReadableStorage : public Closeable {
public:
    ~ReadableStorage() override = default;
    // TODO: exists
    virtual Size size(const Path &path) const = 0;
    virtual Source &read(const Path &path) = 0;
    void close() override = 0;
};

class WriteableStorage : public Closeable {
public:
    ~WriteableStorage() override = default;
    virtual Sink &write(const Path &path) = 0;
    // TODO: rename
    virtual void remove(const Path &path) = 0;
    void close() override = 0;
};

class Storage : public ReadableStorage, public WriteableStorage {
public:
    static std::unique_ptr<Storage> standard(const std::string &root);

    ~Storage() override = default;
    Size size(const Path &path) const override = 0;
    Source &read(const Path &path) override = 0;
    Sink &write(const Path &path) override = 0;
    void remove(const Path &path) override = 0;
    void close() override = 0;
};

namespace Streams {

extern void write(Source &source, Sink &sink, uint8_t *buffer, Size size);

}

}

#endif //OPENDOCUMENT_INPUTOUTPUT_H
