#include <glog/logging.h>
#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/common/constants.h>
#include <internal/common/path.h>
#include <internal/odf/odf_document.h>
#include <internal/oldms/oldms_file.h>
#include <internal/ooxml/ooxml_document.h>
#include <internal/zip/zip_archive.h>
#include <memory>
#include <odr/config.h>
#include <odr/document.h>
#include <odr/exception.h>
#include <odr/meta.h>
#include <utility>

namespace odr {

namespace {
std::unique_ptr<common::Document> open_impl(const std::string &path) {
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

std::unique_ptr<common::Document> open_impl(const std::string &path,
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
  const auto document = open_impl(path);
  return document->meta().type;
}

FileMeta Document::meta(const std::string &path) {
  const auto document = open_impl(path);
  return document->meta();
}

Document::Document(const std::string &path) : m_impl(open_impl(path)) {}

Document::Document(const std::string &path, const FileType as)
    : m_impl(open_impl(path, as)) {}

Document::Document(Document &&) noexcept = default;

Document::~Document() = default;

Document &Document::operator=(Document &&) noexcept = default;

FileType Document::type() const noexcept { return m_impl->meta().type; }

bool Document::encrypted() const noexcept { return m_impl->meta().encrypted; }

const FileMeta &Document::meta() const noexcept { return m_impl->meta(); }

bool Document::decrypted() const noexcept { return m_impl->decrypted(); }

bool Document::translatable() const noexcept { return m_impl->translatable(); }

bool Document::editable() const noexcept { return m_impl->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_impl->savable(encrypted);
}

bool Document::decrypt(const std::string &password) const {
  return m_impl->decrypt(password);
}

void Document::translate(const std::string &path, const Config &config) const {
  m_impl->translate(path, config);
}

void Document::edit(const std::string &diff) const { m_impl->edit(diff); }

void Document::save(const std::string &path) const { m_impl->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_impl->save(path, password);
}

std::optional<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path) noexcept {
  try {
    return DocumentNoExcept(std::make_unique<Document>(path));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

std::optional<DocumentNoExcept>
DocumentNoExcept::open(const std::string &path, const FileType as) noexcept {
  try {
    return DocumentNoExcept(std::make_unique<Document>(path, as));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

FileType DocumentNoExcept::type(const std::string &path) noexcept {
  try {
    auto document = open_impl(path);
    return document->meta().type;
  } catch (...) {
    LOG(ERROR) << "readType failed";
    return FileType::UNKNOWN;
  }
}

FileMeta DocumentNoExcept::meta(const std::string &path) noexcept {
  try {
    auto document = open_impl(path);
    return document->meta();
  } catch (...) {
    LOG(ERROR) << "readMeta failed";
    return {};
  }
}

DocumentNoExcept::DocumentNoExcept(std::unique_ptr<Document> impl)
    : m_impl{std::move(impl)} {}

FileType DocumentNoExcept::type() const noexcept {
  try {
    return m_impl->type();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

bool DocumentNoExcept::encrypted() const noexcept {
  try {
    return m_impl->encrypted();
  } catch (...) {
    LOG(ERROR) << "encrypted failed";
    return false;
  }
}

const FileMeta &DocumentNoExcept::meta() const noexcept {
  try {
    return m_impl->meta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

bool DocumentNoExcept::decrypted() const noexcept {
  try {
    return m_impl->decrypted();
  } catch (...) {
    LOG(ERROR) << "decrypted failed";
    return false;
  }
}

bool DocumentNoExcept::translatable() const noexcept {
  try {
    return m_impl->translatable();
  } catch (...) {
    LOG(ERROR) << "canTranslate failed";
    return false;
  }
}

bool DocumentNoExcept::editable() const noexcept {
  try {
    return m_impl->editable();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool DocumentNoExcept::savable(const bool encrypted) const noexcept {
  try {
    return m_impl->savable(encrypted);
  } catch (...) {
    LOG(ERROR) << "canSave failed";
    return false;
  }
}

bool DocumentNoExcept::decrypt(const std::string &password) const noexcept {
  try {
    return m_impl->decrypt(password);
  } catch (...) {
    LOG(ERROR) << "decrypt failed";
    return false;
  }
}

bool DocumentNoExcept::translate(const std::string &path,
                                 const Config &config) const noexcept {
  try {
    m_impl->translate(path, config);
    return true;
  } catch (...) {
    LOG(ERROR) << "translate failed";
    return false;
  }
}

bool DocumentNoExcept::edit(const std::string &diff) const noexcept {
  try {
    m_impl->edit(diff);
    return true;
  } catch (...) {
    LOG(ERROR) << "edit failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path) const noexcept {
  try {
    m_impl->save(path);
    return true;
  } catch (...) {
    LOG(ERROR) << "save failed";
    return false;
  }
}

bool DocumentNoExcept::save(const std::string &path,
                            const std::string &password) const noexcept {
  try {
    m_impl->save(path, password);
    return true;
  } catch (...) {
    LOG(ERROR) << "saveEncrypted failed";
    return false;
  }
}

} // namespace odr
