#include <odr/internal/oldms/presentation/ppt_parser.hpp>

#include <odr/style.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/presentation/ppt_element_registry.hpp>
#include <odr/internal/oldms/presentation/ppt_io.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
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
    const auto uc = static_cast<std::uint8_t>(c);
    // Keep tab and any non-control byte (>= 0x20, including UTF-8 lead and
    // continuation bytes); drop the remaining control characters.
    if (uc < 0x20 && c != '\x09') {
      continue;
    }
    out.push_back(c);
  }
  return out;
}

/// One run of uniformly formatted text within a text box.
struct StyledRun final {
  std::string text; //< UTF-8
  TextStyle style;
};
using StyledText = std::vector<StyledRun>;

/// What character-formatting resolution needs: the document's font names
/// (interned) and the style of unformatted text.
struct StyleContext final {
  std::vector<const char *> fonts;
  TextStyle default_style;
};

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

/// A character run's formatting on top of the default style.
TextStyle resolve_style(const TextCFRun &run, const StyleContext &context) {
  TextStyle style = context.default_style;
  if (run.bold.has_value()) {
    style.font_weight = *run.bold ? FontWeight::bold : FontWeight::normal;
  }
  if (run.italic.has_value()) {
    style.font_style = *run.italic ? FontStyle::italic : FontStyle::normal;
  }
  if (run.underline.has_value()) {
    style.font_underline = *run.underline;
  }
  if (run.font_size.has_value()) {
    style.font_size =
        Measure(static_cast<double>(*run.font_size), DynamicUnit("pt"));
  }
  if (run.color.has_value()) {
    style.font_color = *run.color;
  }
  if (run.font_ref.has_value()) {
    if (*run.font_ref >= context.fonts.size() ||
        context.fonts[*run.font_ref] == nullptr) {
      throw std::runtime_error("ppt: font reference out of range");
    }
    style.font_name = context.fonts[*run.font_ref];
  }
  return style;
}

/// Splits a pending text at its character-run boundaries; characters past the
/// last run (or all of them, without a StyleTextPropAtom) get the default
/// style. Empty runs are dropped.
StyledText style_pending(const PendingText &pending,
                         const std::vector<TextCFRun> &runs,
                         const StyleContext &context) {
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
      result.push_back({std::move(text), context.default_style});
    }
  }
  return result;
}

/// A single text box or picture shape on a slide: its styled text, its
/// picture reference/bytes, and optionally the position and size from its
/// OfficeArtClientAnchor.
struct TextBox final {
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

/// Builds the paragraph/span/text subtree of one text box under `parent_id`.
/// Paragraphs open lazily (the trailing paragraph mark adds no empty
/// paragraph); each span and paragraph stores its style so empty paragraphs
/// keep their height.
void build_paragraphs(ElementRegistry &registry,
                      const ElementIdentifier parent_id,
                      const StyledText &box_text) {
  ElementIdentifier paragraph_id = null_element_id;

  const auto ensure_paragraph = [&](const TextStyle &style) {
    if (paragraph_id == null_element_id) {
      auto [id, paragraph] = registry.create_element(ElementType::paragraph);
      registry.append_child(parent_id, id);
      registry.set_element_style(id, style);
      paragraph_id = id;
    }
  };

  for (const StyledRun &run : box_text) {
    std::size_t at = 0;
    while (at <= run.text.size()) {
      const std::size_t control = run.text.find_first_of("\x0D\x0B", at);
      const std::size_t segment_end =
          control == std::string::npos ? run.text.size() : control;

      if (std::string cleaned =
              clean_text(run.text.substr(at, segment_end - at));
          !cleaned.empty()) {
        ensure_paragraph(run.style);
        auto [span_id, span] = registry.create_element(ElementType::span);
        registry.set_element_style(span_id, run.style);
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
        ensure_paragraph(run.style);
        paragraph_id = null_element_id;
      } else { // line_break_mark
        ensure_paragraph(run.style);
        auto [line_id, line] = registry.create_element(ElementType::line_break);
        registry.append_child(paragraph_id, line_id);
      }
      at = control + 1;
    }
  }
}

/// Appends a text block to a text box's running text, separating consecutive
/// blocks (e.g. title vs. body) with a paragraph break.
void append_block(StyledText &slide_text, StyledText block,
                  const StyleContext &context) {
  if (block.empty()) {
    return;
  }
  if (!slide_text.empty()) {
    slide_text.push_back(
        {std::string(1, paragraph_mark), context.default_style});
  }
  slide_text.insert(slide_text.end(), std::make_move_iterator(block.begin()),
                    std::make_move_iterator(block.end()));
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

/// Recursively concatenates a container's styled text in stream order. A box
/// holds inline TextChars/TextBytes atoms — each optionally followed by a
/// StyleTextPropAtom ([MS-PPT] 2.9.44) — or an OutlineTextRefAtom indexing
/// the slide's `outline_texts` ([MS-PPT] 2.9.78). Stream at the container
/// body.
void gather_text(std::istream &in, const RecordHeader &container,
                 StyledText &slide_text,
                 const std::vector<StyledText> &outline_texts,
                 const StyleContext &context) {
  PendingText pending;
  const auto flush = [&](const std::vector<TextCFRun> &runs) {
    if (pending.has_value()) {
      append_block(slide_text, style_pending(pending, runs, context), context);
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
        append_block(slide_text, outline_texts[index], context);
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
TextBox read_shape(std::istream &in, const RecordHeader &shape,
                   const std::vector<StyledText> &outline_texts,
                   const StyleContext &context) {
  TextBox box;
  ChildCursor children(in, shape);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_OfficeArtClientAnchor) {
      box.anchor = read_client_anchor(in, child->recLen);
      children.consume(child->recLen);
    } else if (child->recType == RT_OfficeArtFSP &&
               child->recLen >= 2 * sizeof(std::uint32_t)) {
      read_u32(in); // spid
      const std::uint32_t flags = read_u32(in);
      children.consume(2 * sizeof(std::uint32_t));
      box.is_background = (flags & office_art_fsp_background) != 0;
    } else if (child->recType == RT_OfficeArtClientTextbox) {
      gather_text(in, *child, box.text, outline_texts, context);
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
        if (property.opid == office_art_property_pib) {
          box.blip_ref = property.op;
        } else if (property.opid == office_art_property_fill_blip &&
                   !box.blip_ref.has_value()) {
          box.blip_ref = property.op;
        }
      }
    }
    // Other children (shapeProp, clientData, …): skipped by the cursor.
  }
  return box;
}

/// Reads a slide's text boxes in shape (z) order; empty boxes are dropped.
/// First cut: only top-level shapes, whose anchors are already in the slide's
/// master-unit coordinates. Stream at the SlideContainer body.
std::vector<TextBox>
read_slide_text_boxes(std::istream &in, const RecordHeader &slide,
                      const std::vector<StyledText> &outline_texts,
                      const StyleContext &context) {
  // SlideContainer → DrawingContainer → OfficeArtDgContainer (mandatory).
  // The drawing holds a mandatory OfficeArtSpgrContainer plus optionally a
  // direct shape not in any group ([MS-ODRAW] 2.2.13); both are read in
  // stream (z) order.
  const RecordHeader drawing = require_child(in, slide, RT_Drawing);
  const RecordHeader dg = require_child(in, drawing, RT_OfficeArtDgContainer);

  std::vector<TextBox> boxes;
  const auto keep = [&boxes](TextBox box) {
    if (!box.text.empty() || box.blip_ref.has_value()) {
      boxes.push_back(std::move(box));
    }
  };

  bool seen_group = false;
  ChildCursor children(in, dg);
  while (const std::optional<RecordHeader> child = children.next()) {
    if (child->recType == RT_OfficeArtSpgrContainer) {
      seen_group = true;
      ChildCursor shapes(in, *child);
      while (const std::optional<RecordHeader> shape = shapes.next()) {
        if (shape->recType != RT_OfficeArtSpContainer) {
          continue; // not a shape; the cursor skips it
        }
        keep(read_shape(in, *shape, outline_texts, context));
        shapes.consume(shape->recLen);
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
                                   const StyleContext &context) {
  constexpr std::uint32_t persist_ref_size = sizeof(std::uint32_t);

  SlideListText result;
  // Outline texts of the slide currently being read; valid until reassigned at
  // the next SlidePersistAtom (unordered_map keeps references stable on
  // insert).
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

/// Reads an OfficeArt BLIP record ([MS-ODRAW] 2.2.23) at the stream position
/// and returns its image file bytes; empty for BLIP types not modelled
/// (WMF/EMF/PICT/DIB/TIFF).
std::string read_blip_record(std::istream &in) {
  const RecordHeader header = read_record_header(in);

  // JPEG/PNG: rgbUid1, optional rgbUid2 (odd data recInstance), tag, data
  // ([MS-ODRAW] 2.2.27/2.2.28).
  std::uint32_t prefix;
  if (header.recType == RT_OfficeArtBlipJPEG) {
    const bool two_uids =
        header.recInstance == 0x46B || header.recInstance == 0x6E3;
    prefix = 16 + (two_uids ? 16 : 0) + 1;
  } else if (header.recType == RT_OfficeArtBlipPNG) {
    prefix = 16 + (header.recInstance == 0x6E1 ? 16 : 0) + 1;
  } else {
    in.ignore(header.recLen);
    return {};
  }
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
      // A BLIP directly in the store occupies a slot of its own; rewind is
      // impossible on this stream, so re-parse from the header we just read.
      // The header was already consumed by the cursor; read body inline.
      const bool two_uids =
          child->recType == RT_OfficeArtBlipJPEG
              ? (child->recInstance == 0x46B || child->recInstance == 0x6E3)
              : (child->recInstance == 0x6E1);
      const std::uint32_t prefix = 16 + (two_uids ? 16 : 0) + 1;
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
/// ([MS-PPT] 2.9.8/2.9.10), indexed by each FontEntityAtom's recInstance and
/// interned in the registry. Stream at the DocumentContainer body.
std::vector<const char *> read_font_collection(std::istream &in,
                                               const RecordHeader &document,
                                               ElementRegistry &registry) {
  std::vector<const char *> fonts;
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
      fonts.resize(index + 1, nullptr);
    }
    fonts[index] =
        registry.intern_font_name(util::string::u16string_to_string(name));
  }
  return fonts;
}

/// Resolves the presentation slides via the [MS-PPT] reading algorithm (the
/// only spec-defined path): the "Current User" stream points at the newest
/// UserEditAtom, whose chain builds the persist directory, which resolves the
/// live DocumentContainer and each SlideContainer — so slides come out in order
/// from the live records, ignoring stale copies left by incremental saves.
/// Malformed records throw. Returns each slide's text boxes in shape order.
std::vector<std::vector<TextBox>>
collect_slides(std::istream &current_user, std::istream &document,
               const abstract::ReadableFilesystem &files,
               ElementRegistry &registry) {
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
  const std::uint32_t doc_offset = doc_it->second;

  // Fonts and unformatted-text default. The default font size approximates
  // the (unread) master text styles ([MS-PPT] 2.9.36 TextMasterStyleAtom).
  StyleContext context;
  context.default_style.font_size = Measure(18.0, DynamicUnit("pt"));
  {
    document.clear();
    document.seekg(doc_offset);
    const RecordHeader doc_header = read_header(document, RT_DocumentContainer);
    context.fonts = read_font_collection(document, doc_header, registry);
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
  // spec requires the directory to resolve); read its text boxes, passing the
  // slide's outline texts so OutlineTextRefAtom boxes can be resolved.
  static const std::vector<StyledText> no_outline_texts;
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
    const std::vector<StyledText> &outline_texts =
        ot != slide_list_text.outline_texts.end() ? ot->second
                                                  : no_outline_texts;
    slides.push_back(
        read_slide_text_boxes(document, slide_header, outline_texts, context));
  }

  // Resolve picture references against the BLIP store and the "Pictures"
  // (delay) stream; unsupported/unresolvable pictures leave `image` empty.
  const auto pictures_file = files.open(AbsPath("/Pictures"));
  const auto pictures_stream =
      pictures_file != nullptr ? pictures_file->stream() : nullptr;
  for (std::vector<TextBox> &boxes : slides) {
    for (TextBox &box : boxes) {
      if (!box.blip_ref.has_value() || *box.blip_ref == 0 ||
          *box.blip_ref > blip_store.size()) {
        continue;
      }
      BlipSlot &slot = blip_store[*box.blip_ref - 1];
      if (slot.data.empty() && slot.fo_delay != 0xFFFFFFFF &&
          pictures_stream != nullptr) {
        pictures_stream->clear();
        pictures_stream->seekg(slot.fo_delay);
        slot.data = read_blip_record(*pictures_stream);
        slot.fo_delay = 0xFFFFFFFF; // resolved (possibly to unsupported/empty)
      }
      box.image = slot.data;
      if (!box.image.empty()) {
        // Only JPEG and PNG BLIPs are modelled; tell them apart by magic.
        const bool is_png = box.image.starts_with("\x89PNG");
        box.image_href = "Pictures/" + std::to_string(*box.blip_ref) +
                         (is_png ? ".png" : ".jpg");
      }
    }

    // Background shapes cover the whole slide (they carry no anchor of their
    // own) and must render below the other shapes.
    for (TextBox &box : boxes) {
      if (box.is_background && !box.anchor.has_value()) {
        const auto size = registry.slide_size().value_or(
            std::pair<std::int32_t, std::int32_t>{5760, 4320});
        box.anchor = Anchor{0, 0, size.first, size.second};
      }
    }
    std::ranges::stable_partition(
        boxes, [](const TextBox &box) { return box.is_background; });
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

  for (std::vector<TextBox> &boxes : collect_slides(
           *current_user_stream, *document_stream, files, registry)) {
    auto [slide_id, _] = registry.create_element(ElementType::slide);
    registry.append_child(root_id, slide_id);

    // One frame per shape; a picture and/or the box's paragraphs hang off
    // the frame.
    for (TextBox &box : boxes) {
      auto [frame_id, frame_element, frame] = registry.create_frame_element();
      frame.anchor = box.anchor;
      registry.append_child(slide_id, frame_id);
      if (!box.image.empty()) {
        auto [image_id, image_element, image] = registry.create_image_element();
        image.data = std::move(box.image);
        image.href = std::move(box.image_href);
        registry.append_child(frame_id, image_id);
      }
      build_paragraphs(registry, frame_id, box.text);
    }
  }

  return root_id;
}

} // namespace odr::internal::oldms
