#include <odr/internal/oldms/text/doc_parser.hpp>

#include <odr/internal/common/path.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/oldms/text/doc_element_registry.hpp>
#include <odr/internal/oldms/text/doc_helper.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/oldms/text/doc_structs.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <iostream>
#include <string>

namespace odr::internal::oldms {

ElementIdentifier text::parse_tree(ElementRegistry &registry,
                                   const abstract::ReadableFilesystem &files) {
  auto [root_id, _] = registry.create_element(ElementType::root);

  const std::string word_document =
      util::stream::read(*files.open(AbsPath("/WordDocument"))->stream());

  const auto stream = files.open(AbsPath("/WordDocument"))->stream();
  ParsedFib fib;
  read(*stream, fib);

  const std::string tableStreamPath =
      fib.base.fWhichTblStm == 1 ? "/1Table" : "/0Table";

  const std::string table =
      util::stream::read(*files.open(AbsPath(tableStreamPath))->stream());

  const auto table_stream = files.open(AbsPath(tableStreamPath))->stream();
  table_stream->ignore(fib.fibRgFcLcb->clx.fc);
  const CharacterIndex character_index = read_character_index(*table_stream);
  for (const auto &entry : character_index) {
    const auto document_stream = files.open(AbsPath("/WordDocument"))->stream();
    document_stream->seekg(entry.data_offset);
    const std::string text =
        read_string_compressed(*document_stream, entry.data_length);

    auto [text_id, _, text_element] = registry.create_text_element();
    text_element.text = text;

    registry.append_child(root_id, text_id);
  }

  return root_id;
}

} // namespace odr::internal::oldms
