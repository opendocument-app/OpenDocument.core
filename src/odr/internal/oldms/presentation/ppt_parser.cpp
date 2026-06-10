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

/// Paragraph break inside a PPT text atom.
constexpr char paragraph_mark = '\x0D';
/// Manual line break (vertical tab) inside a PPT text atom.
constexpr char line_break_mark = '\x0B';

/// Sequentially walks a container's child records — no tellg/absolute offsets.
/// Construct with the stream at the container body. next() returns the next
/// child header with the stream at its body; read up to recLen bytes and report
/// them via consume(), the rest is skipped. Advances exactly container.recLen.
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
    RecordHeader header = read_record_header(*m_in);
    m_remaining -= static_cast<std::int64_t>(sizeof(RecordHeader));
    if (static_cast<std::int64_t>(header.recLen) > m_remaining) {
      throw std::runtime_error("ppt: not enough space in record body");
    }
    m_body = header.recLen;
    return header;
  }

  /// Records that `n` bytes of the current child's body have been read.
  void consume(const std::uint32_t n) {
    if (n > m_body) {
      throw std::runtime_error("ppt: not enough space in record body");
    }
    m_body -= n;
    m_remaining -= static_cast<std::int64_t>(n);
  }

  void skip_body() {
    if (m_body > 0) {
      m_in->ignore(m_body);
      m_remaining -= static_cast<std::int64_t>(m_body);
      m_body = 0;
    }
  }

  /// Skips any trailing bytes so the cursor advances by exactly
  /// container.recLen.
  void finish() {
    if (m_remaining > 0) {
      m_in->ignore(m_remaining);
      m_remaining = 0;
    }
  }

private:
  std::istream *m_in{};
  std::int64_t m_remaining{}; //< bytes left in the container body
  std::uint32_t m_body{0};    //< unconsumed bytes of the current child's body
};

/// Reads a record header, throwing unless it is of `expected_type`.
RecordHeader read_header(std::istream &in, const std::uint16_t expected_type) {
  const RecordHeader header = read_record_header(in);
  if (header.recType != expected_type) {
    throw std::runtime_error("ppt: unexpected record type " +
                             std::to_string(header.recType) + " (expected " +
                             std::to_string(expected_type) + ")");
  }
  return header;
}

/// Scans `container`'s children for the first matching rec_type (and
/// recInstance, if given), leaving the stream at that child's body. nullopt if
/// none matches (the container body is then fully consumed).
std::optional<RecordHeader>
find_child(std::istream &in, const RecordHeader &container,
           const std::uint16_t rec_type,
           const std::optional<std::uint16_t> rec_instance = std::nullopt) {
  ChildCursor children(in, container);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == rec_type &&
        (!rec_instance.has_value() || child->recInstance == *rec_instance)) {
      return child; // stream is positioned at the child body
    }
  }
  return std::nullopt;
}

/// Like find_child but for a record the spec mandates: throws if it is absent.
RecordHeader require_child(std::istream &in, const RecordHeader &container,
                           const std::uint16_t rec_type) {
  if (const std::optional<RecordHeader> child =
          find_child(in, container, rec_type)) {
    return *child;
  }
  throw std::runtime_error("ppt: missing required record type " +
                           std::to_string(rec_type));
}

/// Drops control/anchor characters from a run of slide text. Paragraph and line
/// breaks are split out by the caller and never reach here.
std::string clean_text(const std::string &in) {
  std::string out;
  out.reserve(in.size());
  for (const char c : in) {
    const auto uc = static_cast<unsigned char>(c);
    // Keep tab and any non-control byte (>= 0x20, including UTF-8 lead and
    // continuation bytes); drop the remaining control characters.
    if (uc < 0x20 && c != '\x09') {
      continue;
    }
    out.push_back(c);
  }
  return out;
}

/// A single text box on a slide: the text it holds and, optionally, the
/// position and size from its OfficeArtClientAnchor.
struct TextBox final {
  std::optional<Anchor> anchor;
  std::string text;
};

/// Builds the paragraph/line_break/text subtree of one text box under
/// `parent_id`, mirroring the .doc parser's splitting.
void build_paragraphs(ElementRegistry &registry,
                      const ElementIdentifier parent_id,
                      const std::string &box_text) {
  auto paragraphs =
      util::string::split(box_text, std::string(1, paragraph_mark));
  // Box text usually ends on a paragraph break; drop the trailing empty.
  if (!paragraphs.empty() && paragraphs.back().empty()) {
    paragraphs.pop_back();
  }

  for (const auto &paragraph : paragraphs) {
    auto [paragraph_id, _] = registry.create_element(ElementType::paragraph);
    registry.append_child(parent_id, paragraph_id);

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

/// Reads the body of a text atom into a string, depending on its record type.
/// The stream must be positioned at the atom body; `header` is its header.
std::string read_text_atom(std::istream &in, const RecordHeader &header) {
  if (header.recType == RT_TextBytesAtom) {
    return read_text_bytes(in, header.recLen);
  }
  return read_text_chars(in, header.recLen);
}

/// Appends a text atom to a slide's running text, separating consecutive text
/// blocks (e.g. title vs. body) with a paragraph break.
void append_text(std::string &slide_text, const std::string &text) {
  if (text.empty()) {
    return;
  }
  if (!slide_text.empty()) {
    slide_text.push_back(paragraph_mark);
  }
  slide_text += text;
}

/// Recursively concatenates a container's text in stream order. A box holds
/// inline TextChars/TextBytes atoms or an OutlineTextRefAtom indexing the
/// slide's `outline_texts` ([MS-PPT] 2.9.78). Stream at the container body.
void gather_text(std::istream &in, const RecordHeader &container,
                 std::string &slide_text,
                 const std::vector<std::string> &outline_texts) {
  ChildCursor children(in, container);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_TextCharsAtom ||
        child->recType == RT_TextBytesAtom) {
      append_text(slide_text, read_text_atom(in, *child));
      children.consume(child->recLen);
    } else if (child->recType == RT_OutlineTextRefAtom &&
               child->recLen >= sizeof(std::int32_t)) {
      // Box references this slide's index-th outline-text block; an
      // out-of-range index is ignored rather than aborting.
      const auto index = static_cast<std::int32_t>(read_u32(in));
      children.consume(sizeof(std::int32_t));
      if (index >= 0 &&
          static_cast<std::size_t>(index) < outline_texts.size()) {
        append_text(slide_text, outline_texts[index]);
      }
    } else if (child->is_container()) {
      gather_text(in, *child, slide_text, outline_texts);
      children.consume(child->recLen);
    }
    // Other atoms: left unconsumed; the cursor skips their bodies.
  }
}

/// Reads one shape (OfficeArtSpContainer): its optional anchor and the text of
/// its OfficeArtClientTextbox. Consumes the whole shape body. `outline_texts`
/// resolves an OutlineTextRefAtom. Stream at the shape body.
TextBox read_shape(std::istream &in, const RecordHeader &shape,
                   const std::vector<std::string> &outline_texts) {
  TextBox box;
  ChildCursor children(in, shape);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_OfficeArtClientAnchor) {
      box.anchor = read_client_anchor(in, child->recLen);
      children.consume(child->recLen);
    } else if (child->recType == RT_OfficeArtClientTextbox) {
      gather_text(in, *child, box.text, outline_texts);
      children.consume(child->recLen);
    }
    // Other children (shapeProp, FOPT, clientData, …): skipped by the cursor.
  }
  return box;
}

/// Reads a slide's text boxes in shape (z) order; empty boxes are dropped.
/// First cut: only top-level shapes, whose anchors are already in the slide's
/// master-unit coordinates. Stream at the SlideContainer body.
std::vector<TextBox>
read_slide_text_boxes(std::istream &in, const RecordHeader &slide,
                      const std::vector<std::string> &outline_texts) {
  // SlideContainer → DrawingContainer → OfficeArtDgContainer →
  // OfficeArtSpgrContainer (all mandatory), then iterate the shapes.
  const RecordHeader drawing = require_child(in, slide, RT_Drawing);
  const RecordHeader dg = require_child(in, drawing, RT_OfficeArtDgContainer);
  const RecordHeader spgr = require_child(in, dg, RT_OfficeArtSpgrContainer);

  std::vector<TextBox> boxes;
  ChildCursor shapes(in, spgr);
  while (const std::optional<RecordHeader> shape = shapes.next()) {
    if (shape->recType != RT_OfficeArtSpContainer) {
      continue; // not a shape; the cursor skips it
    }
    TextBox box = read_shape(in, *shape, outline_texts);
    shapes.consume(shape->recLen);
    if (!box.text.empty()) {
      boxes.push_back(std::move(box));
    }
  }
  return boxes;
}

/// Maps a persist object identifier to its offset in the PowerPoint Document
/// stream. See [MS-PPT] 2.3.4 "persist object directory".
using PersistDirectory = std::unordered_map<std::uint32_t, std::uint32_t>;

/// Adds a PersistDirectoryAtom's (persistId -> offset) pairs to the directory.
/// Insert-if-absent, so processing edits newest-first keeps the newest offset
/// per id ([MS-PPT] reading algorithm step 8). Stream at the atom body.
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

/// A SlideListWithText container's text: slides' persistIdRefs in presentation
/// order, plus per slide (by persistIdRef) the text of each TextHeaderAtom
/// block, indexed by OutlineTextRefAtom ([MS-PPT] 2.4.14.3 / 2.9.78).
struct SlideListText {
  std::vector<std::uint32_t> persist_ids;
  std::unordered_map<std::uint32_t, std::vector<std::string>> outline_texts;
};

/// Walks a SlideListWithText container once. Each SlidePersistAtom starts a
/// slide; the following TextHeaderAtoms are its outline-text blocks, each
/// filled by its following TextChars/TextBytes atom. Stream at the container
/// body.
SlideListText read_slide_list_text(std::istream &in,
                                   const RecordHeader &slide_list) {
  constexpr std::uint32_t persist_ref_size = sizeof(std::uint32_t);

  SlideListText result;
  // Outline texts of the slide currently being read; valid until reassigned at
  // the next SlidePersistAtom (unordered_map keeps references stable on
  // insert).
  std::vector<std::string> *current = nullptr;
  ChildCursor children(in, slide_list);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_SlidePersistAtom &&
        child->recLen >= persist_ref_size) {
      const std::uint32_t persist_id = read_u32(in); // persistIdRef is first
      children.consume(persist_ref_size);
      result.persist_ids.push_back(persist_id);
      current = &result.outline_texts[persist_id];
    } else if (child->recType == RT_TextHeaderAtom) {
      if (current != nullptr) {
        current->emplace_back(); // one block per header; text filled in below
      }
    } else if (child->recType == RT_TextCharsAtom ||
               child->recType == RT_TextBytesAtom) {
      if (current != nullptr && !current->empty()) {
        current->back() = read_text_atom(in, *child);
        children.consume(child->recLen);
      }
    }
    // Everything else (style/meta atoms, …): left for the cursor to skip.
  }
  return result;
}

/// Resolves the presentation slides via the [MS-PPT] reading algorithm (the
/// only spec-defined path): the "Current User" stream points at the newest
/// UserEditAtom, whose chain builds the persist directory, which resolves the
/// live DocumentContainer and each SlideContainer — so slides come out in order
/// from the live records, ignoring stale copies left by incremental saves.
/// Malformed records throw. Returns each slide's text boxes in shape order.
std::vector<std::vector<TextBox>> collect_slides(std::istream &current_user,
                                                 std::istream &document) {
  // Newest user edit offset, from the Current User stream.
  const CurrentUserAtomHead head = read_current_user_atom_head(current_user);
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
    const UserEditAtomBody edit = read_user_edit_atom_body(document);
    if (first) {
      doc_persist_id = edit.docPersistIdRef;
    }

    // Persist directory for this edit.
    document.clear();
    document.seekg(edit.offsetPersistDirectory);
    const RecordHeader dir_header =
        read_header(document, RT_PersistDirectoryAtom);
    read_persist_directory(document, dir_header, directory);

    // offsetLastEdit MUST strictly decrease (0 ends the chain); a
    // non-decreasing value is a malformed/looping chain.
    if (edit.offsetLastEdit != 0 && edit.offsetLastEdit >= edit_offset) {
      throw std::runtime_error("ppt: non-decreasing UserEditAtom chain");
    }
    edit_offset = edit.offsetLastEdit;
  }
  if (doc_persist_id == 0) {
    throw std::runtime_error("ppt: empty UserEditAtom chain (no document)");
  }

  // Resolve the live DocumentContainer through the directory.
  const auto doc_it = directory.find(doc_persist_id);
  if (doc_it == directory.end()) {
    throw std::runtime_error("ppt: document persist id not in directory");
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
  const SlideListText slide_list_text =
      read_slide_list_text(document, *slide_list);

  // Each SlidePersistAtom references a SlideContainer by persist id (which the
  // spec requires the directory to resolve); read its text boxes, passing the
  // slide's outline texts so OutlineTextRefAtom boxes can be resolved.
  static constexpr std::vector<std::string> no_outline_texts;
  std::vector<std::vector<TextBox>> slides;
  slides.reserve(slide_list_text.persist_ids.size());
  for (const std::uint32_t persist_id : slide_list_text.persist_ids) {
    const auto it = directory.find(persist_id);
    if (it == directory.end()) {
      throw std::runtime_error("ppt: slide persist id not in directory");
    }
    document.clear();
    document.seekg(it->second);
    const RecordHeader slide_header = read_header(document, RT_SlideContainer);
    const auto ot = slide_list_text.outline_texts.find(persist_id);
    const std::vector<std::string> &outline_texts =
        ot != slide_list_text.outline_texts.end() ? ot->second
                                                  : no_outline_texts;
    slides.push_back(
        read_slide_text_boxes(document, slide_header, outline_texts));
  }
  return slides;
}

} // namespace
} // namespace odr::internal::oldms::presentation

namespace odr::internal::oldms {

ElementIdentifier
presentation::parse_tree(ElementRegistry &registry,
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

  for (const std::vector<TextBox> &boxes :
       collect_slides(*current_user_stream, *document_stream)) {
    auto [slide_id, _] = registry.create_element(ElementType::slide);
    registry.append_child(root_id, slide_id);

    // One frame per text box; the box's paragraphs hang off the frame.
    for (const TextBox &box : boxes) {
      auto [frame_id, frame_element, frame] = registry.create_frame_element();
      frame.anchor = box.anchor;
      registry.append_child(slide_id, frame_id);
      build_paragraphs(registry, frame_id, box.text);
    }
  }

  return root_id;
}

} // namespace odr::internal::oldms
