#include "OpenDocumentFile.h"
#include <fstream>

namespace opendocument {

OpenDocumentFile::OpenDocumentFile(ReadableStorage &access)
    : _access(access) {
    createMeta();
}

OpenDocumentFile::~OpenDocumentFile() {
}

void OpenDocumentFile::createMeta() {
    // TODO
    _meta.text.pageCount = 0;
    _meta.spreadsheet.tableCount = 0;
    _meta.spreadsheet.tables = new Meta::Spreadsheet::Table[_meta.spreadsheet.tableCount];
    _meta.presentation.pageCount = 0;
}

void OpenDocumentFile::destroyMeta() {
    delete[] _meta.spreadsheet.tables;
}

const OpenDocumentFile::Meta &OpenDocumentFile::meta() const {
    return _meta;
}

size_t OpenDocumentFile::size(const Path &path) const {
    return _access.size(path);
}

Source& OpenDocumentFile::read(const Path &path) {
    return _access.read(path);
}

void OpenDocumentFile::loadXML(const Path &entry) const {
    // TODO: use read
}

void OpenDocumentFile::close() {
    // TODO
}

}
