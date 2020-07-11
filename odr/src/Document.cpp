#include <access/CfbStorage.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <access/ZipStorage.h>
#include <common/Constants.h>
#include <common/Document.h>
#include <glog/logging.h>
#include <memory>
#include <odf/OpenDocument.h>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/Exception.h>
#include <odr/Meta.h>
#include <oldms/LegacyMicrosoft.h>
#include <ooxml/OfficeOpenXml.h>
#include <utility>

namespace odr {

namespace {
std::unique_ptr<common::Document> openImpl(const std::string &path) {
  try {
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::ZipReader>(path);

    try {
      return std::make_unique<odf::OpenDocument>(storage);
    } catch (...) {
      // TODO
    }
    try {
      return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {
      // TODO
    }
  } catch (...) {
    // TODO
  }
  try {
    FileMeta meta;
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::CfbReader>(path);

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoft>(storage);
    } catch (...) {
      // TODO
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {
      // TODO
    }
  } catch (...) {
    // TODO
  }

  throw UnknownFileType();
}

std::unique_ptr<common::Document> openImpl(const std::string &path,
                                           const FileType as) {
  // TODO implement
  throw UnknownFileType();
}
} // namespace

std::string Document::version() noexcept {
  return common::Constants::version();
}

std::string Document::commit() noexcept { return common::Constants::commit(); }

FileType Document::type(const std::string &path) {
  const auto document = openImpl(path);
  return document->meta().type;
}

FileMeta Document::meta(const std::string &path) {
  const auto document = openImpl(path);
  return document->meta();
}

Document::Document(const std::string &path) : impl_(openImpl(path)) {}

Document::Document(const std::string &path, const FileType as)
    : impl_(openImpl(path, as)) {}

Document::Document(Document &&) noexcept = default;

Document::~Document() = default;

FileType Document::type() const noexcept { return impl_->meta().type; }

bool Document::encrypted() const noexcept { return impl_->meta().encrypted; }

const FileMeta &Document::meta() const noexcept { return impl_->meta(); }

bool Document::decrypted() const noexcept { return impl_->decrypted(); }

bool Document::translatable() const noexcept { return impl_->translatable(); }

bool Document::editable() const noexcept { return impl_->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return impl_->savable(encrypted);
}

bool Document::decrypt(const std::string &password) const {
  return impl_->decrypt(password);
}

void Document::translate(const std::string &path, const Config &config) const {
  impl_->translate(path, config);
}

void Document::edit(const std::string &diff) const { impl_->edit(diff); }

void Document::save(const std::string &path) const { impl_->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  impl_->save(path, password);
}

std::unique_ptr<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path) noexcept {
  try {
    return std::make_unique<DocumentNoExcept>(std::make_unique<Document>(path));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

std::unique_ptr<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path, const FileType as) noexcept {
  try {
    return std::make_unique<DocumentNoExcept>(
        std::make_unique<Document>(path, as));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

FileType DocumentNoExcept::type(const std::string &path) noexcept {
  try {
    auto document = openImpl(path);
    return document->meta().type;
  } catch (...) {
    LOG(ERROR) << "readType failed";
    return FileType::UNKNOWN;
  }
}

FileMeta DocumentNoExcept::meta(const std::string &path) noexcept {
  try {
    auto document = openImpl(path);
    return document->meta();
  } catch (...) {
    LOG(ERROR) << "readMeta failed";
    return {};
  }
}

DocumentNoExcept::DocumentNoExcept(std::unique_ptr<Document> impl)
    : impl_{std::move(impl)} {}

DocumentNoExcept::DocumentNoExcept(DocumentNoExcept &&) noexcept = default;

DocumentNoExcept::~DocumentNoExcept() = default;

FileType DocumentNoExcept::type() const noexcept {
  try {
    return impl_->type();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

bool DocumentNoExcept::encrypted() const noexcept {
  try {
    return impl_->encrypted();
  } catch (...) {
    LOG(ERROR) << "encrypted failed";
    return false;
  }
}

const FileMeta &DocumentNoExcept::meta() const noexcept {
  try {
    return impl_->meta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

bool DocumentNoExcept::decrypted() const noexcept {
  try {
    return impl_->decrypted();
  } catch (...) {
    LOG(ERROR) << "decrypted failed";
    return false;
  }
}

bool DocumentNoExcept::canTranslate() const noexcept {
  try {
    return impl_->translatable();
  } catch (...) {
    LOG(ERROR) << "canTranslate failed";
    return false;
  }
}

bool DocumentNoExcept::canEdit() const noexcept {
  try {
    return impl_->editable();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool DocumentNoExcept::canSave() const noexcept { return canSave(false); }

bool DocumentNoExcept::canSave(const bool encrypted) const noexcept {
  try {
    return impl_->savable(encrypted);
  } catch (...) {
    LOG(ERROR) << "canSave failed";
    return false;
  }
}

bool DocumentNoExcept::decrypt(const std::string &password) const noexcept {
  try {
    return impl_->decrypt(password);
  } catch (...) {
    LOG(ERROR) << "decrypt failed";
    return false;
  }
}

bool DocumentNoExcept::translate(const std::string &path,
                                 const Config &config) const noexcept {
  try {
    impl_->translate(path, config);
    return true;
  } catch (...) {
    LOG(ERROR) << "translate failed";
    return false;
  }
}

bool DocumentNoExcept::edit(const std::string &diff) const noexcept {
  try {
    impl_->edit(diff);
    return true;
  } catch (...) {
    LOG(ERROR) << "edit failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path) const noexcept {
  try {
    impl_->save(path);
    return true;
  } catch (...) {
    LOG(ERROR) << "save failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path,
                            const std::string &password) const noexcept {
  try {
    impl_->save(path, password);
    return true;
  } catch (...) {
    LOG(ERROR) << "saveEncrypted failed";
    return false;
  }
}

} // namespace odr
