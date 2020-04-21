#include <access/CfbStorage.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Constants.h>
#include <crypto/CfbCrypto.h>
#include <glog/logging.h>
#include <memory>
#include <odf/OpenDocument.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/Reader.h>
#include <ooxml/OfficeOpenXml.h>

namespace odr {

// TODO throw on invalid state (like not open)
class Reader::Impl final {
public:
  ~Impl() { close(); }

  bool open(const std::string &path) {
    if (opened_)
      close();
    opened_ = open_(path);
    decrypted_ = !meta_.encrypted;
    return opened_;
  }

  bool open(const std::string &path, const FileType as) {
    if (opened_)
      close();
    opened_ = open_(path, as);
    decrypted_ = !meta_.encrypted;
    return opened_;
  }

  bool save(const std::string &path) {
    if (!opened_)
      return false;
    if (translatorOd_)
      return translatorOd_->save(path);
    if (translatorMs_)
      return translatorMs_->save(path);
    return false;
  }

  bool save(const std::string &path, const std::string &password) {
    if (!opened_)
      return false;
    if (translatorOd_)
      return translatorOd_->save(path, password);
    if (translatorMs_)
      return translatorMs_->save(path, password);
    return false;
  }

  void close() noexcept {
    if (!opened_)
      return;
    opened_ = false;
    decrypted_ = false;
    storage_.reset();
    meta_ = {};
    translatorOd_ = nullptr;
    translatorMs_ = nullptr;
  }

  bool isOpen() const noexcept { return opened_; }

  bool isDecrypted() const noexcept { return decrypted_; }

  bool canTranslate() const noexcept {
    if (!opened_)
      return false;
    if (translatorOd_)
      return translatorOd_->canHtml();
    if (translatorMs_)
      return translatorMs_->canHtml();
    return false;
  }

  bool canEdit() const noexcept {
    if (!opened_)
      return false;
    if (translatorOd_)
      return translatorOd_->canEdit();
    if (translatorMs_)
      return translatorMs_->canEdit();
    return false;
  }

  bool canSave(const bool encrypted) const noexcept {
    if (!opened_)
      return false;
    if (translatorOd_)
      return translatorOd_->canSave(encrypted);
    if (translatorMs_)
      return translatorMs_->canSave(encrypted);
    return false;
  }

  bool isEncrypted() const noexcept {
    if (!opened_)
      return false;
    return meta_.encrypted;
  }

  FileType type() const noexcept {
    if (!opened_)
      return FileType::UNKNOWN;
    return meta_.type;
  }

  const FileMeta &meta() const noexcept {
    if (!opened_)
      return {};
    return meta_;
  }

  bool decrypt(const std::string &password) {
    if (!opened_)
      return false;
    if (decrypted_)
      return true;
    decrypted_ = decrypt_(password);
    return decrypted_;
  }

  bool translate(const std::string &path, const Config &config) {
    if (!canTranslate())
      return false;
    if (translatorOd_)
      return translatorOd_->html(path, config);
    if (translatorMs_)
      return translatorMs_->html(path, config);
    return false;
  }

  bool edit(const std::string &diff) {
    if (!canEdit())
      return false;
    if (translatorOd_)
      return translatorOd_->edit(diff);
    if (translatorMs_)
      return translatorMs_->edit(diff);
    return false;
  }

private:
  bool opened_{false};
  bool decrypted_{false};
  std::unique_ptr<access::Storage> storage_;
  FileMeta meta_;

  std::unique_ptr<odf::OpenDocument> translatorOd_;
  std::unique_ptr<ooxml::OfficeOpenXml> translatorMs_;

  bool open_(const std::string &path) {
    try {
      storage_ = std::make_unique<access::ZipReader>(path);

      try {
        translatorOd_ = std::make_unique<odf::OpenDocument>(storage_);
        meta_ = translatorOd_->getMeta();
        return true;
      } catch (...) {
      }
      try {
        translatorMs_ = std::make_unique<ooxml::OfficeOpenXml>(storage_);
        meta_ = translatorMs_->getMeta();
        return true;
      } catch (...) {
      }
    } catch (access::NoZipFileException &) {
    }
    try {
      storage_ = std::make_unique<access::CfbReader>(path);

      // MS-DOC: The "WordDocument" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd926131(v=office.12).aspx
      if (storage_->isFile("WordDocument")) {
        meta_ = {};
        meta_.type = FileType::LEGACY_WORD_DOCUMENT;
        return true;
      }
      // MS-PPT: The "PowerPoint Document" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd911009(v=office.12).aspx
      if (storage_->isFile("PowerPoint Document")) {
        meta_ = {};
        meta_.type = FileType::LEGACY_POWERPOINT_PRESENTATION;
        return true;
      }
      // MS-XLS: The "Workbook" stream MUST be present in the file.
      // https://docs.microsoft.com/en-us/openspecs/office_file_formats/ms-ppt/1fc22d56-28f9-4818-bd45-67c2bf721ccf
      if (storage_->isFile("Workbook")) {
        meta_ = {};
        meta_.type = FileType::LEGACY_EXCEL_WORKSHEETS;
        return true;
      }

      {
        meta_ = {};
        // TODO dedicated OOXML file type?
        meta_.type = FileType::COMPOUND_FILE_BINARY_FORMAT;
        // TODO out-source
        meta_.encrypted = storage_->isFile("EncryptionInfo") &&
                          storage_->isFile("EncryptedPackage");
        return true;
      }
    } catch (access::NoCfbFileException &) {
    }

    // file detection failed
    return false;
  }

  bool open_(const std::string &path, const FileType as) {
    // TODO implement
    return false;
  }

  bool decrypt_(const std::string &password) {
    if (translatorOd_) {
      if (!translatorOd_->decrypt(password))
        return false;
      meta_ = translatorOd_->getMeta();
      return true;
    }
    if (meta_.type == FileType::COMPOUND_FILE_BINARY_FORMAT) {
      // office open xml decryption
      const std::string encryptionInfo =
          access::StreamUtil::read(*storage_->read("EncryptionInfo"));
      crypto::CfbCrypto::Util util(encryptionInfo);
      const std::string key = util.deriveKey(password);
      if (!util.verify(key))
        return false;
      const std::string encryptedPackage =
          access::StreamUtil::read(*storage_->read("EncryptedPackage"));
      const std::string decryptedPackage = util.decrypt(encryptedPackage, key);
      storage_ = std::make_unique<access::ZipReader>(decryptedPackage, false);
      translatorMs_ = std::make_unique<ooxml::OfficeOpenXml>(storage_);
      meta_ = translatorMs_->getMeta();
      return true;
    }
    return false;
  }
};

std::string Reader::version() noexcept { return common::Constants::version(); }

std::string Reader::commit() noexcept { return common::Constants::commit(); }

FileType Reader::guessType(const std::string &path) noexcept {
  try {
    Reader odr;
    if (!odr.open(path))
      return FileType::UNKNOWN;
    return odr.type();
  } catch (...) {
    LOG(ERROR) << "guess failed";
    return FileType::UNKNOWN;
  }
}

FileMeta Reader::fetchMeta(const std::string &path) noexcept {
  try {
    Reader odr;
    if (!odr.open(path))
      return {};
    return odr.meta();
  } catch (...) {
    LOG(ERROR) << "guess failed";
    return {};
  }
}

Reader::Reader() : impl_(std::make_unique<Impl>()) {}

Reader::~Reader() = default;

bool Reader::open(const std::string &path) const noexcept {
  try {
    return impl_->open(path);
  } catch (...) {
    LOG(ERROR) << "open failed";
    return false;
  }
}

bool Reader::open(const std::string &path, const FileType as) const noexcept {
  try {
    return impl_->open(path, as);
  } catch (...) {
    LOG(ERROR) << "openAs failed";
    return false;
  }
}

bool Reader::save(const std::string &path) const noexcept {
  try {
    return impl_->save(path);
  } catch (...) {
    LOG(ERROR) << "save failed";
    return false;
  }
}

bool Reader::save(const std::string &path,
                  const std::string &password) const noexcept {
  try {
    return impl_->save(path, password);
  } catch (...) {
    LOG(ERROR) << "saveEncrypted failed";
    return false;
  }
}

void Reader::close() const noexcept {
  try {
    impl_->close();
  } catch (...) {
    LOG(ERROR) << "close failed";
  }
}

bool Reader::isOpen() const noexcept {
  try {
    return impl_->isOpen();
  } catch (...) {
    LOG(ERROR) << "isOpen failed";
    return false;
  }
}

bool Reader::isDecrypted() const noexcept {
  try {
    return impl_->isDecrypted();
  } catch (...) {
    LOG(ERROR) << "isDecrypted failed";
    return false;
  }
}

bool Reader::canTranslate() const noexcept {
  try {
    return impl_->canTranslate();
  } catch (...) {
    LOG(ERROR) << "canTranslate failed";
    return false;
  }
}

bool Reader::canEdit() const noexcept {
  try {
    return impl_->canEdit();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool Reader::canSave() const noexcept { return canSave(false); }

bool Reader::canSave(const bool encrypted) const noexcept {
  try {
    return impl_->canSave(encrypted);
  } catch (...) {
    LOG(ERROR) << "canSave failed";
    return false;
  }
}

bool Reader::isEncrypted() const noexcept {
  try {
    return impl_->isEncrypted();
  } catch (...) {
    LOG(ERROR) << "isEncrypted failed";
    return false;
  }
}

FileType Reader::type() const noexcept {
  try {
    return impl_->type();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

const FileMeta &Reader::meta() const noexcept {
  try {
    return impl_->meta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

bool Reader::decrypt(const std::string &password) const noexcept {
  try {
    return impl_->decrypt(password);
  } catch (...) {
    LOG(ERROR) << "decrypt failed";
    return false;
  }
}

bool Reader::translate(const std::string &path,
                       const Config &config) const noexcept {
  try {
    return impl_->translate(path, config);
  } catch (...) {
    LOG(ERROR) << "translate failed";
    return false;
  }
}

bool Reader::edit(const std::string &diff) const noexcept {
  try {
    return impl_->edit(diff);
  } catch (...) {
    LOG(ERROR) << "edit failed";
    return false;
  }
}

} // namespace odr
