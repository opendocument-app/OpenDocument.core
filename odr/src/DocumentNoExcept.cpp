#include <odr/DocumentNoExcept.h>
#include <glog/logging.h>

namespace odr {

std::unique_ptr<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path) noexcept {
  try {
    return std::make_unique<DocumentNoExcept>(Document(path));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

std::unique_ptr<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path, const FileType as) noexcept {
  try {
    return std::make_unique<DocumentNoExcept>(Document(path, as));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

FileType DocumentNoExcept::type(const std::string &path) noexcept {
  try {
    return Document::type(path);
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

FileMeta DocumentNoExcept::meta(const std::string &path) noexcept {
  try {
    return Document::meta(path);
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

DocumentNoExcept::DocumentNoExcept(Document &&impl)
    : impl_{std::move(impl)} {}

DocumentNoExcept::DocumentNoExcept(DocumentNoExcept &&) noexcept = default;

DocumentNoExcept::~DocumentNoExcept() = default;

FileType DocumentNoExcept::type() const noexcept {
  try {
    return impl_.type();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

bool DocumentNoExcept::encrypted() const noexcept {
  try {
    return impl_.encrypted();
  } catch (...) {
    LOG(ERROR) << "encrypted failed";
    return false;
  }
}

const FileMeta &DocumentNoExcept::meta() const noexcept {
  try {
    return impl_.meta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

bool DocumentNoExcept::decrypted() const noexcept {
  try {
    return impl_.decrypted();
  } catch (...) {
    LOG(ERROR) << "decrypted failed";
    return false;
  }
}

bool DocumentNoExcept::canTranslate() const noexcept {
  try {
    return impl_.translatable();
  } catch (...) {
    LOG(ERROR) << "canTranslate failed";
    return false;
  }
}

bool DocumentNoExcept::canEdit() const noexcept {
  try {
    return impl_.editable();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool DocumentNoExcept::canSave() const noexcept { return canSave(false); }

bool DocumentNoExcept::canSave(const bool encrypted) const noexcept {
  try {
    return impl_.savable(encrypted);
  } catch (...) {
    LOG(ERROR) << "canSave failed";
    return false;
  }
}

bool DocumentNoExcept::decrypt(const std::string &password) const noexcept {
  try {
    return impl_.decrypt(password);
  } catch (...) {
    LOG(ERROR) << "decrypt failed";
    return false;
  }
}

bool DocumentNoExcept::translate(const std::string &path,
                                 const Config &config) const noexcept {
  try {
    impl_.translate(path, config);
    return true;
  } catch (...) {
    LOG(ERROR) << "translate failed";
    return false;
  }
}

bool DocumentNoExcept::edit(const std::string &diff) const noexcept {
  try {
    impl_.edit(diff);
    return true;
  } catch (...) {
    LOG(ERROR) << "edit failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path) const noexcept {
  try {
    impl_.save(path);
    return true;
  } catch (...) {
    LOG(ERROR) << "save failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path,
                            const std::string &password) const noexcept {
  try {
    impl_.save(path, password);
    return true;
  } catch (...) {
    LOG(ERROR) << "saveEncrypted failed";
    return false;
  }
}

}
