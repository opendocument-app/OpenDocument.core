#include <odr/internal/oldms/presentation/ppt_parser.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/presentation/ppt_element_registry.hpp>
#include <odr/internal/oldms/presentation/ppt_io.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>
#include <odr/internal/oldms/presentation/ppt_style.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odr::internal::oldms::presentation {
namespace {

/// Paragraph break inside a PPT text atom.
constexpr char paragraph_mark = '\x0D';
/// Manual line break (vertical tab) inside a PPT text atom.
constexpr char line_break_mark = '\x0B';
/// The control characters `build_paragraphs` splits on.
constexpr std::array<char, 2> control_marks = {paragraph_mark, line_break_mark};

/// Sequentially walks a container's child records; the stream must be at the
/// container body. next() leaves the stream at the child's body — read up to
/// recLen bytes and report them via consume(), the rest is skipped.
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
    const auto uc = static_cast<std::uint8_t>(c);
    if (uc < 0x20 && c != '\x09') {
      continue;
    }
    out.push_back(c);
  }
  return out;
}

/// One run of uniformly formatted text within a text box; the style index
/// refers to the document's `StyleRegistry`.
struct StyledRun final {
  std::string text; //< UTF-8
  std::uint32_t style_index{default_style_index};
};
using StyledText = std::vector<StyledRun>;

/// The most recent text atom's raw (undecoded) content, kept until the
/// StyleTextPropAtom that most closely follows it ([MS-PPT] 2.9.44) — or
/// until the next text block shows there is none.
class PendingText final {
public:
  void set_chars(std::u16string chars) {
    m_chars = std::move(chars);
    m_bytes.reset();
    m_set = true;
  }
  void set_bytes(std::string bytes) {
    m_bytes = std::move(bytes);
    m_chars.reset();
    m_set = true;
  }
  void reset() {
    m_chars.reset();
    m_bytes.reset();
    m_set = false;
  }

  [[nodiscard]] bool has_value() const { return m_set; }

  /// Length in characters (UTF-16 code units / bytes).
  [[nodiscard]] std::size_t char_count() const {
    return m_chars.has_value() ? m_chars->size()
                               : (m_bytes.has_value() ? m_bytes->size() : 0);
  }

  /// Decodes the characters [begin, end) to UTF-8.
  [[nodiscard]] std::string decode(const std::size_t begin,
                                   const std::size_t end) const {
    if (m_chars.has_value()) {
      return util::string::u16string_to_string(
          m_chars->substr(begin, end - begin));
    }
    if (m_bytes.has_value()) {
      return decode_text_bytes(
          std::string_view(*m_bytes).substr(begin, end - begin));
    }
    return {};
  }

private:
  std::optional<std::u16string> m_chars;
  std::optional<std::string> m_bytes;
  bool m_set{false};
};

/// Splits a pending text at its character-run boundaries; characters past the
/// last run (or all of them, without a StyleTextPropAtom) get the default
/// style. Empty runs are dropped.
StyledText style_pending(const PendingText &pending,
                         const std::vector<TextCFRun> &runs,
                         StyleContext &context) {
  StyledText result;
  const std::size_t count = pending.char_count();
  std::size_t at = 0;
  for (const TextCFRun &run : runs) {
    if (at >= count) {
      break; // the runs also cover the implicit final paragraph mark
    }
    const std::size_t end = std::min<std::size_t>(count, at + run.count);
    if (std::string text = pending.decode(at, end); !text.empty()) {
      result.push_back({std::move(text), resolve_style(run, context)});
    }
    at = end;
  }
  if (at < count) {
    if (std::string text = pending.decode(at, count); !text.empty()) {
      result.push_back({std::move(text), default_style_index});
    }
  }
  return result;
}

/// One shape on a slide — a text box, a picture, or a picture-filled shape
/// with text on top: its styled text, its picture reference/bytes, and
/// optionally the position and size from its OfficeArtClientAnchor.
struct Shape final {
  std::optional<Anchor> anchor;
  StyledText text;
  /// The shape's picture (pib) or picture fill (fillBlip): a one-based index
  /// into the BLIP store.
  std::optional<std::uint32_t> blip_ref;
  /// The picture's file bytes (JPEG/PNG), resolved from the BLIP store.
  std::string image;
  /// Distinct pseudo-path naming the picture (the file has no container
  /// paths); derived from the BLIP store index.
  std::string image_href;
  /// The shape is the slide background (OfficeArtFSP.fBackground).
  bool is_background{false};
};

/// Builds the paragraph/span/text subtree of one shape's text under
/// `parent_id`; paragraphs open lazily, so the trailing paragraph mark adds no
/// empty one.
void build_paragraphs(ElementRegistry &registry,
                      const ElementIdentifier parent_id,
                      const StyledText &shape_text) {
  ElementIdentifier paragraph_id = null_element_id;

  const auto ensure_paragraph = [&](const std::uint32_t style_index) {
    if (paragraph_id == null_element_id) {
      auto [id, paragraph] = registry.create_element(ElementType::paragraph);
      registry.append_child(parent_id, id);
      registry.set_element_style_index(id, style_index);
      paragraph_id = id;
    }
  };

  for (const StyledRun &run : shape_text) {
    std::size_t at = 0;
    while (at <= run.text.size()) {
      const std::size_t control = run.text.find_first_of(
          control_marks.data(), at, control_marks.size());
      const std::size_t segment_end =
          control == std::string::npos ? run.text.size() : control;

      if (std::string cleaned =
              clean_text(run.text.substr(at, segment_end - at));
          !cleaned.empty()) {
        ensure_paragraph(run.style_index);
        auto [span_id, span] = registry.create_element(ElementType::span);
        registry.set_element_style_index(span_id, run.style_index);
        registry.append_child(paragraph_id, span_id);

        auto [text_id, text_element, text_entry] =
            registry.create_text_element();
        text_entry.text = std::move(cleaned);
        registry.append_child(span_id, text_id);
      }

      if (control == std::string::npos) {
        break;
      }
      if (run.text[control] == paragraph_mark) {
        ensure_paragraph(run.style_index);
        paragraph_id = null_element_id;
      } else if (run.text[control] == line_break_mark) {
        ensure_paragraph(run.style_index);
        auto [line_id, line] = registry.create_element(ElementType::line_break);
        registry.append_child(paragraph_id, line_id);
      } else {
        throw std::runtime_error("ppt: unexpected control character");
      }
      at = control + 1;
    }
  }
}

/// Appends a text block to a text box's running text, separating consecutive
/// blocks (e.g. title vs. body) with a paragraph break.
void append_block(StyledText &slide_text, const StyledText &block) {
  if (block.empty()) {
    return;
  }
  if (!slide_text.empty()) {
    slide_text.push_back({std::string(1, paragraph_mark), default_style_index});
  }
  slide_text.insert(slide_text.end(), block.begin(), block.end());
}

/// Reads a text atom's body without decoding into `pending`. The stream must
/// be positioned at the atom body; `header` is its header.
void read_pending_text(std::istream &in, const RecordHeader &header,
                       PendingText &pending) {
  if (header.recType == RT_TextBytesAtom) {
    pending.set_bytes(read_raw_text_bytes(in, header.recLen));
  } else {
    pending.set_chars(read_raw_text_chars(in, header.recLen));
  }
}

/// Reads a StyleTextPropAtom body and returns its character runs; the run
/// counts also cover the implicit final paragraph mark, hence the +1.
std::vector<TextCFRun> read_style_atom(std::istream &in,
                                       const RecordHeader &header,
                                       const PendingText &pending) {
  const std::string body = util::stream::read(in, header.recLen);
  return parse_style_text_prop_atom(body, pending.char_count() + 1);
}

/// Recursively concatenates a container's styled text in stream order. A
/// text box holds inline TextChars/TextBytes atoms — each optionally followed
/// by a StyleTextPropAtom ([MS-PPT] 2.9.44) — or an OutlineTextRefAtom indexing
/// the slide's `outline_texts` ([MS-PPT] 2.9.78). Stream at the container
/// body.
void gather_text(std::istream &in, const RecordHeader &container,
                 StyledText &slide_text,
                 const std::vector<StyledText> &outline_texts,
                 StyleContext &context) {
  PendingText pending;
  const auto flush = [&](const std::vector<TextCFRun> &runs) {
    if (pending.has_value()) {
      append_block(slide_text, style_pending(pending, runs, context));
      pending.reset();
    }
  };

  ChildCursor children(in, container);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_TextCharsAtom ||
        child->recType == RT_TextBytesAtom) {
      flush({});
      read_pending_text(in, *child, pending);
      children.consume(child->recLen);
    } else if (child->recType == RT_StyleTextPropAtom) {
      const std::vector<TextCFRun> runs = read_style_atom(in, *child, pending);
      children.consume(child->recLen);
      flush(runs);
    } else if (child->recType == RT_OutlineTextRefAtom &&
               child->recLen >= sizeof(std::int32_t)) {
      // Box references this slide's index-th outline-text block; an
      // out-of-range index is ignored rather than aborting.
      const auto index = static_cast<std::int32_t>(read_u32(in));
      children.consume(sizeof(std::int32_t));
      flush({});
      if (index >= 0 &&
          static_cast<std::size_t>(index) < outline_texts.size()) {
        append_block(slide_text, outline_texts[index]);
      }
    } else if (child->is_container()) {
      flush({});
      gather_text(in, *child, slide_text, outline_texts, context);
      children.consume(child->recLen);
    }
    // Other atoms: left unconsumed; the cursor skips their bodies.
  }
  flush({});
}

/// Reads one shape (OfficeArtSpContainer): its optional anchor and the text of
/// its OfficeArtClientTextbox. Consumes the whole shape body. `outline_texts`
/// resolves an OutlineTextRefAtom. Stream at the shape body.
Shape read_shape(std::istream &in, const RecordHeader &header,
                 const std::vector<StyledText> &outline_texts,
                 StyleContext &context) {
  Shape shape;
  ChildCursor children(in, header);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_OfficeArtClientAnchor) {
      shape.anchor = read_client_anchor(in, child->recLen);
      children.consume(child->recLen);
    } else if (child->recType == RT_OfficeArtFSP &&
               child->recLen >= 2 * sizeof(std::uint32_t)) {
      read_u32(in); // spid
      const std::uint32_t flags = read_u32(in);
      children.consume(2 * sizeof(std::uint32_t));
      shape.is_background = (flags & office_art_fsp_background) != 0;
    } else if (child->recType == RT_OfficeArtClientTextbox) {
      gather_text(in, *child, shape.text, outline_texts, context);
      children.consume(child->recLen);
    } else if (child->recType == RT_OfficeArtFOPT) {
      // rh.recInstance is the property count; complex data (skipped by the
      // cursor) follows the array ([MS-ODRAW] 2.2.9).
      const std::uint16_t count = child->recInstance;
      for (std::uint16_t i = 0;
           i < count && (i + 1) * sizeof(OfficeArtFopte) <= child->recLen;
           ++i) {
        const auto property = util::byte_stream::read<OfficeArtFopte>(in);
        children.consume(sizeof(OfficeArtFopte));
        if (property.fComplex != 0) {
          continue;
        }
        // A real picture shape (pib) wins over a picture fill (fillBlip).
        if (property.opid == office_art_property_pib ||
            (property.opid == office_art_property_fill_blip &&
             !shape.blip_ref.has_value())) {
          shape.blip_ref = property.op;
        }
      }
    }
    // Other children (shapeProp, clientData, …): skipped by the cursor.
  }
  return shape;
}

/// Reads a slide's shapes in z order; shapes with neither text nor a picture
/// are dropped. First cut: only top-level shapes, whose anchors are already
/// in the slide's master-unit coordinates. Stream at the SlideContainer body.
std::vector<Shape>
read_slide_shapes(std::istream &in, const RecordHeader &slide,
                  const std::vector<StyledText> &outline_texts,
                  StyleContext &context) {
  // The drawing holds a mandatory OfficeArtSpgrContainer plus optionally a
  // shape not in any group ([MS-ODRAW] 2.2.13), read in stream (z) order.
  const RecordHeader drawing = require_child(in, slide, RT_Drawing);
  const RecordHeader dg = require_child(in, drawing, RT_OfficeArtDgContainer);

  std::vector<Shape> shapes;
  const auto keep = [&shapes](Shape shape) {
    if (!shape.text.empty() || shape.blip_ref.has_value()) {
      shapes.push_back(std::move(shape));
    }
  };

  bool seen_group = false;
  ChildCursor children(in, dg);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_OfficeArtSpgrContainer) {
      seen_group = true;
      ChildCursor group(in, *child);
      while (const std::optional<RecordHeader> shape = group.next()) {
        if (shape->recType != RT_OfficeArtSpContainer) {
          continue; // not a shape; the cursor skips it
        }
        keep(read_shape(in, *shape, outline_texts, context));
        group.consume(shape->recLen);
      }
      children.consume(child->recLen);
    } else if (child->recType == RT_OfficeArtSpContainer) {
      keep(read_shape(in, *child, outline_texts, context));
      children.consume(child->recLen);
    }
    // Other children: skipped by the cursor.
  }
  if (!seen_group) {
    throw std::runtime_error("ppt: missing required record type " +
                             std::to_string(RT_OfficeArtSpgrContainer));
  }
  return shapes;
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
/// order, plus per slide (by persistIdRef) the styled text of each
/// TextHeaderAtom block, indexed by OutlineTextRefAtom ([MS-PPT] 2.4.14.3 /
/// 2.9.78).
struct SlideListText {
  std::vector<std::uint32_t> persist_ids;
  std::unordered_map<std::uint32_t, std::vector<StyledText>> outline_texts;
};

/// Walks a SlideListWithText container once. Each SlidePersistAtom starts a
/// slide; the following TextHeaderAtoms are its outline-text blocks, each
/// filled by its following TextChars/TextBytes atom plus the optional
/// StyleTextPropAtom after it. Stream at the container body.
SlideListText read_slide_list_text(std::istream &in,
                                   const RecordHeader &slide_list,
                                   StyleContext &context) {
  constexpr std::uint32_t persist_ref_size = sizeof(std::uint32_t);

  SlideListText result;
  // Outline texts of the slide being read; unordered_map keeps the pointer
  // valid across inserts.
  std::vector<StyledText> *current = nullptr;
  PendingText pending;
  const auto flush = [&](const std::vector<TextCFRun> &runs) {
    if (pending.has_value()) {
      if (current != nullptr && !current->empty()) {
        current->back() = style_pending(pending, runs, context);
      }
      pending.reset();
    }
  };

  ChildCursor children(in, slide_list);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_SlidePersistAtom &&
        child->recLen >= persist_ref_size) {
      flush({});
      const std::uint32_t persist_id = read_u32(in); // persistIdRef is first
      children.consume(persist_ref_size);
      result.persist_ids.push_back(persist_id);
      current = &result.outline_texts[persist_id];
    } else if (child->recType == RT_TextHeaderAtom) {
      flush({});
      if (current != nullptr) {
        current->emplace_back(); // one block per header; text filled in below
      }
    } else if (child->recType == RT_TextCharsAtom ||
               child->recType == RT_TextBytesAtom) {
      flush({});
      if (current != nullptr && !current->empty()) {
        read_pending_text(in, *child, pending);
        children.consume(child->recLen);
      }
    } else if (child->recType == RT_StyleTextPropAtom) {
      const std::vector<TextCFRun> runs = read_style_atom(in, *child, pending);
      children.consume(child->recLen);
      flush(runs);
    }
    // Everything else (meta atoms, …): left for the cursor to skip.
  }
  flush({});
  return result;
}

/// Bytes between a JPEG/PNG BLIP record's header and its picture data:
/// rgbUid1, an optional rgbUid2 marked by the recInstance, and a 1-byte tag
/// ([MS-ODRAW] 2.2.27/2.2.28).
std::uint32_t blip_prefix_size(const RecordHeader &header) {
  const bool two_uids =
      header.recType == RT_OfficeArtBlipJPEG
          ? (header.recInstance == BlipInstance_JpegRgbTwoUids ||
             header.recInstance == BlipInstance_JpegCmykTwoUids)
          : (header.recInstance == BlipInstance_PngTwoUids);
  constexpr std::uint32_t uid_size = 16;
  return uid_size + (two_uids ? uid_size : 0) + 1;
}

/// Reads an OfficeArt BLIP record ([MS-ODRAW] 2.2.23) at the stream position
/// and returns its image file bytes; empty for BLIP types not modelled
/// (WMF/EMF/PICT/DIB/TIFF).
std::string read_blip_record(std::istream &in) {
  const RecordHeader header = read_record_header(in);

  if (header.recType != RT_OfficeArtBlipJPEG &&
      header.recType != RT_OfficeArtBlipPNG) {
    in.ignore(header.recLen);
    return {};
  }
  const std::uint32_t prefix = blip_prefix_size(header);
  if (header.recLen < prefix) {
    throw std::runtime_error("ppt: truncated BLIP record");
  }
  in.ignore(prefix);
  return util::stream::read(in, header.recLen - prefix);
}

/// One slot of the BLIP store: the offset of the BLIP in the "Pictures"
/// stream (0xFFFFFFFF = none), or its already-extracted bytes when the BLIP
/// is embedded in the store itself.
struct BlipSlot final {
  std::uint32_t fo_delay{0xFFFFFFFF};
  std::string data;
};

/// Reads the BLIP store ([MS-ODRAW] 2.2.20) of the DocumentContainer's
/// drawing group; one slot per store entry, in order (pib is a one-based
/// index into these). Stream at the DocumentContainer body.
std::vector<BlipSlot> read_blip_store(std::istream &in,
                                      const RecordHeader &document) {
  std::vector<BlipSlot> slots;
  const std::optional<RecordHeader> drawing_group =
      find_child(in, document, RT_DrawingGroup);
  if (!drawing_group.has_value()) {
    return slots;
  }
  const std::optional<RecordHeader> dgg =
      find_child(in, *drawing_group, RT_OfficeArtDggContainer);
  if (!dgg.has_value()) {
    return slots;
  }
  const std::optional<RecordHeader> store =
      find_child(in, *dgg, RT_OfficeArtBStoreContainer);
  if (!store.has_value()) {
    return slots;
  }

  ChildCursor entries(in, *store);
  while (const std::optional<RecordHeader> child = entries.next()) {
    BlipSlot &slot = slots.emplace_back();
    if (child->recType == RT_OfficeArtFBSE) {
      const auto fbse = util::byte_stream::read<OfficeArtFbseFixed>(in);
      entries.consume(sizeof(OfficeArtFbseFixed));
      in.ignore(fbse.cbName);
      entries.consume(fbse.cbName);
      // An embedded BLIP follows the name ([MS-ODRAW] 2.2.32); otherwise the
      // BLIP lives in the delay ("Pictures") stream at foDelay.
      if (child->recLen >
          sizeof(OfficeArtFbseFixed) + std::uint32_t{fbse.cbName}) {
        const std::uint32_t remaining =
            child->recLen - sizeof(OfficeArtFbseFixed) - fbse.cbName;
        slot.data = read_blip_record(in);
        entries.consume(remaining);
      } else {
        slot.fo_delay = fbse.foDelay;
      }
    } else if (child->recType == RT_OfficeArtBlipJPEG ||
               child->recType == RT_OfficeArtBlipPNG) {
      // A BLIP directly in the store occupies a slot of its own; its header is
      // already consumed, so the body is read here rather than via
      // read_blip_record.
      const std::uint32_t prefix = blip_prefix_size(*child);
      if (child->recLen < prefix) {
        throw std::runtime_error("ppt: truncated BLIP record");
      }
      in.ignore(prefix);
      slot.data = util::stream::read(in, child->recLen - prefix);
      entries.consume(child->recLen);
    }
    // Other record types leave an empty slot; the cursor skips them.
  }
  return slots;
}

/// Reads the document's font names from the FontCollection
/// ([MS-PPT] 2.9.8/2.9.10), indexed by each FontEntityAtom's recInstance
/// (empty strings mark the gaps). Stream at the DocumentContainer body.
std::vector<std::string> read_font_collection(std::istream &in,
                                              const RecordHeader &document) {
  std::vector<std::string> fonts;
  const std::optional<RecordHeader> environment =
      find_child(in, document, RT_Environment);
  if (!environment.has_value()) {
    return fonts;
  }
  const std::optional<RecordHeader> collection =
      find_child(in, *environment, RT_FontCollection);
  if (!collection.has_value()) {
    return fonts;
  }

  ChildCursor entries(in, *collection);
  while (const std::optional<RecordHeader> child = entries.next()) {
    if (child->recType != RT_FontEntityAtom) {
      continue;
    }
    // lfFaceName: 32 UTF-16 units, zero-terminated ([MS-PPT] 2.9.10).
    if (child->recLen < 64) {
      throw std::runtime_error("ppt: truncated FontEntityAtom");
    }
    std::u16string name = read_raw_text_chars(in, 64);
    entries.consume(64);
    if (const std::size_t nul = name.find(u'\0'); nul != std::u16string::npos) {
      name.resize(nul);
    }

    const std::size_t index = child->recInstance;
    if (fonts.size() <= index) {
      fonts.resize(index + 1);
    }
    fonts[index] = util::string::u16string_to_string(name);
  }
  return fonts;
}

/// Resolves the presentation's slides via the [MS-PPT] 2.1.2 reading
/// algorithm: "Current User" → UserEditAtom chain → persist directory → the
/// live DocumentContainer and each SlideContainer. Returns each slide's shapes
/// in z order; their styles accumulate in `context`.
std::vector<std::vector<Shape>>
collect_slides(std::istream &current_user, std::istream &document,
               const abstract::ReadableFilesystem &files,
               ElementRegistry &registry, StyleContext &context) {
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

    // offsetLastEdit MUST strictly decrease (0 ends the chain), so this also
    // breaks a looping chain.
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
  const std::uint32_t doc_offset = doc_it->second;

  // The font names must be complete before the first style is resolved — the
  // styles' `font_name` point into them.
  {
    document.clear();
    document.seekg(doc_offset);
    const RecordHeader doc_header = read_header(document, RT_DocumentContainer);
    context.fonts = read_font_collection(document, doc_header);
  }

  // Slide size from the DocumentAtom ([MS-PPT] 2.4.2): a PointStruct in
  // master units.
  {
    document.clear();
    document.seekg(doc_offset);
    const RecordHeader doc_header = read_header(document, RT_DocumentContainer);
    if (const std::optional<RecordHeader> document_atom =
            find_child(document, doc_header, RT_DocumentAtom);
        document_atom.has_value() &&
        document_atom->recLen >= 2 * sizeof(std::int32_t)) {
      const auto width = static_cast<std::int32_t>(read_u32(document));
      const auto height = static_cast<std::int32_t>(read_u32(document));
      registry.set_slide_size(width, height);
    }
  }

  // The BLIP store, for picture shapes.
  std::vector<BlipSlot> blip_store;
  {
    document.clear();
    document.seekg(doc_offset);
    const RecordHeader doc_header = read_header(document, RT_DocumentContainer);
    blip_store = read_blip_store(document, doc_header);
  }

  document.clear();
  document.seekg(doc_offset);
  const RecordHeader doc_header = read_header(document, RT_DocumentContainer);

  // The presentation slide list (recInstance Slides), then its slides' persist
  // ids in presentation order.
  const std::optional<RecordHeader> slide_list = find_child(
      document, doc_header, RT_SlideListWithText, SlideListInstance_Slides);
  if (!slide_list.has_value()) {
    return {}; // valid document, no presentation slides
  }
  const SlideListText slide_list_text =
      read_slide_list_text(document, *slide_list, context);

  // Each SlidePersistAtom references a SlideContainer by persist id (which the
  // spec requires the directory to resolve); read its shapes, passing the
  // slide's outline texts so OutlineTextRefAtom shapes can be resolved.
  static const std::vector<StyledText> no_outline_texts;
  std::vector<std::vector<Shape>> slides;
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
    const std::vector<StyledText> &outline_texts =
        ot != slide_list_text.outline_texts.end() ? ot->second
                                                  : no_outline_texts;
    slides.push_back(
        read_slide_shapes(document, slide_header, outline_texts, context));
  }

  // Resolve picture references against the BLIP store and the "Pictures"
  // (delay) stream; unsupported/unresolvable pictures leave `image` empty.
  const auto pictures_file = files.open(AbsPath("/Pictures"));
  const auto pictures_stream =
      pictures_file != nullptr ? pictures_file->stream() : nullptr;
  for (std::vector<Shape> &shapes : slides) {
    for (Shape &shape : shapes) {
      if (!shape.blip_ref.has_value() || *shape.blip_ref == 0 ||
          *shape.blip_ref > blip_store.size()) {
        continue;
      }
      BlipSlot &slot = blip_store[*shape.blip_ref - 1];
      if (slot.data.empty() && slot.fo_delay != 0xFFFFFFFF &&
          pictures_stream != nullptr) {
        pictures_stream->clear();
        pictures_stream->seekg(slot.fo_delay);
        slot.data = read_blip_record(*pictures_stream);
        slot.fo_delay = 0xFFFFFFFF; // resolved (possibly to unsupported/empty)
      }
      shape.image = slot.data;
      if (!shape.image.empty()) {
        // Only JPEG and PNG BLIPs are modelled; tell them apart by magic.
        const bool is_png = shape.image.starts_with("\x89PNG");
        shape.image_href = "Pictures/" + std::to_string(*shape.blip_ref) +
                           (is_png ? ".png" : ".jpg");
      }
    }

    // Background shapes cover the whole slide (they carry no anchor of their
    // own) and must render below the other shapes.
    for (Shape &shape : shapes) {
      if (shape.is_background && !shape.anchor.has_value()) {
        const auto size = registry.slide_size().value_or(
            std::pair<std::int32_t, std::int32_t>{5760, 4320});
        shape.anchor = Anchor{0, 0, size.first, size.second};
      }
    }
    std::ranges::stable_partition(
        shapes, [](const Shape &shape) { return shape.is_background; });
  }
  return slides;
}

} // namespace
} // namespace odr::internal::oldms::presentation

namespace odr::internal::oldms {

ElementIdentifier
presentation::parse_tree(ElementRegistry &registry,
                         StyleRegistry &style_registry,
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

  StyleContext context;
  for (std::vector<Shape> &shapes : collect_slides(
           *current_user_stream, *document_stream, files, registry, context)) {
    auto [slide_id, _] = registry.create_element(ElementType::slide);
    registry.append_child(root_id, slide_id);

    // One frame per shape; a picture and/or the shape's paragraphs hang off
    // the frame.
    for (Shape &shape : shapes) {
      auto [frame_id, frame_element, frame] = registry.create_frame_element();
      frame.anchor = shape.anchor;
      registry.append_child(slide_id, frame_id);
      if (!shape.image.empty()) {
        auto [image_id, image_element, image] = registry.create_image_element();
        image.data = std::move(shape.image);
        image.href = std::move(shape.image_href);
        registry.append_child(frame_id, image_id);
      }
      build_paragraphs(registry, frame_id, shape.text);
    }
  }

  // The styles' `font_name` point into the font-name strings, which keep
  // their buffers when the vectors are moved into the registry.
  style_registry =
      StyleRegistry(std::move(context.fonts), std::move(context.styles));

  return root_id;
}

std::optional<bool>
presentation::password_encrypted(const abstract::ReadableFilesystem &files) {
  const std::shared_ptr<abstract::File> file =
      files.open(AbsPath("/Current User"));
  if (file == nullptr) {
    return {};
  }

  const std::unique_ptr<std::istream> stream = file->stream();
  CurrentUserAtomHead head{};
  stream->read(reinterpret_cast<char *>(&head), sizeof(head));
  if (stream->gcount() != sizeof(head) ||
      head.rh.recType != RT_CurrentUserAtom) {
    return {};
  }

  return head.headerToken == current_user_token_encrypted;
}

} // namespace odr::internal::oldms
