#include <odr/DocumentNoExcept.h>
#include <glog/logging.h>

namespace odr {

std::optional<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path) noexcept {
  try {
    return DocumentNoExcept(Document(path));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

std::optional<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path, const FileType as) noexcept {
  try {
    return DocumentNoExcept(Document(path, as));
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
    : FileNoExcept(std::make_unique<Document>(std::move(impl))) {}

DocumentNoExcept::DocumentNoExcept(std::unique_ptr<Document> document) : FileNoExcept(std::move(document)) {}

DocumentNoExcept::DocumentNoExcept(FileNoExcept &&file) : FileNoExcept(std::move(file)) {
  // TODO check document
}

DocumentNoExcept::DocumentNoExcept(DocumentNoExcept &&) noexcept = default;

DocumentNoExcept::~DocumentNoExcept() = default;

Document & DocumentNoExcept::document() const noexcept {
  return *static_cast<Document *>(m_impl.get());
}

DocumentType DocumentNoExcept::documentType() const noexcept {
  try {
    return document().documentType();
  } catch (...) {
    LOG(ERROR) << "document type failed";
    return DocumentType::UNKNOWN;
  }
}

bool DocumentNoExcept::encrypted() const noexcept {
  try {
    return document().encrypted();
  } catch (...) {
    LOG(ERROR) << "encrypted failed";
    return false;
  }
}

bool DocumentNoExcept::decrypted() const noexcept {
  try {
    return document().decrypted();
  } catch (...) {
    LOG(ERROR) << "decrypted failed";
    return false;
  }
}

bool DocumentNoExcept::canTranslate() const noexcept {
  try {
    return document().translatable();
  } catch (...) {
    LOG(ERROR) << "canTranslate failed";
    return false;
  }
}

bool DocumentNoExcept::canEdit() const noexcept {
  try {
    return document().editable();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool DocumentNoExcept::canSave() const noexcept { return canSave(false); }

bool DocumentNoExcept::canSave(const bool encrypted) const noexcept {
  try {
    return document().savable(encrypted);
  } catch (...) {
    LOG(ERROR) << "canSave failed";
    return false;
  }
}

bool DocumentNoExcept::decrypt(const std::string &password) const noexcept {
  try {
    return document().decrypt(password);
  } catch (...) {
    LOG(ERROR) << "decrypt failed";
    return false;
  }
}

bool DocumentNoExcept::translate(const std::string &path,
                                 const Config &config) const noexcept {
  try {
    document().translate(path, config);
    return true;
  } catch (...) {
    LOG(ERROR) << "translate failed";
    return false;
  }
}

bool DocumentNoExcept::edit(const std::string &diff) const noexcept {
  try {
    document().edit(diff);
    return true;
  } catch (...) {
    LOG(ERROR) << "edit failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path) const noexcept {
  try {
    document().save(path);
    return true;
  } catch (...) {
    LOG(ERROR) << "save failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path,
                            const std::string &password) const noexcept {
  try {
    document().save(path, password);
    return true;
  } catch (...) {
    LOG(ERROR) << "saveEncrypted failed";
    return false;
  }
}

}
