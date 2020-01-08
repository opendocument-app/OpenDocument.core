#ifndef ODR_MICROSOFTOPENXMLFILE_H
#define ODR_MICROSOFTOPENXMLFILE_H

#include <memory>
#include <string>
#include <exception>
#include "odr/FileMeta.h"
#include "Storage.h"

namespace odr {

class MicrosoftOpenXmlFile final : public ReadStorage {
public:
    MicrosoftOpenXmlFile(const Path &, const std::string &password);
    MicrosoftOpenXmlFile(const Storage &, const Path &, const std::string &password);
    explicit MicrosoftOpenXmlFile(std::unique_ptr<Storage>);

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

    void generateMeta();
};

}

#endif //ODR_MICROSOFTOPENXMLFILE_H
