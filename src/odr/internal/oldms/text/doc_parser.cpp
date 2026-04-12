#include <odr/internal/oldms/text/doc_parser.hpp>

#include <odr/internal/common/path.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/oldms/text/doc_element_registry.hpp>
#include <odr/internal/oldms/text/doc_helper.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/oldms/text/doc_structs.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <iostream>
#include <string>

namespace odr::internal::oldms {

ElementIdentifier text::parse_tree(ElementRegistry &registry,
                                   const abstract::ReadableFilesystem &files) {
  auto [root_id, _] = registry.create_element(ElementType::root);

  const auto document_stream = files.open(AbsPath("/WordDocument"))->stream();
  ParsedFib fib;
  read(*document_stream, fib);

  const std::string tableStreamPath =
      fib.base.fWhichTblStm == 1 ? "/1Table" : "/0Table";

  const std::string table =
      util::stream::read(*files.open(AbsPath(tableStreamPath))->stream());

  auto [first_paragraph_id, first_paragraph] =
      registry.create_element(ElementType::paragraph);
  registry.append_child(root_id, first_paragraph_id);
  auto current_paragraph_id = first_paragraph_id;

  const auto table_stream = files.open(AbsPath(tableStreamPath))->stream();
  table_stream->ignore(fib.fibRgFcLcb->clx.fc);

  const CharacterIndex character_index = read_character_index(*table_stream);
  for (const auto &entry : character_index) {
    document_stream->seekg(entry.data_offset);
    const std::string text_entry =
        read_string(*document_stream, entry.length_cp, entry.is_compressed);

    const auto paragraphs = util::string::split(text_entry, "\r");
    for (std::uint32_t paragraph_i = 0; paragraph_i < paragraphs.size();
         ++paragraph_i) {
      const auto &paragraph = paragraphs[paragraph_i];
      if (paragraph_i > 0) {
        auto [paragraph_id, _] =
            registry.create_element(ElementType::paragraph);
        registry.append_child(root_id, paragraph_id);
        current_paragraph_id = paragraph_id;
      }

      const auto lines = util::string::split(paragraph, "\n");
      for (std::uint32_t line_i = 0; line_i < lines.size(); ++line_i) {
        if (line_i > 0) {
          auto [line_id, _] = registry.create_element(ElementType::line_break);
          registry.append_child(current_paragraph_id, line_id);
          current_paragraph_id = line_id;
        }

        auto [text_id, _, text_element] = registry.create_text_element();
        text_element.text = paragraph;

        registry.append_child(current_paragraph_id, text_id);
      }
    }
  }

  return root_id;
}

} // namespace odr::internal::oldms
