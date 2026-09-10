#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
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
