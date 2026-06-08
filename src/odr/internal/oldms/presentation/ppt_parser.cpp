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

} // namespace

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const abstract::ReadableFilesystem &files) {
  auto [root_id, _] = registry.create_element(ElementType::root);

  const auto stream = files.open(AbsPath("/PowerPoint Document"))->stream();

  stream->seekg(0, std::ios::end);
  const std::streamoff stream_end = stream->tellg();

  // Two independent strategies (see [MS-PPT]); use whichever yields more text.
  // The outline (option 1) is order-correct but often empty; the per-container
  // walk (option 2) captures the text actually drawn on each slide.
  const std::vector<Record> top = read_children(*stream, 0, stream_end);

  std::vector<std::string> outline_slides;
  if (const std::optional<Record> document =
          find_child(top, RT_DocumentContainer);
      document.has_value()) {
    const std::streamoff document_end =
        document->body_begin +
        static_cast<std::streamoff>(document->header.recLen);
    outline_slides = collect_outline_slides(
        *stream, read_children(*stream, document->body_begin, document_end));
  }

  std::vector<std::string> container_slides =
      collect_container_slides(*stream, 0, stream_end);

  // Prefer the strategy with more total text; on a tie prefer the per-container
  // walk (so empty presentations still report their slides).
  const bool use_container =
      total_text(container_slides) >= total_text(outline_slides);
  const std::vector<std::string> &slides =
      use_container ? container_slides : outline_slides;

  for (const std::string &slide_text : slides) {
    auto [slide_id, _2] = registry.create_element(ElementType::slide);
    registry.append_child(root_id, slide_id);
    build_slide(registry, slide_id, slide_text);
  }

  return root_id;
}

} // namespace odr::internal::oldms::presentation
