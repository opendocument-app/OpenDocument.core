#include <ContentTranslator.h>
#include <Context.h>
#include <Crypto.h>
#include <Meta.h>
#include <StyleTranslator.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Html.h>
#include <common/XmlUtil.h>
#include <fstream>
#include <internal/odf/odf_translator.h>
#include <nlohmann/json.hpp>
#include <odr/config.h>
#include <odr/meta.h>
#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {
void generate_style(std::ofstream &out, Context &context) {
  out << common::Html::odfDefaultStyle();

  if (context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET)
    out << common::Html::odfSpreadsheetDefaultStyle();

  const auto stylesXml = common::XmlUtil::parse(*context.storage, "styles.xml");

  const auto fontFaceDecls =
      stylesXml.child("office:document-styles").child("office:font-face-decls");
  if (fontFaceDecls)
    StyleTranslator::css(fontFaceDecls, context);

  const auto styles =
      stylesXml.child("office:document-styles").child("office:styles");
  if (styles)
    StyleTranslator::css(styles, context);

  const auto automaticStyles = stylesXml.child("office:document-styles")
                                   .child("office:automatic-styles");
  if (automaticStyles)
    StyleTranslator::css(automaticStyles, context);

  const auto masterStyles =
      stylesXml.child("office:document-styles").child("office:master-styles");
  if (masterStyles)
    StyleTranslator::css(masterStyles, context);
}

void generateContentStyle_(const pugi::xml_node &in, Context &context) {
  const auto fontFaceDecls =
      in.child("office:document-content").child("office:font-face-decls");
  if (fontFaceDecls)
    StyleTranslator::css(fontFaceDecls, context);

  const auto automaticStyles =
      in.child("office:document-content").child("office:automatic-styles");
  if (automaticStyles)
    StyleTranslator::css(automaticStyles, context);
}

void generate_script(std::ofstream &out, Context &) {
  out << common::Html::defaultScript();
}

void generate_content(const pugi::xml_node &in, Context &context) {
  const pugi::xml_node body =
      in.child("office:document-content").child("office:body");

  pugi::xml_node content;
  std::string entryName;
  switch (context.meta->type) {
  case FileType::OPENDOCUMENT_TEXT:
  case FileType::OPENDOCUMENT_GRAPHICS:
    content = body.child("office:drawing");
    entryName = "draw:page";
    break;
  case FileType::OPENDOCUMENT_PRESENTATION:
    content = body.child("office:presentation");
    entryName = "draw:page";
    break;
  case FileType::OPENDOCUMENT_SPREADSHEET:
    content = body.child("office:spreadsheet");
    entryName = "table:table";
    break;
  default:
    throw std::invalid_argument("type");
  }

  context.entry = 0;

  if (content &&
      ((context.config->entryOffset > 0) || (context.config->entryCount > 0))) {
    std::uint32_t i = 0;
    for (auto &&e : content) {
      if (e.name() != entryName)
        continue;
      if ((i >= context.config->entryOffset) &&
          ((context.config->entryCount == 0) ||
           (i < context.config->entryOffset + context.config->entryCount))) {
        ContentTranslator::html(e, context);
      } else {
        ++context.entry; // TODO hacky
      }
      ++i;
    }
  } else {
    ContentTranslator::html(body, context);
  }
}
} // namespace

OpenDocumentTranslator::OpenDocumentTranslator(
    OpenDocumentTranslator &&) noexcept = default;

OpenDocumentTranslator::OpenDocumentTranslator(const char *path)
    : OpenDocumentTranslator(common::Path(path)) {}

OpenDocumentTranslator::OpenDocumentTranslator(const std::string &path)
    : OpenDocumentTranslator(common::Path(path)) {}

OpenDocumentTranslator::OpenDocumentTranslator(const common::Path &path)
    : OpenDocumentTranslator(
          std::unique_ptr<access::ReadStorage>(new access::ZipReader(path))) {}

OpenDocumentTranslator::OpenDocumentTranslator(
    std::unique_ptr<access::ReadStorage> &&filesystem) {
  m_meta = Meta::parseFileMeta(*filesystem, false);
  m_manifest = Meta::parseManifest(*filesystem);

  m_filesystem = std::move(filesystem);
}

OpenDocumentTranslator::OpenDocumentTranslator(
    std::unique_ptr<access::ReadStorage> &filesystem) {
  m_meta = Meta::parseFileMeta(*filesystem, false);
  m_manifest = Meta::parseManifest(*filesystem);

  m_filesystem = std::move(filesystem);
}

OpenDocumentTranslator::~OpenDocumentTranslator() = default;

OpenDocumentTranslator &
OpenDocumentTranslator::operator=(OpenDocumentTranslator &&) noexcept = default;

const FileMeta &OpenDocumentTranslator::meta() const noexcept { return m_meta; }

const abstract::ReadFilesystem &
OpenDocumentTranslator::filesystem() const noexcept {
  return *m_filesystem;
}

bool OpenDocumentTranslator::decrypted() const noexcept { return m_decrypted; }

bool OpenDocumentTranslator::translatable() const noexcept { return true; }

bool OpenDocumentTranslator::editable() const noexcept { return true; }

bool OpenDocumentTranslator::savable(const bool encrypted) const noexcept {
  if (encrypted)
    return false;
  return !m_meta.encrypted;
}

bool OpenDocumentTranslator::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  // TODO throw if decrypted
  const bool success = Crypto::decrypt(m_filesystem, m_manifest, password);
  if (success)
    m_meta = Meta::parseFileMeta(*m_filesystem, true);
  m_decrypted = success;
  return success;
}

bool OpenDocumentTranslator::translate(const common::Path &path,
                                       const Config &config) {
  // TODO throw if not decrypted
  std::ofstream out(path);
  if (!out.is_open())
    return false;
  m_context.config = &config;
  m_context.meta = &m_meta;
  m_context.storage = m_filesystem.get();
  m_context.output = &out;

  m_content = common::XmlUtil::parse(*m_filesystem, "content.xml");

  out << common::Html::doctype();
  out << "<html><head>";
  out << common::Html::defaultHeaders();
  out << "<style>";
  generate_style(out, m_context);
  generateContentStyle_(m_content, m_context);
  out << "</style>";
  out << "</head>";

  out << "<body " << common::Html::bodyAttributes(config) << ">";
  generate_content(m_content, m_context);
  out << "</body>";

  out << "<script>";
  generate_script(out, m_context);
  out << "</script>";
  out << "</html>";

  m_context.config = nullptr;
  m_context.output = nullptr;
  out.close();
  return true;
}

bool OpenDocumentTranslator::edit(const std::string &diff) {
  // TODO throw if not decrypted
  const auto json = nlohmann::json::parse(diff);

  if (json.contains("modifiedText")) {
    for (auto &&i : json["modifiedText"].items()) {
      const auto it = m_context.textTranslation.find(std::stoi(i.key()));
      // TODO dirty const off-cast
      if (it == m_context.textTranslation.end())
        continue;
      it->second.set(i.value().get<std::string>().c_str());
    }
  }

  return true;
}

bool OpenDocumentTranslator::save(const common::Path &path) const {
  // TODO throw if not decrypted
  // TODO this would decrypt/inflate and encrypt/deflate again
  access::ZipWriter writer(path);

  // `mimetype` has to be the first file and uncompressed
  if (m_filesystem->isFile("mimetype")) {
    const auto in = m_filesystem->read("mimetype");
    const auto out = writer.write("mimetype", 0);
    access::StreamUtil::pipe(*in, *out);
  }

  m_filesystem->visit([&](const auto &p) {
    std::cout << p.string() << std::endl;
    if (p == "mimetype")
      return;
    if (m_filesystem->isDirectory(p)) {
      writer.createDirectory(p);
      return;
    }
    const auto in = m_filesystem->read(p);
    const auto out = writer.write(p);
    if (p == "content.xml") {
      m_content.print(*out);
      return;
    }
    access::StreamUtil::pipe(*in, *out);
  });

  return true;
}

bool OpenDocumentTranslator::save(const common::Path &path,
                                  const std::string &password) const {
  // TODO throw if not decrypted
  return false;
}

} // namespace odr::internal::odf
