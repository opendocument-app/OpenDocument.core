#include <access/CfbStorage.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Constants.h>
#include <crypto/CfbCrypto.h>
#include <memory>
#include <odf/OpenDocument.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/OpenDocumentReader.h>
#include <ooxml/OfficeOpenXml.h>
#include <glog/logging.h>

namespace odr {

class OpenDocumentReader::Impl final {
public:
  FileType guess(const std::string &path) {
    if (!open(path))
      return FileType::UNKNOWN;
    const FileType result = meta_.type;
    close();
    return result;
  }

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

  const FileMeta &getMeta() const noexcept { return meta_; }

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

      // TODO legacy microsoft documents

      {
        meta_ = {};
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

std::string OpenDocumentReader::getVersion() noexcept {
  return common::Constants::getVersion();
}

std::string OpenDocumentReader::getCommit() noexcept {
  return common::Constants::getCommit();
}

OpenDocumentReader::OpenDocumentReader() : impl_(std::make_unique<Impl>()) {}

OpenDocumentReader::~OpenDocumentReader() = default;

FileType OpenDocumentReader::guess(const std::string &path) const noexcept {
  try {
    return impl_->guess(path);
  } catch (...) {
    LOG(ERROR) << "guess failed";
    return FileType::UNKNOWN;
  }
}

bool OpenDocumentReader::open(const std::string &path) const noexcept {
  try {
    return impl_->open(path);
  } catch (...) {
    LOG(ERROR) << "open failed";
    return false;
  }
}

bool OpenDocumentReader::open(const std::string &path, const FileType as) const
    noexcept {
  try {
    return impl_->open(path, as);
  } catch (...) {
    LOG(ERROR) << "openAs failed";
    return false;
  }
}

bool OpenDocumentReader::save(const std::string &path) const noexcept {
  try {
    return impl_->save(path);
  } catch (...) {
    LOG(ERROR) << "save failed";
    return false;
  }
}

bool OpenDocumentReader::save(const std::string &path,
                              const std::string &password) const noexcept {
  try {
    return impl_->save(path, password);
  } catch (...) {
    LOG(ERROR) << "saveEncrypted failed";
    return false;
  }
}

void OpenDocumentReader::close() const noexcept {
  try {
    impl_->close();
  } catch (...) {
    LOG(ERROR) << "close failed";
  }
}

bool OpenDocumentReader::isOpen() const noexcept {
  try {
    return impl_->isOpen();
  } catch (...) {
    LOG(ERROR) << "isOpen failed";
    return false;
  }
}

bool OpenDocumentReader::isDecrypted() const noexcept {
  try {
    return impl_->isDecrypted();
  } catch (...) {
    LOG(ERROR) << "isDecrypted failed";
    return false;
  }
}

bool OpenDocumentReader::canTranslate() const noexcept {
  try {
    return impl_->canTranslate();
  } catch (...) {
    LOG(ERROR) << "canTranslate failed";
    return false;
  }
}

bool OpenDocumentReader::canEdit() const noexcept {
  try {
    return impl_->canEdit();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool OpenDocumentReader::canSave() const noexcept { return canSave(false); }

bool OpenDocumentReader::canSave(const bool encrypted) const noexcept {
  try {
    return impl_->canSave(encrypted);
  } catch (...) {
    LOG(ERROR) << "canSave failed";
    return false;
  }
}

const FileMeta &OpenDocumentReader::getMeta() const noexcept {
  try {
    return impl_->getMeta();
  } catch (...) {
    LOG(ERROR) << "getMeta failed";
    return {};
  }
}

bool OpenDocumentReader::decrypt(const std::string &password) const noexcept {
  try {
    return impl_->decrypt(password);
  } catch (...) {
    LOG(ERROR) << "decrypt failed";
    return false;
  }
}

bool OpenDocumentReader::translate(const std::string &path,
                                   const Config &config) const noexcept {
  try {
    return impl_->translate(path, config);
  } catch (...) {
    LOG(ERROR) << "translate failed";
    return false;
  }
}

bool OpenDocumentReader::edit(const std::string &diff) const noexcept {
  try {
    return impl_->edit(diff);
  } catch (...) {
    LOG(ERROR) << "edit failed";
    return false;
  }
}

} // namespace odr
