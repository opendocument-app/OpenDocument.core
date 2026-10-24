#include <test_util.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::test;

namespace {

/// A few words, as a user changes them: the runs are spread over the document.
constexpr std::size_t edited_runs = 5;
constexpr const char *edit_mark = " odr-edited";

void collect_runs(const Element &element, std::vector<Text> &runs) {
  for (Element child = element.first_child(); child;
       child = child.next_sibling()) {
    if (const Text text = child.as_text();
        text && text.is_editable() && !text.content().empty()) {
      runs.push_back(text);
    }
    collect_runs(child, runs);
  }
}

std::string all_text(const Element &element) {
  std::string result;
  for (Element child = element.first_child(); child;
       child = child.next_sibling()) {
    if (const Text text = child.as_text()) {
      result += text.content();
    }
    result += all_text(child);
  }
  return result;
}

Sheet first_sheet(const Document &document) {
  return document.root_element().first_child().as_sheet();
}

/// The cell right of the content of the first sheet: it is empty, so nothing
/// locks it, and the write has to create it.
std::uint32_t edited_column(const Document &document) {
  return first_sheet(document).content(std::nullopt).columns;
}

/// The texts the saved file has to hold.
std::vector<std::string> edit(const Document &document) {
  if (document.document_type() == DocumentType::spreadsheet) {
    first_sheet(document).set_cell(edited_column(document), 0,
                                   CellValue("odr-edited"));
    return {"odr-edited"};
  }

  std::vector<Text> runs;
  collect_runs(document.root_element(), runs);
  std::vector<std::string> result;
  for (std::size_t i = 0; i < edited_runs && i < runs.size(); ++i) {
    const Text &run = runs[i * runs.size() / edited_runs];
    const std::string content = run.content() + edit_mark;
    run.set_content(content);
    result.push_back(content);
  }
  return result;
}

/// The document of @p test_file, where it decodes into one that can edit and
/// save.
std::optional<Document> editable_document(const TestFile &test_file) {
  if (test_file.password.has_value()) {
    return std::nullopt;
  }
  try {
    const DecodedFile file = open(test_file.absolute_path);
    if (!file.is_document_file()) {
      return std::nullopt;
    }
    Document document = file.as_document_file().document();
    if (!document.is_editable() || !document.is_savable()) {
      return std::nullopt;
    }
    return document;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

/// Edits @p document, saves it to @p path and opens the file again.
void round_trip(const Document &document, const std::filesystem::path &path) {
  const bool sheet = document.document_type() == DocumentType::spreadsheet;
  const std::uint32_t column = sheet ? edited_column(document) : 0;
  const std::vector<std::string> expected = edit(document);
  std::filesystem::create_directories(path.parent_path());
  document.save(path.string());

  const Document reopened = open(path.string()).as_document_file().document();
  EXPECT_EQ(reopened.document_type(), document.document_type());

  if (sheet) {
    EXPECT_EQ(first_sheet(reopened).cell(column, 0).value().text(),
              expected.front());
    return;
  }
  const std::string text = all_text(reopened.root_element());
  for (const std::string &content : expected) {
    EXPECT_NE(text.find(content), std::string::npos) << content;
  }
}

} // namespace

/// Every editable document of the corpus takes a small edit, saves, and opens
/// again with the edit in it. The saved files stay under `output/saved` for
/// `test/scripts/check_saved_files.py`, which loads them in LibreOffice.
TEST(SaveCorpus, a_small_edit_saves_and_opens_again) {
  const std::filesystem::path saved_root =
      std::filesystem::current_path() / "output" / "saved";
  std::size_t saved = 0;

  for (const TestFile &test_file : TestData::test_files()) {
    const std::optional<Document> document = editable_document(test_file);
    if (!document.has_value()) {
      continue;
    }
    SCOPED_TRACE(test_file.short_path);
    try {
      round_trip(*document, saved_root / test_file.short_path);
    } catch (const std::exception &e) {
      ADD_FAILURE() << e.what();
    }
    ++saved;
  }

  if (saved == 0) {
    GTEST_SKIP() << "No editable corpus files available";
  }
}
