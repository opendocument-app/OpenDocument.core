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
#include <iterator>
#include <optional>
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

// A record's header together with the absolute stream offset of its body.
struct Record {
  RecordHeader header;
  std::streamoff body_begin{0};
};

// Reads the immediate child records contained in the byte range [begin, end).
// Each returned record's body spans [body_begin, body_begin + recLen). Stops
// early on a malformed/truncated record rather than throwing.
std::vector<Record> read_children(std::istream &in, const std::streamoff begin,
                                  const std::streamoff end) {
  std::vector<Record> result;

  std::streamoff pos = begin;
  while (pos + static_cast<std::streamoff>(sizeof(RecordHeader)) <= end) {
    in.clear();
    in.seekg(pos);

    Record record;
    read(in, record.header);
    if (!in) {
      break;
    }
    record.body_begin = pos + static_cast<std::streamoff>(sizeof(RecordHeader));

    const std::streamoff body_end =
        record.body_begin + static_cast<std::streamoff>(record.header.recLen);
    if (body_end > end) {
      break; // truncated record
    }

    result.push_back(record);
    pos = body_end;
  }

  return result;
}

// Seeks to an absolute stream offset and reads a record header. Returns false
// if the read fails (offset past EOF, truncated header, …).
bool read_header_at(std::istream &in, const std::streamoff offset,
                    RecordHeader &out) {
  in.clear();
  in.seekg(offset);
  read(in, out);
  return static_cast<bool>(in);
}

// Finds the first immediate child with the given record type, optionally also
// matching recInstance. Returns nullopt if none is found.
std::optional<Record>
find_child(const std::vector<Record> &children, const std::uint16_t rec_type,
           const std::optional<std::uint16_t> rec_instance = std::nullopt) {
  for (const Record &record : children) {
    if (record.header.recType != rec_type) {
      continue;
    }
    if (rec_instance.has_value() &&
        record.header.rec_instance() != *rec_instance) {
      continue;
    }
    return record;
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
std::string read_text_atom(std::istream &in, const Record &record) {
  in.clear();
  in.seekg(record.body_begin);
  if (record.header.recType == RT_TextBytesAtom) {
    return read_text_bytes(in, record.header.recLen);
  }
  return read_text_chars(in, record.header.recLen);
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

// Recursively gathers all text atoms in [begin, end), in stream order, into one
// string. Used to collect the text drawn on a single slide.
void gather_text(std::istream &in, const std::streamoff begin,
                 const std::streamoff end, std::string &slide_text) {
  for (const Record &record : read_children(in, begin, end)) {
    if (record.header.recType == RT_TextCharsAtom ||
        record.header.recType == RT_TextBytesAtom) {
      append_text(slide_text, read_text_atom(in, record));
    } else if (record.header.is_container()) {
      const std::streamoff record_end =
          record.body_begin + static_cast<std::streamoff>(record.header.recLen);
      gather_text(in, record.body_begin, record_end, slide_text);
    }
  }
}

// Option 1: outline text from the slide list (RT_SlideListWithText, instance
// Slides). One slide per SlidePersistAtom; robust and order-correct but misses
// text drawn directly on slides. Empty for presentations that do not populate
// the outline (e.g. LibreOffice exports).
std::vector<std::string>
collect_outline_slides(std::istream &in,
                       const std::vector<Record> &document_children) {
  std::optional<Record> slide_list = find_child(
      document_children, RT_SlideListWithText, SlideListInstance_Slides);
  if (!slide_list.has_value()) {
    slide_list = find_child(document_children, RT_SlideListWithText);
  }
  if (!slide_list.has_value()) {
    return {};
  }

  const std::streamoff slide_list_end =
      slide_list->body_begin +
      static_cast<std::streamoff>(slide_list->header.recLen);

  std::vector<std::string> slides;
  for (const Record &record :
       read_children(in, slide_list->body_begin, slide_list_end)) {
    switch (record.header.recType) {
    case RT_SlidePersistAtom:
      slides.emplace_back();
      break;
    case RT_TextCharsAtom:
    case RT_TextBytesAtom:
      if (!slides.empty()) {
        append_text(slides.back(), read_text_atom(in, record));
      }
      break;
    default:
      break;
    }
  }
  return slides;
}

// Option 2: walk every SlideContainer (RT_SlideContainer) and gather the text
// atoms beneath it. Catches text boxes outside placeholders; slide order is
// stream order (presentation order requires the persist directory, a
// follow-up).
std::vector<std::string> collect_container_slides(std::istream &in,
                                                  const std::streamoff begin,
                                                  const std::streamoff end) {
  std::vector<std::string> slides;
  for (const Record &record : read_children(in, begin, end)) {
    if (!record.header.is_container()) {
      continue;
    }
    const std::streamoff record_end =
        record.body_begin + static_cast<std::streamoff>(record.header.recLen);
    if (record.header.recType == RT_SlideContainer) {
      std::string slide_text;
      gather_text(in, record.body_begin, record_end, slide_text);
      slides.push_back(std::move(slide_text));
    } else {
      // Slides live at the top level, but recurse generically to be safe.
      std::vector<std::string> nested =
          collect_container_slides(in, record.body_begin, record_end);
      slides.insert(slides.end(), std::make_move_iterator(nested.begin()),
                    std::make_move_iterator(nested.end()));
    }
  }
  return slides;
}

std::size_t total_text(const std::vector<std::string> &slides) {
  std::size_t total = 0;
  for (const std::string &slide : slides) {
    total += slide.size();
  }
  return total;
}

// Maps a persist object identifier to its offset in the PowerPoint Document
// stream. See [MS-PPT] 2.3.4 "persist object directory".
using PersistDirectory = std::unordered_map<std::uint32_t, std::uint32_t>;

// Parses the PersistDirectoryAtom at the given offset and adds its
// (persistId -> offset) pairs to the directory. Existing entries are kept
// (insert-if-absent), so when edits are processed newest-first the newest
// offset for each id wins, as required by [MS-PPT] step 8 of the reading
// algorithm.
void read_persist_directory(std::istream &in, const std::uint32_t offset,
                            PersistDirectory &directory) {
  RecordHeader header{};
  if (!read_header_at(in, static_cast<std::streamoff>(offset), header) ||
      header.recType != RT_PersistDirectoryAtom) {
    return;
  }
  const std::streamoff begin =
      static_cast<std::streamoff>(offset) +
      static_cast<std::streamoff>(sizeof(RecordHeader));
  const std::streamoff end = begin + static_cast<std::streamoff>(header.recLen);

  std::streamoff pos = begin;
  while (pos + 4 <= end) {
    in.clear();
    in.seekg(pos);
    const std::uint32_t entry = read_u32(in); // persistId:20 | cPersist:12
    if (!in) {
      return;
    }
    pos += 4;
    const std::uint32_t persist_id = entry & 0x000FFFFF;
    const std::uint32_t count = entry >> 20;
    for (std::uint32_t i = 0; i < count && pos + 4 <= end; ++i) {
      const std::uint32_t persist_offset = read_u32(in);
      if (!in) {
        return;
      }
      pos += 4;
      directory.emplace(persist_id + i, persist_offset);
    }
  }
}

// The spec-correct path ([MS-PPT] reading algorithm): resolve the live
// DocumentContainer and the presentation slides through the persist object
// directory, so slides come out in presentation order regardless of where their
// SlideContainer records sit in the stream. Returns nullopt if the persist
// structures cannot be established (missing "Current User" stream, malformed
// offsets, …), in which case the caller falls back to a plain stream scan.
std::optional<std::vector<std::string>>
collect_persist_ordered_slides(const abstract::ReadableFilesystem &files,
                               std::istream &in) {
  const AbsPath current_user_path("/Current User");
  if (!files.is_file(current_user_path)) {
    return std::nullopt;
  }

  // Newest user edit offset, from the Current User stream.
  std::uint32_t current_edit = 0;
  {
    const auto current_user = files.open(current_user_path)->stream();
    CurrentUserAtomHead head{};
    read(*current_user, head);
    if (!*current_user || head.rh.recType != RT_CurrentUserAtom) {
      return std::nullopt;
    }
    current_edit = head.offsetToCurrentEdit;
  }

  // Walk the UserEditAtom chain newest -> oldest, accumulating the persist
  // directory (newest wins) and capturing the live document's persist id.
  PersistDirectory directory;
  std::uint32_t doc_persist_id = 0;
  std::uint32_t edit_offset = current_edit;
  for (bool first = true; edit_offset != 0; first = false) {
    RecordHeader edit_header{};
    if (!read_header_at(in, static_cast<std::streamoff>(edit_offset),
                        edit_header) ||
        edit_header.recType != RT_UserEditAtom) {
      return std::nullopt;
    }
    UserEditAtomBody edit{};
    read(in, edit);
    if (!in) {
      return std::nullopt;
    }
    if (first) {
      doc_persist_id = edit.docPersistIdRef;
    }
    read_persist_directory(in, edit.offsetPersistDirectory, directory);

    // offsetLastEdit MUST strictly decrease; stop on a non-decreasing (i.e.
    // malformed/looping) chain.
    if (edit.offsetLastEdit >= edit_offset) {
      break;
    }
    edit_offset = edit.offsetLastEdit;
  }
  if (doc_persist_id == 0) {
    return std::nullopt;
  }

  // Resolve the live DocumentContainer through the directory.
  const auto doc_it = directory.find(doc_persist_id);
  if (doc_it == directory.end()) {
    return std::nullopt;
  }
  RecordHeader doc_header{};
  if (!read_header_at(in, static_cast<std::streamoff>(doc_it->second),
                      doc_header) ||
      doc_header.recType != RT_DocumentContainer) {
    return std::nullopt;
  }
  const std::streamoff doc_begin =
      static_cast<std::streamoff>(doc_it->second) +
      static_cast<std::streamoff>(sizeof(RecordHeader));
  const std::streamoff doc_end =
      doc_begin + static_cast<std::streamoff>(doc_header.recLen);

  // The presentation slide list (recInstance Slides).
  const std::vector<Record> doc_children =
      read_children(in, doc_begin, doc_end);
  const std::optional<Record> slide_list =
      find_child(doc_children, RT_SlideListWithText, SlideListInstance_Slides);
  if (!slide_list.has_value()) {
    return std::vector<std::string>{}; // valid document, no presentation slides
  }
  const std::streamoff slide_list_end =
      slide_list->body_begin +
      static_cast<std::streamoff>(slide_list->header.recLen);

  // Each SlidePersistAtom, in presentation order, references a SlideContainer
  // by persist id; resolve it and gather the text drawn on that slide.
  std::vector<std::string> slides;
  for (const Record &record :
       read_children(in, slide_list->body_begin, slide_list_end)) {
    if (record.header.recType != RT_SlidePersistAtom) {
      continue;
    }
    in.clear();
    in.seekg(record.body_begin);
    const std::uint32_t persist_id_ref = read_u32(in); // first body field

    std::string slide_text;
    if (const auto it = directory.find(persist_id_ref); it != directory.end()) {
      RecordHeader slide_header{};
      if (read_header_at(in, static_cast<std::streamoff>(it->second),
                         slide_header) &&
          slide_header.recType == RT_SlideContainer) {
        const std::streamoff slide_begin =
            static_cast<std::streamoff>(it->second) +
            static_cast<std::streamoff>(sizeof(RecordHeader));
        const std::streamoff slide_end =
            slide_begin + static_cast<std::streamoff>(slide_header.recLen);
        gather_text(in, slide_begin, slide_end, slide_text);
      }
    }
    slides.push_back(std::move(slide_text));
  }
  return slides;
}

// Fallback for files whose persist structures cannot be resolved: scan the
// stream and use whichever text strategy (outline vs. per-container) yields
// more text. Slide order is stream order.
std::vector<std::string> collect_scanned_slides(std::istream &in) {
  in.clear();
  in.seekg(0, std::ios::end);
  const std::streamoff stream_end = in.tellg();

  const std::vector<Record> top = read_children(in, 0, stream_end);

  std::vector<std::string> outline_slides;
  if (const std::optional<Record> document =
          find_child(top, RT_DocumentContainer);
      document.has_value()) {
    const std::streamoff document_end =
        document->body_begin +
        static_cast<std::streamoff>(document->header.recLen);
    outline_slides = collect_outline_slides(
        in, read_children(in, document->body_begin, document_end));
  }

  std::vector<std::string> container_slides =
      collect_container_slides(in, 0, stream_end);

  // Prefer the strategy with more total text; on a tie prefer the per-container
  // walk (so empty presentations still report their slides).
  if (total_text(container_slides) >= total_text(outline_slides)) {
    return container_slides;
  }
  return outline_slides;
}

} // namespace

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const abstract::ReadableFilesystem &files) {
  auto [root_id, _] = registry.create_element(ElementType::root);

  const auto stream = files.open(AbsPath("/PowerPoint Document"))->stream();

  // Prefer the spec-correct persist-directory path (presentation order, live
  // DocumentContainer); fall back to a plain stream scan if it cannot be
  // established.
  std::vector<std::string> slides;
  if (std::optional<std::vector<std::string>> ordered =
          collect_persist_ordered_slides(files, *stream);
      ordered.has_value()) {
    slides = std::move(*ordered);
  } else {
    slides = collect_scanned_slides(*stream);
  }

  for (const std::string &slide_text : slides) {
    auto [slide_id, _2] = registry.create_element(ElementType::slide);
    registry.append_child(root_id, slide_id);
    build_slide(registry, slide_id, slide_text);
  }

  return root_id;
}

} // namespace odr::internal::oldms::presentation
