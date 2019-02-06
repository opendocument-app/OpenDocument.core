#include "OpenDocumentFile.h"
#include <fstream>

namespace opendocument {

OpenDocumentFile::OpenDocumentFile(Storage &access)
    : _access(access) {
    createMeta();
}

OpenDocumentFile::~OpenDocumentFile() {
    destroyMeta();
}

void OpenDocumentFile::createMeta() {
    // TODO: implement
    _meta.text.pageCount = 0;
    _meta.spreadsheet.tableCount = 0;
    _meta.spreadsheet.tables = new Meta::Spreadsheet::Table[_meta.spreadsheet.tableCount];
    _meta.presentation.pageCount = 0;
}

void OpenDocumentFile::destroyMeta() {
    delete[] _meta.spreadsheet.tables;
}

bool OpenDocumentFile::exists(const Path &path) {
    // TODO: implement
    return false;
}

bool OpenDocumentFile::isFile(const Path &path) {
    // TODO: implement
    return false;
}

bool OpenDocumentFile::isDirectory(const Path &path) {
    // TODO: implement
    return false;
}

Size OpenDocumentFile::getSize(const Path &path) {
    return _access.getSize(path);
}

const OpenDocumentFile::Meta &OpenDocumentFile::getMeta() const {
    return _meta;
}

std::unique_ptr<Source> OpenDocumentFile::read(const Path &path) {
    return _access.read(path);
}

void OpenDocumentFile::loadXML(const Path &entry) const {
    // TODO: use read
}

void OpenDocumentFile::close() {
    // TODO
}

}
