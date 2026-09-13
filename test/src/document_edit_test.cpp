#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>
#include <odr/internal/zip/zip_file.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace odr;
using namespace odr::internal;

namespace {

/// One sheet whose single row holds three string cells.
Document three_cell_sheet() {
  const std::string source =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.spreadsheet">)"
      R"(<office:body><office:spreadsheet><table:table table:name="s">)"
      R"(<table:table-row>)"
      R"(<table:table-cell office:value-type="string"><text:p>a</text:p></table:table-cell>)"
      R"(<table:table-cell office:value-type="string"><text:p>b</text:p></table:table-cell>)"
      R"(<table:table-cell office:value-type="string"><text:p>c</text:p></table:table-cell>)"
      R"(</table:table-row></table:table>)"
      R"(</office:spreadsheet></office:body></office:document>)";
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

/// The one run the @p column -th cell of the first row holds.
Element run_of_cell(const Document &document, const std::uint32_t column) {
  return first_sheet(document).cell(column, 0).first_child().first_child();
}

} // namespace

TEST(DocumentEdit, the_ops_are_applied_in_order) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":2,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":0,"row":0,)"
                R"("value":{"type":"string","text":"first"}},)"
                R"({"op":"setCell","sheet":0,"column":0,"row":0,)"
                R"("value":{"type":"string","text":"second"}}]})");

  EXPECT_EQ(first_sheet(document).cell(0, 0).value().text(), "second");
}

TEST(DocumentEdit, a_number_op_states_the_number_and_the_text) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":2,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":1,"row":0,)"
                R"("value":{"type":"number","number":12.5,"text":"12,50"}}]})");

  const CellValue value = first_sheet(document).cell(1, 0).value();
  EXPECT_EQ(value.type(), ValueType::float_number);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 12.5);
  EXPECT_EQ(value.text(), "12,50");
}

/// The text is optional: the number spells itself.
TEST(DocumentEdit, a_number_op_without_text_spells_itself) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":2,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":1,"row":0,)"
                R"("value":{"type":"number","number":12.5}}]})");

  EXPECT_EQ(first_sheet(document).cell(1, 0).value().text(), "12.5");
}

TEST(DocumentEdit, an_empty_op_clears_the_cell) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":2,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":2,"row":0,)"
                R"("value":{"type":"empty"}}]})");

  EXPECT_EQ(first_sheet(document).cell(2, 0).value().text(), "");
}

TEST(DocumentEdit, a_text_op_names_its_element_by_id) {
  const Document document = three_cell_sheet();
  const Element run = run_of_cell(document, 0);

  document.edit(R"({"version":2,"ops":[{"op":"setText","id":)" +
                std::to_string(run.identifier()) + R"(,"text":"typed"}]})");

  EXPECT_EQ(first_sheet(document).cell(0, 0).value().text(), "typed");
}

TEST(DocumentEdit, a_text_op_naming_an_element_that_is_not_there_refuses) {
  const Document document = three_cell_sheet();

  EXPECT_THROW(document.edit(R"({"version":2,"ops":[)"
                             R"({"op":"setText","id":9999,"text":"typed"}]})"),
               std::invalid_argument);
  EXPECT_THROW(document.edit(R"({"version":2,"ops":[)"
                             R"({"op":"setText","id":0,"text":"typed"}]})"),
               std::invalid_argument);
}

TEST(DocumentEdit, a_text_op_naming_something_that_is_not_a_run_refuses) {
  const Document document = three_cell_sheet();
  const ElementIdentifier cell = first_sheet(document).cell(0, 0).identifier();

  EXPECT_THROW(document.edit(R"({"version":2,"ops":[{"op":"setText","id":)" +
                             std::to_string(cell) + R"(,"text":"typed"}]})"),
               std::invalid_argument);
}

TEST(DocumentEdit, an_unknown_version_refuses) {
  const Document document = three_cell_sheet();

  EXPECT_THROW(document.edit(R"({"version":1,"ops":[]})"),
               std::invalid_argument);
  EXPECT_THROW(document.edit(R"({"version":3,"ops":[]})"),
               std::invalid_argument);
  EXPECT_THROW(document.edit(R"({"ops":[]})"), std::invalid_argument);
}

TEST(DocumentEdit, an_unknown_op_refuses) {
  const Document document = three_cell_sheet();

  EXPECT_THROW(document.edit(R"({"version":2,"ops":[{"op":"setStyle"}]})"),
               std::invalid_argument);
}

TEST(DocumentEdit, an_op_naming_a_sheet_that_is_not_there_refuses) {
  const Document document = three_cell_sheet();

  EXPECT_THROW(document.edit(R"({"version":2,"ops":[)"
                             R"({"op":"setCell","sheet":3,"column":0,"row":0,)"
                             R"("value":{"type":"empty"}}]})"),
               std::invalid_argument);
}

/// It throws on the first op it cannot apply, so the ones before it stand.
TEST(DocumentEdit, the_ops_before_a_refusal_are_applied) {
  const Document document = three_cell_sheet();

  EXPECT_ANY_THROW(
      document.edit(R"({"version":2,"ops":[)"
                    R"({"op":"setCell","sheet":0,"column":0,"row":0,)"
                    R"("value":{"type":"string","text":"written"}},)"
                    R"({"op":"setCell","sheet":9,"column":0,"row":0,)"
                    R"("value":{"type":"string","text":"absent"}}]})"));

  EXPECT_EQ(first_sheet(document).cell(0, 0).value().text(), "written");
}

namespace {

/// Two paragraphs, the first of three runs with the middle one under a span -
/// so a test can see which parent a new run lands in.
Document two_paragraph_text() {
  const std::string source =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.text">)"
      R"(<office:body><office:text>)"
      R"(<text:p>one <text:span text:style-name="s">two</text:span> three</text:p>)"
      R"(<text:p>second</text:p>)"
      R"(</office:text></office:body></office:document>)";
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

/// Every run under @p element, in document order, joined.
std::string text_of(const Element element) {
  std::string result;
  if (element.type() == ElementType::text) {
    result += element.as_text().content();
  }
  for (const Element child : element.children()) {
    result += text_of(child);
  }
  return result;
}

Element paragraph_at(const Document &document, const std::uint32_t ordinal) {
  std::uint32_t seen = 0;
  for (const Element child : document.root_element().children()) {
    if (child.type() == ElementType::paragraph && seen++ == ordinal) {
      return child;
    }
  }
  return {};
}

/// The @p ordinal -th run of the @p paragraph -th paragraph, counting a run
/// under a span as the paragraph's own.
Element run_at(const Document &document, const std::uint32_t paragraph,
               const std::uint32_t ordinal) {
  std::uint32_t seen = 0;
  const auto walk = [&](this auto &&self, const Element element) -> Element {
    if (element.type() == ElementType::text && seen++ == ordinal) {
      return element;
    }
    for (const Element child : element.children()) {
      if (const Element found = self(child)) {
        return found;
      }
    }
    return {};
  };
  return walk(paragraph_at(document, paragraph));
}

std::string ops(const std::string &body) {
  return R"({"version":2,"ops":[)" + body + "]}";
}

std::string id_of(const Element element) {
  return std::to_string(element.identifier());
}

} // namespace

TEST(DocumentEdit, a_run_is_inserted_after_the_one_it_names) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"insertText","after":)" +
                    id_of(run_at(document, 0, 0)) +
                    R"(,"text":"and ","id":-1})"));

  EXPECT_EQ(text_of(paragraph_at(document, 0)), "one and two three");
}

TEST(DocumentEdit, a_run_is_inserted_before_the_one_it_names) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"insertText","before":)" +
                    id_of(run_at(document, 0, 2)) + R"(,"text":"!","id":-1})"));

  EXPECT_EQ(text_of(paragraph_at(document, 0)), "one two! three");
}

TEST(DocumentEdit, a_run_inserted_beside_a_styled_one_shares_its_parent) {
  const Document document = two_paragraph_text();
  const Element styled = run_at(document, 0, 1);

  document.edit(ops(R"({"op":"insertText","after":)" + id_of(styled) +
                    R"(,"text":"!","id":-1})"));

  EXPECT_EQ(text_of(paragraph_at(document, 0)), "one two! three");
  EXPECT_EQ(run_at(document, 0, 2).parent(), styled.parent());
}

TEST(DocumentEdit, a_removed_run_takes_its_text_with_it) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"removeElement","id":)" +
                    id_of(run_at(document, 0, 1)) + "}"));

  EXPECT_EQ(text_of(paragraph_at(document, 0)), "one  three");
}

TEST(DocumentEdit, a_removed_span_takes_its_subtree_with_it) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"removeElement","id":)" +
                    id_of(run_at(document, 0, 1).parent()) + "}"));

  EXPECT_EQ(text_of(paragraph_at(document, 0)), "one  three");
}

/// What typing over a selection spanning three runs looks like on the wire.
TEST(DocumentEdit, an_edit_across_three_runs_is_three_ops) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"setText","id":)" + id_of(run_at(document, 0, 0)) +
                    R"(,"text":"oX"},)" + R"({"op":"removeElement","id":)" +
                    id_of(run_at(document, 0, 1)) + "}," +
                    R"({"op":"setText","id":)" + id_of(run_at(document, 0, 2)) +
                    R"(,"text":"ree"})"));

  EXPECT_EQ(text_of(paragraph_at(document, 0)), "oXree");
}

TEST(DocumentEdit, a_later_op_names_a_run_an_earlier_one_created) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"insertText","after":)" +
                    id_of(run_at(document, 0, 2)) +
                    R"(,"text":"four","id":-1},)" +
                    R"({"op":"setText","id":-1,"text":"FOUR"})"));

  EXPECT_EQ(text_of(paragraph_at(document, 0)), "one two threeFOUR");
}

TEST(DocumentEdit, an_op_naming_a_created_element_that_is_not_there_refuses) {
  const Document document = two_paragraph_text();

  EXPECT_THROW(document.edit(ops(R"({"op":"setText","id":-1,"text":"x"})")),
               std::invalid_argument);
}

TEST(DocumentEdit, creating_two_elements_under_one_id_refuses) {
  const Document document = two_paragraph_text();
  const std::string anchor = id_of(run_at(document, 0, 0));

  EXPECT_THROW(document.edit(ops(R"({"op":"insertText","after":)" + anchor +
                                 R"(,"text":"a","id":-1},)" +
                                 R"({"op":"insertText","after":)" + anchor +
                                 R"(,"text":"b","id":-1})")),
               std::invalid_argument);
}

TEST(DocumentEdit, a_created_element_with_a_positive_id_refuses) {
  const Document document = two_paragraph_text();

  EXPECT_THROW(document.edit(ops(R"({"op":"insertText","after":)" +
                                 id_of(run_at(document, 0, 0)) +
                                 R"(,"text":"a","id":7})")),
               std::invalid_argument);
}

TEST(DocumentEdit, an_insert_naming_neither_side_or_both_refuses) {
  const Document document = two_paragraph_text();
  const std::string anchor = id_of(run_at(document, 0, 0));

  EXPECT_THROW(document.edit(ops(R"({"op":"insertText","text":"a","id":-1})")),
               std::invalid_argument);
  EXPECT_THROW(
      document.edit(ops(R"({"op":"insertText","after":)" + anchor +
                        R"(,"before":)" + anchor + R"(,"text":"a","id":-1})")),
      std::invalid_argument);
}

/// Rtf throws its source away as it parses, so its model is read-only.
TEST(DocumentEdit, a_read_only_engine_refuses_a_structural_op) {
  const Document document =
      DecodedFile(open_strategy::open_file(std::make_shared<MemoryFile>(
                                               std::string(R"({\rtf1 hello})")),
                                           {}, Logger::null()))
          .as_document_file()
          .document();
  ASSERT_FALSE(document.is_editable());

  const Element run = run_at(document, 0, 0);
  ASSERT_TRUE(run);
  EXPECT_THROW(
      document.edit(ops(R"({"op":"removeElement","id":)" + id_of(run) + "}")),
      UnsupportedOperation);
  EXPECT_THROW(document.edit(ops(R"({"op":"insertText","after":)" + id_of(run) +
                                 R"(,"text":"x","id":-1})")),
               UnsupportedOperation);
}

TEST(DocumentEdit, a_structural_edit_refuses_another_documents_element) {
  const Document document = two_paragraph_text();
  const Document other = two_paragraph_text();
  const Element run = run_at(other, 0, 0);
  ASSERT_TRUE(run);

  EXPECT_THROW(document.remove(run), std::invalid_argument);
  EXPECT_THROW((void)document.insert_text_after(run.as_text(), "x"),
               std::invalid_argument);
}

TEST(DocumentEdit, inserting_a_run_beside_something_that_is_not_one_refuses) {
  const Document document = two_paragraph_text();
  const Element paragraph = paragraph_at(document, 0);

  EXPECT_THROW((void)document.insert_text_after(paragraph.as_text(), "x"),
               std::invalid_argument);
}
/// Every paragraph of @p document, its runs joined.
namespace {

std::vector<std::string> paragraph_texts(const Document &document) {
  std::vector<std::string> result;
  for (const Element child : document.root_element().children()) {
    if (child.type() == ElementType::paragraph) {
      result.push_back(text_of(child));
    }
  }
  return result;
}

} // namespace

TEST(DocumentEdit, a_paragraph_splits_after_the_run_it_names) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"splitParagraph","paragraph":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                    id_of(run_at(document, 0, 0)) + R"(,"id":-1})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one ", "two three", "second"}));
}

/// Enter at the very start of a paragraph.
TEST(DocumentEdit, a_paragraph_naming_nothing_to_split_after_moves_everything) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"splitParagraph","paragraph":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"id":-1})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"", "one two three", "second"}));
}

/// Enter at the very end of a paragraph.
TEST(DocumentEdit, a_split_after_the_last_run_leaves_an_empty_paragraph) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"splitParagraph","paragraph":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                    id_of(run_at(document, 0, 2)) + R"(,"id":-1})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one two three", "", "second"}));
}

TEST(DocumentEdit, a_split_inside_a_span_leaves_the_span_in_both_halves) {
  const Document document = two_paragraph_text();
  const Element styled = run_at(document, 0, 1);

  document.edit(ops(R"({"op":"insertText","after":)" + id_of(styled) +
                    R"(,"text":"TWO","id":-1},)" +
                    R"({"op":"splitParagraph","paragraph":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                    id_of(styled) + R"(,"id":-2})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one two", "TWO three", "second"}));
  // the tail's run still sits under a span of its own, not under the paragraph
  EXPECT_EQ(run_at(document, 1, 0).parent().type(), ElementType::span);
}

/// What Enter in the middle of a run looks like on the wire.
TEST(DocumentEdit, enter_in_the_middle_of_a_run_is_three_ops) {
  const Document document = two_paragraph_text();
  const Element run = run_at(document, 0, 0);

  document.edit(ops(R"({"op":"setText","id":)" + id_of(run) +
                    R"(,"text":"on"},)" + R"({"op":"insertText","after":)" +
                    id_of(run) + R"(,"text":"e ","id":-1},)" +
                    R"({"op":"splitParagraph","paragraph":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                    id_of(run) + R"(,"id":-2})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"on", "e two three", "second"}));
}

TEST(DocumentEdit, a_paragraph_takes_the_one_after_it) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"mergeParagraph","paragraph":)" +
                    id_of(paragraph_at(document, 0)) + "}"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one two threesecond"}));
}

/// Backspace at the start of a paragraph and then Enter again puts it back.
TEST(DocumentEdit, a_merge_and_a_split_undo_each_other) {
  const Document document = two_paragraph_text();
  const std::string first = id_of(paragraph_at(document, 0));

  document.edit(ops(R"({"op":"mergeParagraph","paragraph":)" + first + "}," +
                    R"({"op":"splitParagraph","paragraph":)" + first +
                    R"(,"after":)" + id_of(run_at(document, 0, 2)) +
                    R"(,"id":-1})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one two three", "second"}));
}

TEST(DocumentEdit, a_merge_with_nothing_after_it_refuses) {
  const Document document = two_paragraph_text();

  EXPECT_THROW(document.edit(ops(R"({"op":"mergeParagraph","paragraph":)" +
                                 id_of(paragraph_at(document, 1)) + "}")),
               std::invalid_argument);
}

TEST(DocumentEdit, a_new_paragraph_is_inserted_after_the_one_it_names) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"insertParagraph","after":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"id":-1},)" +
                    R"({"op":"insertText","before":)" +
                    id_of(run_at(document, 0, 0)) + R"(,"text":"x","id":-2})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"xone two three", "", "second"}));
}

TEST(DocumentEdit, a_later_op_names_a_paragraph_an_earlier_one_created) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"insertParagraph","after":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"id":-1},)" +
                    R"({"op":"insertParagraph","after":-1,"id":-2})"));

  EXPECT_EQ(paragraph_texts(document).size(), 4U);
}

TEST(DocumentEdit, an_op_naming_a_paragraph_that_is_not_one_refuses) {
  const Document document = two_paragraph_text();

  EXPECT_THROW(document.edit(ops(R"({"op":"mergeParagraph","paragraph":)" +
                                 id_of(run_at(document, 0, 0)) + "}")),
               std::invalid_argument);
}

TEST(DocumentEdit, a_split_after_something_outside_the_paragraph_refuses) {
  const Document document = two_paragraph_text();

  EXPECT_THROW(
      document.edit(ops(R"({"op":"splitParagraph","paragraph":)" +
                        id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                        id_of(run_at(document, 1, 0)) + R"(,"id":-1})")),
      std::invalid_argument);
}

namespace {

/// A paragraph whose middle run sits under a link, and one whose middle child
/// is a frame holding a paragraph of its own.
Document nested_text(const std::string &middle) {
  const std::string source =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.text">)"
      R"(<office:body><office:text><text:p>one )" +
      middle +
      R"( three</text:p></office:text></office:body></office:document>)";
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

} // namespace

TEST(DocumentEdit, a_split_inside_a_link_leaves_the_link_in_both_halves) {
  const Document document =
      nested_text(R"(<text:a xlink:href="https://x.example">two</text:a>)");
  const Element linked = run_at(document, 0, 1);

  document.edit(ops(R"({"op":"insertText","after":)" + id_of(linked) +
                    R"(,"text":"TWO","id":-1},)" +
                    R"({"op":"splitParagraph","paragraph":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                    id_of(linked) + R"(,"id":-2})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one two", "TWO three"}));
  const Element tail = run_at(document, 1, 0).parent();
  ASSERT_EQ(tail.type(), ElementType::link);
  EXPECT_EQ(tail.as_link().href(), "https://x.example");
}

/// A copy of a frame has no obvious meaning, so the split refuses.
TEST(DocumentEdit, a_split_through_something_that_is_not_a_span_refuses) {
  const Document document = nested_text(
      R"(<draw:frame><draw:text-box><text:p>two</text:p></draw:text-box>)"
      R"(</draw:frame>)");
  const Element inner = run_at(document, 0, 1);
  ASSERT_TRUE(inner);

  EXPECT_THROW(
      document.edit(ops(R"({"op":"splitParagraph","paragraph":)" +
                        id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                        id_of(inner) + R"(,"id":-1})")),
      UnsupportedOperation);
}

namespace {

/// A paragraph under a bold automatic style, two spans sharing an italic
/// automatic style, a span under a named style, and a bare run - the four
/// places a mark lands.
Document styled_text() {
  const std::string source =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.text">)"
      R"(<office:styles>)"
      R"(<style:style style:name="Emphasis" style:family="text">)"
      R"(<style:text-properties fo:font-style="italic"/></style:style>)"
      R"(</office:styles>)"
      R"(<office:automatic-styles>)"
      R"(<style:style style:name="P1" style:family="paragraph">)"
      R"(<style:text-properties fo:font-weight="bold"/></style:style>)"
      R"(<style:style style:name="T1" style:family="text">)"
      R"(<style:text-properties fo:font-style="italic"/></style:style>)"
      R"(</office:automatic-styles>)"
      R"(<office:body><office:text>)"
      R"(<text:p text:style-name="P1">bold </text:p>)"
      R"(<text:p><text:span text:style-name="T1">one</text:span>)"
      R"(<text:span text:style-name="T1">two</text:span>)"
      R"(<text:span text:style-name="Emphasis">three</text:span> four</text:p>)"
      R"(</office:text></office:body></office:document>)";
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

std::string style_op(const Element run, const std::string &style) {
  return R"({"op":"setTextStyle","id":)" + id_of(run) + R"(,"style":)" + style +
         "}";
}

} // namespace

TEST(DocumentEdit, a_style_op_on_a_bare_run_gives_it_a_span_of_its_own) {
  const Document document = two_paragraph_text();
  const Element run = run_at(document, 0, 0);
  ASSERT_EQ(run.parent().type(), ElementType::paragraph);

  document.edit(ops(style_op(run, R"({"bold":true})")));

  EXPECT_EQ(run.parent().type(), ElementType::span);
  EXPECT_EQ(run.as_text().style().font_weight, FontWeight::bold);
  EXPECT_EQ(run_at(document, 0, 2).as_text().style().font_weight, std::nullopt);
  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one two three", "second"}));
}

TEST(DocumentEdit, a_style_op_on_a_run_alone_in_its_span_keeps_the_span) {
  const Document document = styled_text();
  const Element run = run_at(document, 1, 0);
  const Element span = run.parent();
  ASSERT_EQ(span.type(), ElementType::span);

  document.edit(ops(style_op(run, R"({"bold":true})")));

  EXPECT_EQ(run.parent(), span);
  EXPECT_EQ(run.as_text().style().font_weight, FontWeight::bold);
}

TEST(DocumentEdit, a_style_op_copies_a_shared_automatic_style) {
  const Document document = styled_text();
  const Element one = run_at(document, 1, 0);
  const Element two = run_at(document, 1, 1);

  document.edit(ops(style_op(one, R"({"bold":true})")));

  // the copy keeps the italic and the sibling under the old style stays as it
  // was
  EXPECT_EQ(one.as_text().style().font_style, FontStyle::italic);
  EXPECT_EQ(one.as_text().style().font_weight, FontWeight::bold);
  EXPECT_EQ(two.as_text().style().font_style, FontStyle::italic);
  EXPECT_EQ(two.as_text().style().font_weight, std::nullopt);
}

TEST(DocumentEdit, a_style_op_under_a_named_style_inherits_from_it) {
  const Document document = styled_text();
  const Element three = run_at(document, 1, 2);

  document.edit(ops(style_op(three, R"({"bold":true})")));

  EXPECT_EQ(three.as_text().style().font_style, FontStyle::italic);
  EXPECT_EQ(three.as_text().style().font_weight, FontWeight::bold);
}

TEST(DocumentEdit, a_style_turned_off_is_written_over_the_paragraph) {
  const Document document = styled_text();
  const Element run = run_at(document, 0, 0);
  ASSERT_EQ(run.as_text().style().font_weight, FontWeight::bold);

  document.edit(ops(style_op(run, R"({"bold":false})")));

  EXPECT_EQ(run.as_text().style().font_weight, FontWeight::normal);
}

TEST(DocumentEdit, every_property_reaches_the_run) {
  const Document document = two_paragraph_text();
  const Element run = run_at(document, 0, 1);

  document.edit(ops(style_op(
      run,
      R"({"bold":true,"italic":true,"underline":true,"strikethrough":true,)"
      R"("highlight":"#ffff00","color":"#ff0000","size":"14pt"})")));

  const TextStyle style = run.as_text().style();
  EXPECT_EQ(style.font_weight, FontWeight::bold);
  EXPECT_EQ(style.font_style, FontStyle::italic);
  EXPECT_EQ(style.font_underline, true);
  EXPECT_EQ(style.font_line_through, true);
  ASSERT_TRUE(style.background_color.has_value());
  EXPECT_EQ(style.background_color->rgb(), 0xffff00U);
  ASSERT_TRUE(style.font_color.has_value());
  EXPECT_EQ(style.font_color->rgb(), 0xff0000U);
  ASSERT_TRUE(style.font_size.has_value());
  EXPECT_EQ(style.font_size->to_string(), "14pt");
}

TEST(DocumentEdit, a_highlight_of_null_takes_the_highlight_away) {
  const Document document = two_paragraph_text();
  const Element run = run_at(document, 0, 1);

  document.edit(ops(style_op(run, R"({"highlight":"#ffff00"})") + "," +
                    style_op(run, R"({"highlight":null})")));

  EXPECT_EQ(run.as_text().style().background_color, std::nullopt);
}

TEST(DocumentEdit, a_second_style_op_keeps_what_the_first_wrote) {
  const Document document = two_paragraph_text();
  const Element run = run_at(document, 0, 0);

  document.edit(ops(style_op(run, R"({"bold":true})") + "," +
                    style_op(run, R"({"italic":true})")));

  EXPECT_EQ(run.as_text().style().font_weight, FontWeight::bold);
  EXPECT_EQ(run.as_text().style().font_style, FontStyle::italic);
}

TEST(DocumentEdit, a_style_op_on_a_split_run_marks_the_middle_only) {
  const Document document = styled_text();
  const Element run = run_at(document, 1, 0);

  // the browser splits `one` into `o`, `n`, `e` and marks the `n`
  document.edit(ops(R"({"op":"setText","id":)" + id_of(run) +
                    R"(,"text":"o"},)" + R"({"op":"insertText","after":)" +
                    id_of(run) + R"(,"text":"n","id":-1},)" +
                    R"({"op":"insertText","after":-1,"text":"e","id":-2},)" +
                    R"({"op":"setTextStyle","id":-1,"style":{"bold":true}})"));

  EXPECT_EQ(text_of(paragraph_at(document, 1)), "onetwothree four");
  EXPECT_EQ(run_at(document, 1, 0).as_text().style().font_weight, std::nullopt);
  EXPECT_EQ(run_at(document, 1, 1).as_text().style().font_weight,
            FontWeight::bold);
  EXPECT_EQ(run_at(document, 1, 2).as_text().style().font_weight, std::nullopt);
  // each part sits in a span of its own, all italic from the copied style
  for (const std::uint32_t ordinal : {0U, 1U, 2U}) {
    const Element part = run_at(document, 1, ordinal);
    EXPECT_EQ(part.parent().type(), ElementType::span);
    EXPECT_EQ(part.as_text().style().font_style, FontStyle::italic);
  }
}

TEST(DocumentEdit, a_style_op_survives_a_save) {
  const Document document = styled_text();
  document.edit(ops(style_op(run_at(document, 1, 0), R"({"bold":true})") + "," +
                    style_op(run_at(document, 0, 0), R"({"bold":false})")));

  const Document reloaded =
      DecodedFile(open_strategy::open_file(
                      std::make_shared<MemoryFile>(std::string(
                          document.save_to_memory().memory_data().value())),
                      {}, Logger::null()))
          .as_document_file()
          .document();

  EXPECT_EQ(run_at(reloaded, 1, 0).as_text().style().font_weight,
            FontWeight::bold);
  EXPECT_EQ(run_at(reloaded, 1, 0).as_text().style().font_style,
            FontStyle::italic);
  EXPECT_EQ(run_at(reloaded, 1, 1).as_text().style().font_weight, std::nullopt);
  EXPECT_EQ(run_at(reloaded, 0, 0).as_text().style().font_weight,
            FontWeight::normal);
}

TEST(DocumentEdit, a_style_op_with_an_unknown_property_refuses) {
  const Document document = two_paragraph_text();
  const Element run = run_at(document, 0, 0);

  EXPECT_THROW(document.edit(ops(style_op(run, R"({"blink":true})"))),
               std::invalid_argument);
  EXPECT_THROW(document.edit(ops(style_op(run, R"({"color":"red"})"))),
               std::invalid_argument);
  EXPECT_THROW(document.edit(ops(style_op(run, R"({"size":"large"})"))),
               std::invalid_argument);
  // no fixed size, so no engine can write it
  EXPECT_THROW(document.edit(ops(style_op(run, R"({"size":"2em"})"))),
               std::invalid_argument);
  EXPECT_EQ(run.as_text().style().font_weight, std::nullopt);
}

TEST(DocumentEdit, a_read_only_engine_refuses_a_style_op) {
  const Document document =
      DecodedFile(open_strategy::open_file(std::make_shared<MemoryFile>(
                                               std::string(R"({\rtf1 hello})")),
                                           {}, Logger::null()))
          .as_document_file()
          .document();
  const Element run = run_at(document, 0, 0);
  ASSERT_TRUE(run);

  EXPECT_THROW(document.edit(ops(style_op(run, R"({"bold":true})"))),
               UnsupportedOperation);
}

TEST(DocumentEdit, a_style_the_handle_does_not_write_refuses) {
  const Document document = two_paragraph_text();
  TextStyle style;
  style.font_position = FontPosition::super;

  EXPECT_THROW(run_at(document, 0, 0).as_text().set_style(style),
               UnsupportedOperation);
}

namespace {

using Part = std::pair<std::string, std::string>;

Document package_of(const std::vector<Part> &parts) {
  zip::ZipArchive archive;
  for (const auto &[path, content] : parts) {
    archive.insert_file(std::end(archive), RelPath(path),
                        std::make_shared<MemoryFile>(content));
  }
  std::stringstream out;
  archive.save(out);
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(out.str()),
                                      {}, Logger::null()))
      .as_document_file()
      .document();
}

/// The smallest docx that opens, its body @p paragraphs and one paragraph
/// style `Bold`.
Document docx_of(const std::string &paragraphs) {
  return package_of(
      {{"_rels/.rels",
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>)"
        R"(</Relationships>)"},
       {"word/styles.xml",
        R"(<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">)"
        R"(<w:style w:type="paragraph" w:styleId="Bold"><w:rPr><w:b/></w:rPr></w:style>)"
        R"(</w:styles>)"},
       {"word/document.xml",
        R"(<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">)"
        R"(<w:body>)" +
            paragraphs + R"(</w:body></w:document>)"}});
}

/// The smallest pptx that opens: one slide, one shape, its text body holding
/// @p paragraphs.
Document pptx_of(const std::string &paragraphs) {
  return package_of(
      {{"ppt/presentation.xml",
        R"(<p:presentation xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" )"
        R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
        R"(<p:sldIdLst><p:sldId id="256" r:id="rId1"/></p:sldIdLst></p:presentation>)"},
       {"ppt/_rels/presentation.xml.rels",
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide1.xml"/>)"
        R"(</Relationships>)"},
       {"ppt/slides/slide1.xml",
        R"(<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" )"
        R"(xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">)"
        R"(<p:cSld><p:spTree><p:sp><p:txBody>)" +
            paragraphs + R"(</p:txBody></p:sp></p:spTree></p:cSld></p:sld>)"}});
}

/// The @p ordinal -th run anywhere in @p document.
Element nth_run(const Document &document, const std::uint32_t ordinal) {
  std::uint32_t seen = 0;
  const auto walk = [&](this auto &&self, const Element element) -> Element {
    if (element.type() == ElementType::text && seen++ == ordinal) {
      return element;
    }
    for (const Element child : element.children()) {
      if (const Element found = self(child)) {
        return found;
      }
    }
    return {};
  };
  return walk(document.root_element());
}

/// The part @p path of @p document saved, empty elements spelt `<x/>`
/// whichever way the writer spells them.
std::string part_of(const Document &document, const std::string &path) {
  const std::shared_ptr<abstract::File> saved = std::make_shared<MemoryFile>(
      std::string(document.save_to_memory().memory_data().value()));
  std::string xml = util::stream::read(*zip::ZipFile(saved)
                                            .archive()
                                            ->as_filesystem()
                                            ->open(AbsPath("/" + path))
                                            ->stream());
  util::string::replace_all(xml, " />", "/>");
  return xml;
}

Document reopened(const Document &document) {
  return DecodedFile(open_strategy::open_file(
                         std::make_shared<MemoryFile>(std::string(
                             document.save_to_memory().memory_data().value())),
                         {}, Logger::null()))
      .as_document_file()
      .document();
}

const std::string docx_paragraphs =
    R"(<w:p><w:r><w:rPr><w:i/></w:rPr><w:t>one</w:t></w:r></w:p>)"
    R"(<w:p><w:pPr><w:pStyle w:val="Bold"/></w:pPr><w:r><w:t>bold</w:t></w:r></w:p>)"
    R"(<w:p><w:r><w:rPr><w:rFonts w:ascii="Arial"/><w:lang w:val="en"/></w:rPr><w:t>ordered</w:t></w:r></w:p>)";

const std::string pptx_paragraphs =
    R"(<a:p><a:r><a:rPr lang="en" i="1"/><a:t>one</a:t></a:r></a:p>)"
    R"(<a:p><a:r><a:t>plain</a:t></a:r></a:p>)";

} // namespace

TEST(DocumentEdit, docx_a_mark_on_a_run_alone_in_its_w_r_writes_into_it) {
  const Document document = docx_of(docx_paragraphs);
  const Element run = nth_run(document, 0);
  const Element holder = run.parent();
  ASSERT_EQ(holder.type(), ElementType::span);

  document.edit(ops(style_op(run, R"({"bold":true})")));

  EXPECT_EQ(run.parent(), holder);
  EXPECT_EQ(run.as_text().style().font_weight, FontWeight::bold);
  EXPECT_EQ(run.as_text().style().font_style, FontStyle::italic);
}

TEST(DocumentEdit, docx_a_mark_on_part_of_a_w_r_cuts_it) {
  const Document document = docx_of(docx_paragraphs);
  const Element run = nth_run(document, 0);

  document.edit(ops(R"({"op":"setText","id":)" + id_of(run) +
                    R"(,"text":"o"},)" + R"({"op":"insertText","after":)" +
                    id_of(run) + R"(,"text":"n","id":-1},)" +
                    R"({"op":"insertText","after":-1,"text":"e","id":-2},)" +
                    R"({"op":"setTextStyle","id":-1,"style":{"bold":true}})"));

  EXPECT_EQ(text_of(document.root_element()), "oneboldordered");
  EXPECT_EQ(nth_run(document, 0).as_text().style().font_weight, std::nullopt);
  EXPECT_EQ(nth_run(document, 1).as_text().style().font_weight,
            FontWeight::bold);
  EXPECT_EQ(nth_run(document, 2).as_text().style().font_weight, std::nullopt);
  // three `w:r`, each with the italic the one carried
  for (const std::uint32_t ordinal : {0U, 1U, 2U}) {
    const Element part = nth_run(document, ordinal);
    EXPECT_EQ(part.as_text().style().font_style, FontStyle::italic);
    EXPECT_NE(part.parent(), nth_run(document, (ordinal + 1) % 3).parent());
  }
}

TEST(DocumentEdit, docx_off_is_written_over_the_paragraph_style) {
  const Document document = docx_of(docx_paragraphs);
  const Element run = nth_run(document, 1);
  ASSERT_EQ(run.as_text().style().font_weight, FontWeight::bold);

  document.edit(ops(style_op(run, R"({"bold":false})")));

  EXPECT_EQ(run.as_text().style().font_weight, FontWeight::normal);
  EXPECT_NE(part_of(document, "word/document.xml")
                .find(R"(<w:rPr><w:b w:val="0"/><w:bCs w:val="0"/></w:rPr>)"),
            std::string::npos);
}

TEST(DocumentEdit, docx_every_property_reaches_the_run_in_schema_order) {
  const Document document = docx_of(docx_paragraphs);
  const Element run = nth_run(document, 2);

  document.edit(ops(style_op(
      run,
      R"({"bold":true,"italic":true,"underline":true,"strikethrough":true,)"
      R"("highlight":"#ffff00","color":"#ff0000","size":"14pt"})")));

  const TextStyle style = run.as_text().style();
  EXPECT_EQ(style.font_weight, FontWeight::bold);
  EXPECT_EQ(style.font_style, FontStyle::italic);
  EXPECT_EQ(style.font_underline, true);
  EXPECT_EQ(style.font_line_through, true);
  ASSERT_TRUE(style.background_color.has_value());
  EXPECT_EQ(style.background_color->rgb(), 0xffff00U);
  ASSERT_TRUE(style.font_color.has_value());
  EXPECT_EQ(style.font_color->rgb(), 0xff0000U);
  ASSERT_TRUE(style.font_size.has_value());
  EXPECT_EQ(style.font_size->to_string(), "14pt");

  // between the font and the language the file already had, in the order
  // [ECMA-376] 17.3.2.28 gives
  EXPECT_NE(part_of(document, "word/document.xml")
                .find(R"(<w:rPr><w:rFonts w:ascii="Arial"/><w:b/><w:bCs/>)"
                      R"(<w:i/><w:iCs/><w:strike/><w:color w:val="FF0000"/>)"
                      R"(<w:sz w:val="28"/><w:szCs w:val="28"/>)"
                      R"(<w:highlight w:val="yellow"/><w:u w:val="single"/>)"
                      R"(<w:lang w:val="en"/></w:rPr><w:t>ordered</w:t>)"),
            std::string::npos);
}

TEST(DocumentEdit, docx_a_highlight_outside_words_palette_is_a_shading) {
  const Document document = docx_of(docx_paragraphs);
  const Element run = nth_run(document, 1);

  document.edit(ops(style_op(run, R"({"highlight":"#123456"})")));

  ASSERT_TRUE(run.as_text().style().background_color.has_value());
  EXPECT_EQ(run.as_text().style().background_color->rgb(), 0x123456U);
  const std::string shaded = part_of(document, "word/document.xml");
  EXPECT_NE(
      shaded.find(R"(<w:shd w:val="clear" w:color="auto" w:fill="123456"/>)"),
      std::string::npos);
  EXPECT_EQ(shaded.find("w:highlight"), std::string::npos);

  document.edit(ops(style_op(run, R"({"highlight":null})")));

  EXPECT_EQ(run.as_text().style().background_color, std::nullopt);
  const std::string cleared = part_of(document, "word/document.xml");
  EXPECT_NE(cleared.find(R"(<w:highlight w:val="none"/>)"), std::string::npos);
  EXPECT_EQ(cleared.find("w:shd"), std::string::npos);
}

TEST(DocumentEdit, docx_a_mark_survives_a_save) {
  const Document document = docx_of(docx_paragraphs);
  document.edit(ops(style_op(nth_run(document, 0), R"({"bold":true})") + "," +
                    style_op(nth_run(document, 1), R"({"bold":false})")));

  const Document saved = reopened(document);

  EXPECT_EQ(nth_run(saved, 0).as_text().style().font_weight, FontWeight::bold);
  EXPECT_EQ(nth_run(saved, 0).as_text().style().font_style, FontStyle::italic);
  EXPECT_EQ(nth_run(saved, 1).as_text().style().font_weight,
            FontWeight::normal);
}

TEST(DocumentEdit, pptx_a_mark_writes_the_attributes_of_a_rPr) {
  const Document document = pptx_of(pptx_paragraphs);
  const Element run = nth_run(document, 0);

  document.edit(ops(style_op(
      run,
      R"({"bold":true,"underline":true,"strikethrough":true,"size":"20pt"})")));

  const TextStyle style = run.as_text().style();
  EXPECT_EQ(style.font_weight, FontWeight::bold);
  EXPECT_EQ(style.font_style, FontStyle::italic);
  EXPECT_EQ(style.font_underline, true);
  EXPECT_EQ(style.font_line_through, true);
  ASSERT_TRUE(style.font_size.has_value());
  EXPECT_EQ(style.font_size->to_string(), "20pt");
  EXPECT_NE(part_of(document, "ppt/slides/slide1.xml")
                .find(R"(<a:rPr lang="en" i="1" b="1" u="sng" )"
                      R"(strike="sngStrike" sz="2000"/><a:t>one</a:t>)"),
            std::string::npos);
}

TEST(DocumentEdit, pptx_a_run_without_rPr_gets_one_ahead_of_its_text) {
  const Document document = pptx_of(pptx_paragraphs);
  const Element run = nth_run(document, 1);

  document.edit(
      ops(style_op(run, R"({"color":"#ff0000","highlight":"#00ff00"})")));

  ASSERT_TRUE(run.as_text().style().font_color.has_value());
  EXPECT_EQ(run.as_text().style().font_color->rgb(), 0xff0000U);
  ASSERT_TRUE(run.as_text().style().background_color.has_value());
  EXPECT_EQ(run.as_text().style().background_color->rgb(), 0x00ff00U);
  EXPECT_NE(part_of(document, "ppt/slides/slide1.xml")
                .find(R"(<a:r><a:rPr><a:solidFill><a:srgbClr val="FF0000"/>)"
                      R"(</a:solidFill><a:highlight><a:srgbClr val="00FF00"/>)"
                      R"(</a:highlight></a:rPr><a:t>plain</a:t></a:r>)"),
            std::string::npos);
}

TEST(DocumentEdit, pptx_a_highlight_taken_away_leaves_no_element) {
  const Document document = pptx_of(pptx_paragraphs);
  const Element run = nth_run(document, 0);

  document.edit(ops(style_op(run, R"({"highlight":"#00ff00"})") + "," +
                    style_op(run, R"({"highlight":null})")));

  EXPECT_EQ(run.as_text().style().background_color, std::nullopt);
  EXPECT_EQ(part_of(document, "ppt/slides/slide1.xml").find("a:highlight"),
            std::string::npos);
}

TEST(DocumentEdit, pptx_a_mark_on_part_of_an_a_r_cuts_it) {
  const Document document = pptx_of(pptx_paragraphs);
  const Element run = nth_run(document, 0);

  document.edit(ops(R"({"op":"setText","id":)" + id_of(run) +
                    R"(,"text":"o"},)" + R"({"op":"insertText","after":)" +
                    id_of(run) + R"(,"text":"n","id":-1},)" +
                    R"({"op":"insertText","after":-1,"text":"e","id":-2},)" +
                    R"({"op":"setTextStyle","id":-1,"style":{"bold":true}})"));

  EXPECT_EQ(text_of(document.root_element()), "oneplain");
  EXPECT_EQ(nth_run(document, 0).as_text().style().font_weight, std::nullopt);
  EXPECT_EQ(nth_run(document, 1).as_text().style().font_weight,
            FontWeight::bold);
  EXPECT_EQ(nth_run(document, 2).as_text().style().font_weight, std::nullopt);
  for (const std::uint32_t ordinal : {0U, 1U, 2U}) {
    EXPECT_EQ(nth_run(document, ordinal).as_text().style().font_style,
              FontStyle::italic);
  }
}

TEST(DocumentEdit, pptx_a_mark_survives_a_save) {
  const Document document = pptx_of(pptx_paragraphs);
  document.edit(ops(style_op(nth_run(document, 0), R"({"bold":true})") + "," +
                    style_op(nth_run(document, 1), R"({"italic":true})")));

  const Document saved = reopened(document);

  EXPECT_EQ(nth_run(saved, 0).as_text().style().font_weight, FontWeight::bold);
  EXPECT_EQ(nth_run(saved, 1).as_text().style().font_style, FontStyle::italic);
}

TEST(DocumentEdit, a_paragraph_edit_refuses_another_documents_element) {
  const Document document = two_paragraph_text();
  const Document other = two_paragraph_text();
  const Paragraph paragraph = paragraph_at(other, 0).as_paragraph();
  ASSERT_TRUE(paragraph);

  EXPECT_THROW(document.merge_paragraph_with_next(paragraph),
               std::invalid_argument);
  EXPECT_THROW((void)document.insert_paragraph_after(paragraph),
               std::invalid_argument);
  EXPECT_THROW((void)document.split_paragraph(paragraph, Element()),
               std::invalid_argument);
}

/// The paragraph Enter just made holds no run to sit beside, so the operation
/// names the paragraph itself.
TEST(DocumentEdit, a_run_is_appended_into_a_paragraph_that_holds_none) {
  const Document document = two_paragraph_text();

  document.edit(ops(R"({"op":"insertParagraph","after":)" +
                    id_of(paragraph_at(document, 0)) + R"(,"id":-1},)" +
                    R"({"op":"insertText","parent":-1,"text":"typed",)"
                    R"("id":-2})"));

  EXPECT_EQ(paragraph_texts(document),
            (std::vector<std::string>{"one two three", "typed", "second"}));
}

TEST(DocumentEdit, an_insert_naming_a_parent_and_a_run_to_sit_beside_refuses) {
  const Document document = two_paragraph_text();

  EXPECT_THROW(document.edit(ops(
                   R"({"op":"insertText","parent":)" +
                   id_of(paragraph_at(document, 0)) + R"(,"after":)" +
                   id_of(run_at(document, 0, 0)) + R"(,"text":"x","id":-1})")),
               std::invalid_argument);
}
