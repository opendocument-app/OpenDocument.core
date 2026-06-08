#include <odr/internal/oldms/presentation/ppt_parser.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/presentation/ppt_element_registry.hpp>
#include <odr/internal/oldms/presentation/ppt_io.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstdint>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odr::internal::oldms::presentation {

namespace {

// Paragraph break inside a PPT text atom.
constexpr char paragraph_mark = '\x0D';
// Manual line break (vertical tab) inside a PPT text atom.
constexpr char line_break_mark = '\x0B';

// Walks the child records of a container by reading forward from the stream. It
// never uses absolute offsets or tellg (the underlying CFB stream's tellg is
// not reliable); only sequential reads are used.
//
// Construct it with the stream positioned at the start of the container body
// and the container's header. next() returns the next child's header with the
// stream positioned at that child's body; read up to recLen bytes from it and
// report how many via consume() — whatever you leave is skipped on the
// following next(). Over a full iteration the cursor consumes exactly
// container.recLen bytes, so nested containers stay in sync.
class ChildCursor {
public:
  ChildCursor(std::istream &in, const RecordHeader &container)
      : m_in(&in), m_remaining(static_cast<std::int64_t>(container.recLen)) {}

  std::optional<RecordHeader> next() {
    skip_body();
    if (m_remaining <= 0) {
      return std::nullopt;
    }
    if (m_remaining < static_cast<std::int64_t>(sizeof(RecordHeader))) {
      throw std::runtime_error("ppt: not enough space in record header");
    }
    RecordHeader header{};
    read(*m_in, header);
    m_remaining -= static_cast<std::int64_t>(sizeof(RecordHeader));
    if (static_cast<std::int64_t>(header.recLen) > m_remaining) {
      throw std::runtime_error("ppt: not enough space in record body");
    }
    m_body = header.recLen;
    return header;
  }

  // Records that `n` bytes of the current child's body have been read.
  void consume(std::uint32_t n) {
    if (n > m_body) {
      throw std::runtime_error("ppt: not enough space in record body");
    }
    m_body -= n;
    m_remaining -= static_cast<std::int64_t>(n);
  }

  void skip_body() {
    if (m_body > 0) {
      m_in->ignore(static_cast<std::streamsize>(m_body));
      m_remaining -= static_cast<std::int64_t>(m_body);
      m_body = 0;
    }
  }

  // Consumes any trailing bytes of the container body that do not form a
  // record, so the cursor always advances the stream by exactly
  // container.recLen.
  void finish() {
    if (m_remaining > 0) {
      m_in->ignore(static_cast<std::streamsize>(m_remaining));
      m_remaining = 0;
    }
  }

private:
  std::istream *m_in{};
  std::int64_t m_remaining{}; // bytes left in the container body
  std::uint32_t m_body{0};    // unconsumed bytes of the current child's body
};

// Reads a record header at the current stream position and returns it, throwing
// if the read fails or the record is not of `expected_type`. The caller
// positions the stream; a wrong type means malformed input, so we fail early.
RecordHeader read_header(std::istream &in, const std::uint16_t expected_type) {
  RecordHeader header{};
  read(in, header);
  if (header.recType != expected_type) {
    throw std::runtime_error("ppt: unexpected record type " +
                             std::to_string(header.recType) + " (expected " +
                             std::to_string(expected_type) + ")");
  }
  return header;
}

// Scans the children of a container (stream positioned at its body, `container`
// its header) for the first one matching rec_type (and recInstance, if given),
// leaving the stream positioned at that child's body and returning its header.
// Returns nullopt if none matches (the container body is then fully consumed).
std::optional<RecordHeader>
find_child(std::istream &in, const RecordHeader &container,
           const std::uint16_t rec_type,
           const std::optional<std::uint16_t> rec_instance = std::nullopt) {
  ChildCursor children(in, container);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == rec_type &&
        (!rec_instance.has_value() || child->rec_instance() == *rec_instance)) {
      return child; // stream is positioned at the child body
    }
  }
  return std::nullopt;
}

// Removes control/anchor characters from a run of slide text, keeping only what
// should be visible. Paragraph and manual line breaks are consumed by the
// caller's split and never reach this function.
std::string clean_text(const std::string &in) {
  std::string out;
  out.reserve(in.size());
  for (const char c : in) {
    const auto uc = static_cast<unsigned char>(c);
    // Keep tab and any non-control byte (>= 0x20, including UTF-8 lead and
    // continuation bytes); drop the remaining control characters.
    if (c == '\x09' || uc >= 0x20) {
      out.push_back(c);
    }
  }
  return out;
}

// Builds the paragraph/line_break/text subtree of a single slide from its
// concatenated text. Mirrors the splitting done by the .doc parser.
void build_slide(ElementRegistry &registry, const ElementIdentifier slide_id,
                 const std::string &slide_text) {
  auto paragraphs =
      util::string::split(slide_text, std::string(1, paragraph_mark));
  // Slide text usually ends on a paragraph break; drop the trailing empty.
  if (!paragraphs.empty() && paragraphs.back().empty()) {
    paragraphs.pop_back();
  }

  for (const auto &paragraph : paragraphs) {
    auto [paragraph_id, _] = registry.create_element(ElementType::paragraph);
    registry.append_child(slide_id, paragraph_id);

    const auto lines =
        util::string::split(paragraph, std::string(1, line_break_mark));
    for (std::size_t line_i = 0; line_i < lines.size(); ++line_i) {
      if (line_i > 0) {
        auto [line_id, _2] = registry.create_element(ElementType::line_break);
        registry.append_child(paragraph_id, line_id);
      }

      auto [text_id, _3, text_element] = registry.create_text_element();
      text_element.text = clean_text(lines[line_i]);
      registry.append_child(paragraph_id, text_id);
    }
  }
}

// Reads the body of a text atom into a string, depending on its record type.
// The stream must be positioned at the atom body; `header` is its header.
std::string read_text_atom(std::istream &in, const RecordHeader &header) {
  if (header.recType == RT_TextBytesAtom) {
    return read_text_bytes(in, header.recLen);
  }
  return read_text_chars(in, header.recLen);
}

// Appends a text atom to a slide's running text, separating consecutive text
// blocks (e.g. title vs. body) with a paragraph break.
void append_text(std::string &slide_text, std::string text) {
  if (text.empty()) {
    return;
  }
  if (!slide_text.empty()) {
    slide_text.push_back(paragraph_mark);
  }
  slide_text += std::move(text);
}

// Recursively gathers all text atoms within a container, in stream order, into
// one string. The stream must be positioned at the start of the container body;
// `container` is its header. Used to collect the text drawn on a single slide.
void gather_text(std::istream &in, const RecordHeader &container,
                 std::string &slide_text) {
  ChildCursor children(in, container);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_TextCharsAtom ||
        child->recType == RT_TextBytesAtom) {
      append_text(slide_text, read_text_atom(in, *child));
      children.consume(child->recLen);
    } else if (child->is_container()) {
      gather_text(in, *child, slide_text);
      children.consume(child->recLen);
    }
    // Other atoms: left unconsumed; the cursor skips their bodies.
  }
}

// Maps a persist object identifier to its offset in the PowerPoint Document
// stream. See [MS-PPT] 2.3.4 "persist object directory".
using PersistDirectory = std::unordered_map<std::uint32_t, std::uint32_t>;

// Adds the (persistId -> offset) pairs of a PersistDirectoryAtom to the
// directory. The stream must be positioned at the atom body; `header` is its
// header. Existing entries are kept (insert-if-absent), so when edits are
// processed newest-first the newest offset for each id wins, as required by
// [MS-PPT] step 8 of the reading algorithm.
void read_persist_directory(std::istream &in, const RecordHeader &header,
                            PersistDirectory &directory) {
  constexpr std::uint32_t field_size = sizeof(std::uint32_t);

  std::uint32_t remaining = header.recLen;
  while (remaining >= field_size) {
    const std::uint32_t entry = read_u32(in); // persistId:20 | cPersist:12
    remaining -= field_size;
    const std::uint32_t persist_id = entry & 0x000FFFFF;
    const std::uint32_t count = entry >> 20;
    for (std::uint32_t i = 0; i < count && remaining >= field_size; ++i) {
      const std::uint32_t persist_offset = read_u32(in);
      remaining -= field_size;
      directory.emplace(persist_id + i, persist_offset);
    }
  }
}

// Reads the persistIdRef of every SlidePersistAtom in a SlideListWithText
// container, in order. The stream must be positioned at the container body;
// `slide_list` is its header.
std::vector<std::uint32_t>
read_slide_persist_ids(std::istream &in, const RecordHeader &slide_list) {
  constexpr std::uint32_t persist_ref_size = sizeof(std::uint32_t);

  std::vector<std::uint32_t> ids;
  ChildCursor children(in, slide_list);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_SlidePersistAtom &&
        child->recLen >= persist_ref_size) {
      ids.push_back(read_u32(in)); // persistIdRef is the first body field
      children.consume(persist_ref_size);
    }
  }
  return ids;
}

// Resolves the presentation slides via the [MS-PPT] reading algorithm (the only
// spec-defined read path): the "Current User" stream points at the newest
// UserEditAtom, whose chain (in the "PowerPoint Document" stream) builds the
// persist object directory; that directory resolves the live DocumentContainer
// and each slide's SlideContainer. Slides therefore come out in presentation
// order, from the live records, regardless of stale/duplicate copies left in
// the stream by incremental saves. The caller provides both required streams
// (spec 2.1.1/2.1.2); malformed records throw.
std::vector<std::string> collect_slides(std::istream &current_user,
                                        std::istream &document) {
  // Newest user edit offset, from the Current User stream.
  CurrentUserAtomHead head{};
  read(current_user, head);
  if (head.rh.recType != RT_CurrentUserAtom) {
    throw std::runtime_error(
        "ppt: invalid CurrentUserAtom in \"Current User\" stream");
  }
  const std::uint32_t current_edit = head.offsetToCurrentEdit;

  // Walk the UserEditAtom chain newest -> oldest, accumulating the persist
  // directory (newest wins) and capturing the live document's persist id.
  PersistDirectory directory;
  std::uint32_t doc_persist_id = 0;
  std::uint32_t edit_offset = current_edit;
  for (bool first = true; edit_offset != 0; first = false) {
    document.clear();
    document.seekg(edit_offset);
    read_header(document, RT_UserEditAtom);
    UserEditAtomBody edit{};
    read(document, edit);
    if (first) {
      doc_persist_id = edit.docPersistIdRef;
    }

    // Persist directory for this edit.
    document.clear();
    document.seekg(edit.offsetPersistDirectory);
    const RecordHeader dir_header =
        read_header(document, RT_PersistDirectoryAtom);
    read_persist_directory(document, dir_header, directory);

    // offsetLastEdit MUST strictly decrease; stop on a non-decreasing (i.e.
    // malformed/looping) chain.
    if (edit.offsetLastEdit >= edit_offset) {
      break;
    }
    edit_offset = edit.offsetLastEdit;
  }
  if (doc_persist_id == 0) {
    return {};
  }

  // Resolve the live DocumentContainer through the directory.
  const auto doc_it = directory.find(doc_persist_id);
  if (doc_it == directory.end()) {
    return {};
  }
  document.clear();
  document.seekg(doc_it->second);
  const RecordHeader doc_header = read_header(document, RT_DocumentContainer);

  // The presentation slide list (recInstance Slides), then its slides' persist
  // ids in presentation order.
  const std::optional<RecordHeader> slide_list = find_child(
      document, doc_header, RT_SlideListWithText, SlideListInstance_Slides);
  if (!slide_list.has_value()) {
    return {}; // valid document, no presentation slides
  }
  const std::vector<std::uint32_t> persist_ids =
      read_slide_persist_ids(document, *slide_list);

  // Each SlidePersistAtom references a SlideContainer by persist id; resolve it
  // and gather the text drawn on that slide.
  std::vector<std::string> slides;
  slides.reserve(persist_ids.size());
  for (const std::uint32_t persist_id : persist_ids) {
    std::string slide_text;
    if (const auto it = directory.find(persist_id); it != directory.end()) {
      document.clear();
      document.seekg(it->second);
      const RecordHeader slide_header =
          read_header(document, RT_SlideContainer);
      gather_text(document, slide_header, slide_text);
    }
    slides.push_back(std::move(slide_text));
  }
  return slides;
}

} // namespace

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const abstract::ReadableFilesystem &files) {
  auto [root_id, _] = registry.create_element(ElementType::root);

  const auto document_file = files.open(AbsPath("/PowerPoint Document"));
  const auto current_user_file = files.open(AbsPath("/Current User"));

  if (document_file == nullptr) {
    throw std::runtime_error("ppt: missing \"PowerPoint Document\" stream");
  }
  if (current_user_file == nullptr) {
    throw std::runtime_error("ppt: missing \"Current User\" stream");
  }

  // Both streams are required in a conformant .ppt (spec 2.1.1/2.1.2).
  const auto document_stream = document_file->stream();
  const auto current_user_stream = current_user_file->stream();

  for (const std::string &slide_text :
       collect_slides(*current_user_stream, *document_stream)) {
    auto [slide_id, _] = registry.create_element(ElementType::slide);
    registry.append_child(root_id, slide_id);
    build_slide(registry, slide_id, slide_text);
  }

  return root_id;
}

} // namespace odr::internal::oldms::presentation
