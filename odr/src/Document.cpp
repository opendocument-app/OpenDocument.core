#include <access/CfbStorage.h>
#include <access/Storage.h>
#include <access/ZipStorage.h>
#include <common/Constants.h>
#include <glog/logging.h>
#include <memory>
#include <odf/OpenDocument.h>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/Meta.h>
#include <ooxml/OfficeOpenXml.h>
#include <utility>

namespace odr {

class Document::Impl {
public:
  Impl() = default;
  Impl(FileMeta meta) : meta_{std::move(meta)} {}
  virtual ~Impl() = default;

  FileType type() const { return meta_.type; }
  bool encrypted() const { return meta_.encrypted; }
  virtual const FileMeta &meta() const { return meta_; }

  virtual bool decrypted() const { return false; }
  virtual bool canTranslate() const { return false; }
  virtual bool canEdit() const { return false; }
  virtual bool canSave() const { return false; }
  virtual bool canSave(bool encrypted) const { return false; }

  virtual bool decrypt(const std::string &password) { return false; }

  virtual bool translate(const std::string &path, const Config &config) {
    return false;
  }
  virtual bool edit(const std::string &diff) { return false; }

  virtual bool save(const std::string &path) const { return false; }
  virtual bool save(const std::string &path,
                    const std::string &password) const {
    return false;
  }

protected:
  FileMeta meta_;
};

namespace {
class OpenDocumentImpl final : public Document::Impl {
public:
  explicit OpenDocumentImpl(std::unique_ptr<access::ReadStorage> &storage)
      : document_{storage} {
    meta_ = document_.meta();
  }

  bool decrypted() const final { return document_.decrypted(); }
  bool canTranslate() const final { return document_.canHtml(); }
  bool canEdit() const final { return document_.canEdit(); }
  bool canSave() const final { return document_.canSave(); }
  bool canSave(const bool encrypted) const final {
    return document_.canSave(encrypted);
  }

  bool decrypt(const std::string &password) final {
    const bool result = document_.decrypt(password);
    if (result)
      meta_ = document_.meta();
    return result;
  }

  bool translate(const std::string &path, const Config &config) final {
    return document_.html(path, config);
  }
  bool edit(const std::string &diff) final { return document_.edit(diff); }

  bool save(const std::string &path) const final {
    return document_.save(path);
  }
  bool save(const std::string &path, const std::string &password) const final {
    return document_.save(path, password);
  }

private:
  odf::OpenDocument document_;
};

class OfficeOpenXmlImpl final : public Document::Impl {
public:
  explicit OfficeOpenXmlImpl(std::unique_ptr<access::ReadStorage> &storage)
      : document_{storage} {
    meta_ = document_.meta();
  }

  bool decrypted() const final { return document_.decrypted(); }
  bool canTranslate() const final { return document_.canHtml(); }
  bool canEdit() const final { return document_.canEdit(); }
  bool canSave() const final { return document_.canSave(); }
  bool canSave(const bool encrypted) const final {
    return document_.canSave(encrypted);
  }

  bool decrypt(const std::string &password) final {
    const bool result = document_.decrypt(password);
    if (result)
      meta_ = document_.meta();
    return result;
  }

  bool translate(const std::string &path, const Config &config) final {
    return document_.html(path, config);
  }
  bool edit(const std::string &diff) final { return document_.edit(diff); }

  bool save(const std::string &path) const final {
    return document_.save(path);
  }
  bool save(const std::string &path, const std::string &password) const final {
    return document_.save(path, password);
  }

private:
  bool decrypted_{false};
  std::unique_ptr<access::ReadStorage> storage_;
  ooxml::OfficeOpenXml document_;
};

struct UnknownFileType : public std::runtime_error {
  UnknownFileType() : std::runtime_error("unknown file type") {}
};

std::unique_ptr<Document::Impl> openImpl(const std::string &path) {
  try {
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::ZipReader>(path);

    try {
      return std::make_unique<OpenDocumentImpl>(storage);
    } catch (...) {
    }
    try {
      return std::make_unique<OfficeOpenXmlImpl>(storage);
    } catch (...) {
    }
  } catch (access::NoZipFileException &) {
  }
  try {
    FileMeta meta;
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::CfbReader>(path);

    // MS-DOC: The "WordDocument" stream MUST be present in the file.
    // https://msdn.microsoft.com/en-us/library/dd926131(v=office.12).aspx
    if (storage->isFile("WordDocument")) {
      meta.type = FileType::LEGACY_WORD_DOCUMENT;
      return std::make_unique<Document::Impl>(meta);
    }
    // MS-PPT: The "PowerPoint Document" stream MUST be present in the file.
    // https://msdn.microsoft.com/en-us/library/dd911009(v=office.12).aspx
    if (storage->isFile("PowerPoint Document")) {
      meta.type = FileType::LEGACY_POWERPOINT_PRESENTATION;
      return std::make_unique<Document::Impl>(meta);
    }
    // MS-XLS: The "Workbook" stream MUST be present in the file.
    // https://docs.microsoft.com/en-us/openspecs/office_file_formats/ms-ppt/1fc22d56-28f9-4818-bd45-67c2bf721ccf
    if (storage->isFile("Workbook")) {
      meta.type = FileType::LEGACY_EXCEL_WORKSHEETS;
      return std::make_unique<Document::Impl>(meta);
    }

    // encrypted ooxml
    try {
      return std::make_unique<OfficeOpenXmlImpl>(storage);
    } catch (...) {
    }
  } catch (access::NoCfbFileException &) {
  }

  throw UnknownFileType();
}

std::unique_ptr<Document::Impl> openImpl(const std::string &path,
                                         const FileType as) {
  // TODO implement
  throw UnknownFileType();
}
} // namespace

std::string Document::version() noexcept {
  return common::Constants::version();
}

std::string Document::commit() noexcept { return common::Constants::commit(); }

FileType Document::readType(const std::string &path) {
  const auto document = openImpl(path);
  return document->type();
}

FileMeta Document::readMeta(const std::string &path) {
  const auto document = openImpl(path);
  return document->meta();
}

Document::Document(const std::string &path) : impl_(openImpl(path)) {}

Document::Document(const std::string &path, const FileType as)
    : impl_(openImpl(path, as)) {}

Document::Document(Document &&) noexcept = default;

Document::~Document() = default;

FileType Document::type() const noexcept { return impl_->type(); }

bool Document::encrypted() const noexcept { return impl_->encrypted(); }

const FileMeta &Document::meta() const noexcept { return impl_->meta(); }

bool Document::decrypted() const noexcept { return impl_->decrypted(); }

bool Document::canTranslate() const noexcept { return impl_->canTranslate(); }

bool Document::canEdit() const noexcept { return impl_->canEdit(); }

bool Document::canSave() const noexcept { return impl_->canSave(); }

bool Document::canSave(const bool encrypted) const noexcept {
  impl_->canSave(encrypted);
}

bool Document::decrypt(const std::string &password) const {
  impl_->decrypt(password);
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

FileType DocumentNoExcept::readType(const std::string &path) noexcept {
  try {
    auto document = openImpl(path);
    return document->type();
  } catch (...) {
    LOG(ERROR) << "readType failed";
    return FileType::UNKNOWN;
  }
}

FileMeta DocumentNoExcept::readMeta(const std::string &path) noexcept {
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
    return impl_->canTranslate();
  } catch (...) {
    LOG(ERROR) << "canTranslate failed";
    return false;
  }
}

bool DocumentNoExcept::canEdit() const noexcept {
  try {
    return impl_->canEdit();
  } catch (...) {
    LOG(ERROR) << "canEdit failed";
    return false;
  }
}

bool DocumentNoExcept::canSave() const noexcept { return canSave(false); }

bool DocumentNoExcept::canSave(const bool encrypted) const noexcept {
  try {
    return impl_->canSave(encrypted);
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
