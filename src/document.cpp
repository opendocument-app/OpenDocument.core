#include <glog/logging.h>
#include <internal/abstract/document_translator.h>
#include <internal/abstract/filesystem.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/common/archive.h>
#include <internal/common/constants.h>
#include <internal/common/path.h>
#include <internal/odf/odf_translator.h>
#include <internal/zip/zip_archive.h>
#include <memory>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file_meta.h>
#include <odr/html_config.h>
#include <utility>

using namespace odr::internal;

namespace odr {

namespace {
std::unique_ptr<internal::abstract::DocumentTranslator>
open_impl(const std::string &path) {
  std::shared_ptr<common::DiscFile> disc_file =
      std::make_shared<common::DiscFile>(path);

  try {
    common::ArchiveFile<zip::ReadonlyZipArchive> zip(disc_file);

    auto filesystem = zip.archive()->filesystem();

    try {
      odf::OpenDocumentTranslator tmp(filesystem);
      return std::make_unique<odf::OpenDocumentTranslator>(filesystem);
    } catch (...) {
      // TODO
    }
    try {
      // TODO
      // return std::make_unique<ooxml::OfficeOpenXml>(filesystem);
    } catch (...) {
      // TODO
    }
  } catch (...) {
    // TODO
  }

  try {
    auto memory_file = std::make_shared<common::MemoryFile>(*disc_file);

    common::ArchiveFile<cfb::ReadonlyCfbArchive> cfb(memory_file);

    auto filesystem = cfb.archive()->filesystem();

    // legacy microsoft
    try {
      // TODO
      // return std::make_unique<oldms::LegacyMicrosoft>(storage);
    } catch (...) {
      // TODO
    }

    // encrypted ooxml
    try {
      // TODO
      // return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {
      // TODO
    }
  } catch (...) {
    // TODO
  }

  throw UnknownFileType();
}

std::unique_ptr<internal::abstract::DocumentTranslator>
open_impl(const std::string &path, const FileType as) {
  // TODO implement
  throw UnknownFileType();
}
} // namespace

std::string Document::version() noexcept {
  return internal::common::constants::version();
}

std::string Document::commit() noexcept {
  return internal::common::constants::commit();
}

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

void Document::translate(const std::string &path,
                         const HtmlConfig &config) const {
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
                                 const HtmlConfig &config) const noexcept {
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
