#include <odr/internal/pdf/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/pdf/pdf_annotation.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>
#include <odr/internal/pdf/pdf_encryption.hpp>
#include <odr/internal/pdf/pdf_object.hpp>
#include <odr/internal/pdf/pdf_writer.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace odr::internal::pdf {

namespace {

/// One `/Info` string entry, resolved and decoded to UTF-8 (`nullopt` when
/// absent, not a string, or empty).
std::optional<std::string> info_string(DocumentParser &parser,
                                       const Dictionary &info,
                                       const std::string &key) {
  if (!info.has_value(key)) {
    return std::nullopt;
  }
  const Object value = parser.resolve_object_copy(info.get(key));
  if (!value.is_string()) {
    return std::nullopt;
  }
  std::string decoded = decode_text_string(value.as_string());
  if (decoded.empty()) {
    return std::nullopt;
  }
  return decoded;
}

/// Document metadata from the trailer: the page count (the root `/Pages
/// /Count`, a single lookup — no tree walk) and the `/Info` document
/// information dictionary (ISO 32000-1 14.3.3). Reads may throw on a malformed
/// file; the caller treats that as "no metadata".
void parse_document_meta(DocumentParser &parser, FileMeta &meta) {
  meta.document_type = DocumentType::text;

  const Dictionary &trailer = parser.trailer();

  if (trailer.has_value("Root")) {
    const Object root = parser.resolve_object_copy(trailer.get("Root"));
    if (root.is_dictionary() && root.as_dictionary().has_value("Pages")) {
      const Object pages =
          parser.resolve_object_copy(root.as_dictionary().get("Pages"));
      if (pages.is_dictionary() && pages.as_dictionary().has_value("Count")) {
        const Object count =
            parser.resolve_object_copy(pages.as_dictionary().get("Count"));
        if (count.is_integer() && count.as_integer() >= 0) {
          meta.entry_count = static_cast<std::uint32_t>(count.as_integer());
        }
      }
    }
  }

  if (trailer.has_value("Info")) {
    const Object info = parser.resolve_object_copy(trailer.get("Info"));
    if (info.is_dictionary()) {
      const Dictionary &d = info.as_dictionary();
      meta.title = info_string(parser, d, "Title");
      meta.author = info_string(parser, d, "Author");
      meta.subject = info_string(parser, d, "Subject");
      meta.keywords = info_string(parser, d, "Keywords");
      meta.creator = info_string(parser, d, "Creator");
      meta.producer = info_string(parser, d, "Producer");
      meta.creation_date = info_string(parser, d, "CreationDate");
      meta.modification_date = info_string(parser, d, "ModDate");
    }
  }
}

/// Fill the document fields of `meta` best-effort (page count + `/Info`) from a
/// readable parser. Never fatal, and all-or-nothing: a malformed structure
/// leaves every document field unset rather than half-filled.
void populate_document_meta(DocumentParser &parser, FileMeta &meta) {
  try {
    FileMeta parsed = meta;
    parse_document_meta(parser, parsed);
    meta = std::move(parsed);
  } catch (...) { // NOLINT(bugprone-empty-catch): metadata is best-effort, a
                  // malformed file simply carries no document metadata.
  }
}

} // namespace

PdfFile::PdfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw std::invalid_argument("pdf: file is null");
  }

  DocumentParser parser(m_file->stream());

  m_file_meta.type = FileType::portable_document_format;
  // Both conditions `IncrementalWriter` refuses on. An `/Encrypt` counts even
  // where the empty password opens it, since the new objects would still have
  // to be encrypted with a key the parser does not keep.
  m_annotatable = !parser.is_encrypted() && !parser.is_recovered();

  m_authenticator = parser.authenticator();
  if (parser.is_encrypted()) {
    // Most "protected" PDFs are owner-locked only, so try the empty user
    // password first; if it opens the file, no password is required. Install
    // the resulting decryptor on `parser` so the metadata read below decrypts,
    // and keep a copy so rendering needs neither the password nor decrypt().
    // A file with an unsupported `/Encrypt` handler has no authenticator: it
    // stays encrypted (and not decodable), rather than throwing here.
    const bool unlocked =
        m_authenticator.has_value() && parser.authenticate("");
    if (unlocked) {
      m_decryptor = parser.decryptor();
    }
    m_file_meta.password_encrypted = !unlocked;
    m_encryption_state =
        unlocked ? EncryptionState::not_encrypted : EncryptionState::encrypted;
  }

  // Best-effort document metadata (page count + `/Info`), only when the file is
  // readable.
  if (m_encryption_state != EncryptionState::encrypted) {
    populate_document_meta(parser, m_file_meta);
  }
}

std::shared_ptr<abstract::File> PdfFile::file() const noexcept {
  return m_file;
}

FileMeta PdfFile::file_meta() const noexcept { return m_file_meta; }

bool PdfFile::annotatable() const noexcept { return m_annotatable; }

bool PdfFile::password_encrypted() const noexcept {
  return m_encryption_state == EncryptionState::encrypted;
}

EncryptionState PdfFile::encryption_state() const noexcept {
  return m_encryption_state;
}

std::shared_ptr<abstract::DecodedFile>
PdfFile::decrypt(const std::string &password) const {
  if (m_encryption_state != EncryptionState::encrypted) {
    throw NotEncryptedError();
  }
  if (!m_authenticator.has_value()) {
    throw std::logic_error("pdf: no authenticator for encrypted file");
  }
  std::optional<Decryptor> decryptor = m_authenticator->authenticate(password);
  if (!decryptor.has_value()) {
    throw WrongPasswordError();
  }

  auto decrypted = std::make_shared<PdfFile>(*this);
  decrypted->m_decryptor = std::move(decryptor);
  decrypted->m_encryption_state = EncryptionState::decrypted;
  decrypted->m_file_meta.password_encrypted = false;

  // The file is readable now, so fill in the best-effort document metadata that
  // construction had to skip while it was locked.
  DocumentParser parser(m_file->stream(), decrypted->m_decryptor);
  populate_document_meta(parser, decrypted->m_file_meta);

  return decrypted;
}

bool PdfFile::is_decodable() const noexcept {
  return m_encryption_state != EncryptionState::encrypted;
}

DocumentParser PdfFile::create_parser(const Logger &logger) const {
  return DocumentParser(m_file->stream(), m_decryptor, logger);
}

namespace {

/// A required member of `value`, refused rather than defaulted.
const nlohmann::json &at(const nlohmann::json &value, const char *key) {
  const auto it = value.find(key);
  if (it == value.end()) {
    throw std::invalid_argument(std::string("annotation is missing /") + key);
  }
  return *it;
}

std::array<double, 3> read_color(const nlohmann::json &value) {
  if (!value.is_array() || value.size() != 3) {
    throw std::invalid_argument("color is not three components");
  }
  return {value[0].get<double>(), value[1].get<double>(),
          value[2].get<double>()};
}

AnnotationCommon read_common(const nlohmann::json &value) {
  AnnotationCommon result;
  result.color = read_color(at(value, "color"));
  result.opacity = value.value("opacity", 1.0);
  result.author = value.value("author", std::string());
  result.contents = value.value("contents", std::string());
  return result;
}

TextMarkupKind read_markup_kind(const std::string &type) {
  if (type == "highlight") {
    return TextMarkupKind::highlight;
  }
  if (type == "underline") {
    return TextMarkupKind::underline;
  }
  if (type == "strikeOut") {
    return TextMarkupKind::strike_out;
  }
  if (type == "squiggly") {
    return TextMarkupKind::squiggly;
  }
  throw std::invalid_argument("unknown annotation type " + type);
}

TextMarkup read_text_markup(const nlohmann::json &value,
                            const std::string &type) {
  TextMarkup result;
  result.kind = read_markup_kind(type);
  for (const nlohmann::json &quad : at(value, "quads")) {
    if (!quad.is_array() || quad.size() != 8) {
      throw std::invalid_argument("quad is not eight coordinates");
    }
    Quad &out = result.quads.emplace_back();
    for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = quad[i].get<double>();
    }
  }
  result.common = read_common(value);
  return result;
}

Ink read_ink(const nlohmann::json &value) {
  Ink result;
  for (const nlohmann::json &stroke : at(value, "strokes")) {
    if (!stroke.is_array() || stroke.empty() || stroke.size() % 2 != 0) {
      throw std::invalid_argument("stroke is not a sequence of x y pairs");
    }
    result.strokes.push_back(stroke.get<std::vector<double>>());
  }
  result.width = value.value("width", 1.0);
  result.common = read_common(value);
  return result;
}

/// @throws std::invalid_argument for a payload this build cannot write.
void write_annotations(DocumentParser &parser, const nlohmann::json &json,
                       std::ostream &out) {
  // the guard against a payload from a frontend this build does not know
  if (json.value("version", 0) != 1) {
    throw std::invalid_argument("unsupported annotation format version");
  }

  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();

  IncrementalWriter writer(parser);
  // one page rewrite per page, however many annotations land on it
  std::map<std::size_t, std::vector<ObjectReference>> by_page;

  for (const nlohmann::json &value :
       json.value("annotations", nlohmann::json::array())) {
    const auto index = at(value, "page").get<std::size_t>();
    if (index >= pages.size()) {
      throw std::invalid_argument("annotation names page " +
                                  std::to_string(index) +
                                  ", which is not there");
    }
    const auto type = at(value, "type").get<std::string>();

    by_page[index].push_back(
        type == "ink"
            ? write_ink(writer, read_ink(value))
            : write_text_markup(writer, read_text_markup(value, type)));
  }

  for (const auto &[index, references] : by_page) {
    append_page_annotations(writer, *pages[index], references);
  }

  writer.write(out);
}

} // namespace

void PdfFile::annotate(const std::string_view annotations, std::ostream &out,
                       const Logger &logger) const {
  try {
    const nlohmann::json json = nlohmann::json::parse(annotations);
    DocumentParser parser = create_parser(logger);
    write_annotations(parser, json, out);
  } catch (const nlohmann::json::exception &e) {
    // nlohmann reports a member of the wrong type in a hierarchy of its own,
    // and that is a malformed payload like any other
    throw std::invalid_argument(std::string("annotations are malformed: ") +
                                e.what());
  }
}

} // namespace odr::internal::pdf
