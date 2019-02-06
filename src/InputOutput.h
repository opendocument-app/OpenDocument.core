#ifndef OPENDOCUMENT_INPUTOUTPUT_H
#define OPENDOCUMENT_INPUTOUTPUT_H

#include <cstdint>

namespace opendocument {

typedef char Byte;
typedef std::int64_t Size;

class Closeable {
public:
    virtual ~Closeable() = default;
    virtual void close() = 0;
};

class Source : public Closeable {
public:
    Source() = default;
    Source(const Source &) = delete;
    ~Source() override = default;
    Source &operator=(const Source &) = delete;
    virtual Size read(Byte *buffer, Size size) = 0;
    void close() override = 0;
};

class Sink : public Closeable {
public:
    Sink() = default;
    Sink(const Sink &) = delete;
    ~Sink() override = default;
    Sink &operator=(const Sink &) = delete;
    virtual void write(Byte *buffer, Size size) = 0;
    void close() override = 0;
};

}

#endif //OPENDOCUMENT_INPUTOUTPUT_H
