#include "InputOutput.h"

namespace opendocument {

void Streams::write(Source &source, Sink &sink, uint8_t *buffer, std::size_t size) {
    while (true) {
        std::size_t read = source.read(buffer, size);
        if (read <= 0) {
            break;
        }
        sink.write(buffer, read);
    }
}

class StorageStandardImpl final : public Storage {
public:
    explicit StorageStandardImpl(const std::string &root);
    ~StorageStandardImpl() override = default;

    size_t size(const Path &path) const override;
    Source &read(const Path &path) override;
    Sink &write(const Path &path) override;
    void remove(const Path &path) override;

    void close() override;
private:
    const std::string _root;
};

StorageStandardImpl::StorageStandardImpl(const std::string &root)
        : _root(root) {
    // TODO: trow if not a directory
}

size_t StorageStandardImpl::size(const Path &path) const {
    // TODO
}

Source& StorageStandardImpl::read(const Path &path) {
    // TODO
}

Sink& StorageStandardImpl::write(const Path &path) {
    // TODO
}

void StorageStandardImpl::remove(const Path &path) {
    // TODO
}

void StorageStandardImpl::close() {
    // TODO
}

std::unique_ptr<Storage> Storage::standard(const std::string &root) {
    return std::make_unique<StorageStandardImpl>(root);
}

}
