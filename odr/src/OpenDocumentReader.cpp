#include <access/CfbStorage.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Constants.h>
#include <common/TranslationContext.h>
#include <memory>
#include <odf/OpenDocumentCrypto.h>
#include <odf/OpenDocumentMeta.h>
#include <odf/OpenDocumentTranslator.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/OpenDocumentReader.h>
#include <ooxml/OfficeOpenXmlCrypto.h>
#include <ooxml/OfficeOpenXmlMeta.h>
#include <ooxml/OfficeOpenXmlTranslator.h>

namespace odr {

class OpenDocumentReader::Impl final {
private:
  bool opened = false;
  bool decrypted = false;
  std::unique_ptr<access::Storage> storage;
  FileMeta meta;

  Config config;
  common::TranslationContext context;

  odf::OpenDocumentTranslator translatorOd;
  ooxml::OfficeOpenXmlTranslator translatorMs;

  bool open_(const std::string &path) {
    try {
      storage = std::make_unique<access::ZipReader>(path);

      try {
        meta = odf::OpenDocumentMeta::parseFileMeta(*storage, false);
        return true;
      } catch (odf::NoOpenDocumentFileException &) {
      }
      try {
        meta = ooxml::OfficeOpenXmlMeta::parseFileMeta(*storage);
        return true;
      } catch (ooxml::NoOfficeOpenXmlFileException &) {
      }
    } catch (access::NoZipFileException &) {
    }
    try {
      storage = std::make_unique<access::CfbReader>(path);

      // TODO legacy microsoft documents

      {
        meta = {};
        meta.type = FileType::COMPOUND_FILE_BINARY_FORMAT;
        meta.encrypted = storage->isFile("EncryptionInfo") &&
                         storage->isFile("EncryptedPackage");
        return true;
      }
    } catch (access::NoCfbFileException &) {
    }

    // file detection failed
    return false;
  }

  bool decrypt_(const std::string &password) {
    switch (meta.type) {
    case FileType::OPENDOCUMENT_TEXT:
    case FileType::OPENDOCUMENT_PRESENTATION:
    case FileType::OPENDOCUMENT_SPREADSHEET:
    case FileType::OPENDOCUMENT_GRAPHICS: {
      const auto manifest = odf::OpenDocumentMeta::parseManifest(*storage);
      if (!odf::OpenDocumentCrypto::decrypt(storage, manifest, password))
        return false;
      meta = odf::OpenDocumentMeta::parseFileMeta(*storage, true);
      return true;
    }
    case FileType::COMPOUND_FILE_BINARY_FORMAT: {
      // office open xml decryption
      {
        const std::string encryptionInfo =
            access::StreamUtil::read(*storage->read("EncryptionInfo"));
        ooxml::OfficeOpenXmlCrypto::Util util(encryptionInfo);
        const std::string key = util.deriveKey(password);
        if (!util.verify(key))
          return false;
        const std::string encryptedPackage =
            access::StreamUtil::read(*storage->read("EncryptedPackage"));
        const std::string decryptedPackage =
            util.decrypt(encryptedPackage, key);
        storage = std::make_unique<access::ZipReader>(decryptedPackage, false);
        meta = ooxml::OfficeOpenXmlMeta::parseFileMeta(*storage);
        return true;
      }

      // TODO legacy microsoft decryption

      return false;
    }
    default:
      return false;
    }
  }

public:
  FileType guess(const std::string &path) {
    if (!open(path))
      return FileType::UNKNOWN;
    const FileType result = meta.type;
    close();
    return result;
  }

  bool open(const std::string &path) {
    if (opened)
      close();
    opened = open_(path);
    decrypted = !meta.encrypted;
    return opened;
  }

  bool save(const std::string &path) { return false; }

  void close() noexcept {
    if (!opened)
      return;
    opened = false;
    storage.reset();
    meta = {};
    context = {};
  }

  bool canTranslate() const noexcept {
    if (!opened)
      return false;
    if (!decrypted)
      return false;
    switch (meta.type) {
    case FileType::OPENDOCUMENT_TEXT:
    case FileType::OPENDOCUMENT_PRESENTATION:
    case FileType::OPENDOCUMENT_SPREADSHEET:
    case FileType::OPENDOCUMENT_GRAPHICS:
    case FileType::OFFICE_OPEN_XML_DOCUMENT:
    case FileType::OFFICE_OPEN_XML_PRESENTATION:
    case FileType::OFFICE_OPEN_XML_WORKBOOK:
      return true;
    default:
      return false;
    }
  }

  bool canBackTranslate() const noexcept {
    if (!opened)
      return false;
    if (!decrypted)
      return false;
    if (!context.config->editable)
      return false;
    switch (meta.type) {
    case FileType::OPENDOCUMENT_TEXT:
    case FileType::OPENDOCUMENT_PRESENTATION:
    case FileType::OPENDOCUMENT_SPREADSHEET:
    case FileType::OPENDOCUMENT_GRAPHICS:
      return !meta.encrypted;
    default:
      return false;
    }
  }

  const FileMeta &getMeta() const noexcept { return meta; }

  bool decrypt(const std::string &password) {
    if (!opened)
      return false;
    if (decrypted)
      return true;
    decrypted = decrypt_(password);
    return decrypted;
  }

  bool translate(const std::string &path, const Config &c) {
    if (!canTranslate())
      return false;

    config = c;
    context = {};
    context.config = &config;
    context.meta = &meta;
    context.storage = storage.get();

    switch (meta.type) {
    case FileType::OPENDOCUMENT_TEXT:
    case FileType::OPENDOCUMENT_PRESENTATION:
    case FileType::OPENDOCUMENT_SPREADSHEET:
    case FileType::OPENDOCUMENT_GRAPHICS:
      // TODO: optimize; dont reload xml, dont regenerate styles, ... for same
      // input file
      return translatorOd.translate(path, context);
    case FileType::OFFICE_OPEN_XML_DOCUMENT:
    case FileType::OFFICE_OPEN_XML_PRESENTATION:
    case FileType::OFFICE_OPEN_XML_WORKBOOK:
      // TODO: optimize; dont reload xml, dont regenerate styles, ... for same
      // input file
      return translatorMs.translate(path, context);
    default:
      return false;
    }
  }

  bool backTranslate(const std::string &diff, const std::string &path) {
    if (!canBackTranslate())
      return false;

    switch (meta.type) {
    case FileType::OPENDOCUMENT_TEXT:
    case FileType::OPENDOCUMENT_PRESENTATION:
    case FileType::OPENDOCUMENT_SPREADSHEET:
    case FileType::OPENDOCUMENT_GRAPHICS:
      return translatorOd.backTranslate(diff, path, context);
    default:
      return false;
    }
  }
};

std::string OpenDocumentReader::getVersion() {
  return common::Constants::getVersion();
}

std::string OpenDocumentReader::getCommit() {
  return common::Constants::getCommit();
}

OpenDocumentReader::OpenDocumentReader() : impl_(std::make_unique<Impl>()) {}

OpenDocumentReader::~OpenDocumentReader() = default;

FileType OpenDocumentReader::guess(const std::string &path) const noexcept {
  try {
    return impl_->guess(path);
  } catch (...) {
    // TODO log
    return FileType::UNKNOWN;
  }
}

bool OpenDocumentReader::open(const std::string &path) noexcept {
  try {
    return impl_->open(path);
  } catch (...) {
    // TODO log
    return false;
  }
}

bool OpenDocumentReader::save(const std::string &path) noexcept {
  try {
    return impl_->save(path);
  } catch (...) {
    // TODO log
  }
}

void OpenDocumentReader::close() noexcept {
  try {
    impl_->close();
  } catch (...) {
    // TODO log
  }
}

bool OpenDocumentReader::canTranslate() const noexcept {
  try {
    return impl_->canTranslate();
  } catch (...) {
    // TODO log
    return false;
  }
}

bool OpenDocumentReader::canBackTranslate() const noexcept {
  try {
    return impl_->canBackTranslate();
  } catch (...) {
    // TODO log
    return false;
  }
}

const FileMeta &OpenDocumentReader::getMeta() const noexcept {
  try {
    return impl_->getMeta();
  } catch (...) {
    // TODO log
    return {};
  }
}

bool OpenDocumentReader::decrypt(const std::string &password) noexcept {
  try {
    return impl_->decrypt(password);
  } catch (...) {
    // TODO log
    return false;
  }
}

bool OpenDocumentReader::translate(const std::string &outPath,
                                   const Config &config) noexcept {
  try {
    return impl_->translate(outPath, config);
  } catch (...) {
    // TODO log
    return false;
  }
}

bool OpenDocumentReader::backTranslate(const std::string &diff,
                                       const std::string &path) noexcept {
  try {
    return impl_->backTranslate(diff, path);
  } catch (...) {
    // TODO log
    return false;
  }
}

} // namespace odr
