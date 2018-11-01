#ifndef OPENDOCUMENT_ZIPFILE_H
#define OPENDOCUMENT_ZIPFILE_H

#include <string>
#include <memory>
#include "InputOutput.h"

namespace opendocument {

class ZipFile : public ReadableStorage {
public:
    // TODO: move to Storage; use class with getters and setters
    struct Entry {
        bool directory;
        std::string name;
        std::string comment;
        size_t size_compressed;
        size_t size_uncompressed;
    };

    static std::unique_ptr<ZipFile> open(const std::string &path);

    ~ZipFile() override = default;

    size_t size(const Path &path) const override = 0;
    Source &read(const Path &path) override = 0;
    void close() override = 0;

protected:
    class ConstIterator_ {
    public:
        virtual ~ConstIterator_() = default;
        virtual std::unique_ptr<ConstIterator_> copy() const = 0;
        virtual const Entry &operator*() const = 0;
        virtual void operator++() = 0;
        virtual bool operator!=(const ConstIterator_ &it) const = 0;
    };

    virtual std::unique_ptr<ConstIterator_> begin_() const = 0;
    virtual std::unique_ptr<ConstIterator_> end_() const = 0;

public:
    class ConstIterator {
    public:
        explicit ConstIterator(std::unique_ptr<ConstIterator_> it);
        ConstIterator(const ConstIterator &other);
        const Entry &operator*() const;
        ConstIterator &operator++();
        bool operator!=(const ConstIterator &it) const;
    private:
        const std::unique_ptr<ConstIterator_> _it;
    };

    ConstIterator begin() const;
    ConstIterator end() const;
};

}

#endif //OPENDOCUMENT_ZIPFILE_H
