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

Document & DocumentNoExcept::impl() const noexcept {
  return *static_cast<Document *>(m_impl.get());
}

DocumentType DocumentNoExcept::documentType() const noexcept {
  try {
    return impl().documentType();
  } catch (...) {
    LOG(ERROR) << "document type failed";
    return DocumentType::UNKNOWN;
  }
}

bool DocumentNoExcept::editable() const noexcept {
  try {
    return impl().editable();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool DocumentNoExcept::savable(const bool encrypted) const noexcept {
  try {
    return impl().savable(encrypted);
  } catch (...) {
    LOG(ERROR) << "canSave failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path) const noexcept {
  try {
    impl().save(path);
    return true;
  } catch (...) {
    LOG(ERROR) << "save failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path,
                            const std::string &password) const noexcept {
  try {
    impl().save(path, password);
    return true;
  } catch (...) {
    LOG(ERROR) << "saveEncrypted failed";
    return false;
  }
}

}
