#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/html.h>
#include <internal/common/path.h>
#include <internal/odf/odf_crypto.h>
#include <internal/odf/odf_manifest.h>
#include <internal/odf/odf_meta.h>
#include <internal/odf/odf_translator.h>
#include <internal/odf/odf_translator_content.h>
#include <internal/odf/odf_translator_context.h>
#include <internal/odf/odf_translator_style.h>
#include <internal/util/stream_util.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <nlohmann/json.hpp>
#include <odr/exceptions.h>
#include <odr/file_meta.h>
#include <odr/html_config.h>
#include <pugixml.hpp>
#include <sstream>

namespace odr::internal::odf {

namespace {
void generate_style(std::ofstream &out, Context &context) {
  out << common::Html::default_style();

  if (context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) {
    out << common::Html::default_spreadsheet_style();
  }

  const auto styles_xml = util::xml::parse(*context.filesystem, "styles.xml");

  const auto font_face_decls = styles_xml.child("office:document-styles")
                                   .child("office:font-face-decls");
  if (font_face_decls) {
    style_translator::css(font_face_decls, context);
  }

  const auto styles =
      styles_xml.child("office:document-styles").child("office:styles");
  if (styles) {
    style_translator::css(styles, context);
  }

  const auto automatic_styles = styles_xml.child("office:document-styles")
                                    .child("office:automatic-styles");
  if (automatic_styles) {
    style_translator::css(automatic_styles, context);
  }

  const auto master_styles =
      styles_xml.child("office:document-styles").child("office:master-styles");
  if (master_styles) {
    style_translator::css(master_styles, context);
  }
}

void generate_content_style(const pugi::xml_node &in, Context &context) {
  const auto font_face_decls =
      in.child("office:document-content").child("office:font-face-decls");
  if (font_face_decls) {
    style_translator::css(font_face_decls, context);
  }

  const auto automatic_styles =
      in.child("office:document-content").child("office:automatic-styles");
  if (automatic_styles) {
    style_translator::css(automatic_styles, context);
  }
}

void generate_script(std::ofstream &out, Context &) {
  out << common::Html::default_script();
}

void generate_content(const pugi::xml_node &in, Context &context) {
  const pugi::xml_node body =
      in.child("office:document-content").child("office:body");

  pugi::xml_node content;
  std::string entry_name;
  switch (context.meta->type) {
  case FileType::OPENDOCUMENT_TEXT:
  case FileType::OPENDOCUMENT_GRAPHICS:
    content = body.child("office:drawing");
    entry_name = "draw:page";
    break;
  case FileType::OPENDOCUMENT_PRESENTATION:
    content = body.child("office:presentation");
    entry_name = "draw:page";
    break;
  case FileType::OPENDOCUMENT_SPREADSHEET:
    content = body.child("office:spreadsheet");
    entry_name = "table:table";
    break;
  default:
    throw std::invalid_argument("type");
  }

  context.entry = 0;

  if (content && ((context.config->entry_offset > 0) ||
                  (context.config->entry_count > 0))) {
    std::uint32_t i = 0;
    for (auto &&e : content) {
      if (e.name() != entry_name) {
        continue;
      }
      if ((i >= context.config->entry_offset) &&
          ((context.config->entry_count == 0) ||
           (i < context.config->entry_offset + context.config->entry_count))) {
        content_translator::html(e, context);
      } else {
        ++context.entry; // TODO hacky
      }
      ++i;
    }
  } else {
    content_translator::html(body, context);
  }
}
} // namespace

OpenDocumentTranslator::OpenDocumentTranslator(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  if (m_filesystem->exists("META-INF/manifest.xml")) {
    auto manifest = util::xml::parse(*m_filesystem, "META-INF/manifest.xml");

    m_meta = parse_file_meta(*m_filesystem, &manifest, false);
    m_manifest = parse_manifest(manifest);
  } else {
    m_meta = parse_file_meta(*m_filesystem, nullptr, false);
  }
}

OpenDocumentTranslator::~OpenDocumentTranslator() = default;

const FileMeta &OpenDocumentTranslator::meta() const noexcept { return m_meta; }

const abstract::ReadableFilesystem &
OpenDocumentTranslator::filesystem() const noexcept {
  return *m_filesystem;
}

bool OpenDocumentTranslator::decrypted() const noexcept { return m_decrypted; }

bool OpenDocumentTranslator::translatable() const noexcept { return true; }

bool OpenDocumentTranslator::editable() const noexcept { return true; }

bool OpenDocumentTranslator::savable(const bool encrypted) const noexcept {
  if (encrypted) {
    return false;
  }
  return !m_meta.encrypted;
}

bool OpenDocumentTranslator::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  // TODO throw if decrypted
  const bool success = odf::decrypt(m_filesystem, m_manifest, password);
  if (success) {
    auto manifest = util::xml::parse(*m_filesystem, "META-INF/manifest.xml");
    m_meta = parse_file_meta(*m_filesystem, &manifest, true);
    m_manifest = parse_manifest(manifest);
  }
  m_decrypted = success;
  return success;
}

void OpenDocumentTranslator::translate(const common::Path &path,
                                       const HtmlConfig &config) {
  // TODO throw if not decrypted
  std::ofstream out(path.path());
  if (!out.is_open()) {
    throw FileNotCreated();
  }

  m_context.config = &config;
  m_context.meta = &m_meta;
  m_context.filesystem = m_filesystem.get();
  m_context.output = &out;

  m_content = util::xml::parse(*m_filesystem, "content.xml");

  out << common::Html::doctype();
  out << "<html><head>";
  out << common::Html::default_headers();
  out << "<style>";
  generate_style(out, m_context);
  generate_content_style(m_content, m_context);
  out << "</style>";
  out << "</head>";

  out << "<body " << common::Html::body_attributes(config) << ">";
  generate_content(m_content, m_context);
  out << "</body>";

  out << "<script>";
  generate_script(out, m_context);
  out << "</script>";
  out << "</html>";

  m_context.config = nullptr;
  m_context.output = nullptr;
  out.close();
}

void OpenDocumentTranslator::edit(const std::string &diff) {
  // TODO throw if not decrypted
  const auto json = nlohmann::json::parse(diff);

  if (json.contains("modifiedText")) {
    for (auto &&i : json["modifiedText"].items()) {
      const auto it = m_context.text_translation.find(std::stoi(i.key()));
      // TODO dirty const off-cast
      if (it == std::end(m_context.text_translation)) {
        continue;
      }
      it->second.set(i.value().get<std::string>().c_str());
    }
  }
}

void OpenDocumentTranslator::save(const common::Path &path) const {
  // TODO throw if not decrypted
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_filesystem->is_file("mimetype")) {
    archive.insert_file(std::end(archive), "mimetype",
                        m_filesystem->open("mimetype"), 0);
  }

  for (auto walker = m_filesystem->file_walker("/"); !walker->end();
       walker->next()) {
    auto p = walker->path();
    if (p == "mimetype") {
      continue;
    }
    if (m_filesystem->is_directory(p)) {
      archive.insert_directory(std::end(archive), p);
      continue;
    }
    if (p == "content.xml") {
      // TODO stream
      std::stringstream out;
      m_content.print(out);
      auto tmp = std::make_shared<common::MemoryFile>(out.str());
      archive.insert_file(std::end(archive), p, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), p, m_filesystem->open(p));
  }

  std::ofstream ostream(path.path());
  archive.save(ostream);
}

void OpenDocumentTranslator::save(const common::Path & /*path*/,
                                  const std::string & /*password*/) const {
  // TODO throw if not decrypted
  throw UnsupportedOperation();
}

} // namespace odr::internal::odf
