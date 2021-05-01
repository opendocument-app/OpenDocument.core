#include <fstream>
#include <gtest/gtest.h>
#include <internal/common/file.h>
#include <internal/svm/svm_file.h>
#include <internal/svm/svm_to_svg.h>
#include <sstream>
#include <string>
#include <test_util.h>

using namespace odr::internal;
using namespace odr::test;

TEST(SvmFile, open) {
  svm::SvmFile svm(std::make_shared<common::DiscFile>(
      TestData::test_file_path("odr-public/svm/chart-1.svm")));

  EXPECT_EQ(odr::FileType::STARVIEW_METAFILE, svm.file_type());
}

TEST(SvmToSvg, string) {
  svm::SvmFile svm(std::make_shared<common::DiscFile>(
      TestData::test_file_path("odr-public/svm/table-1.svm")));

  std::stringstream out;
  svm::Translator::svg(svm, out);
  auto out_string = out.str();

  EXPECT_LT(0, out_string.size());

  // std::ofstream test("/home/andreas/test.svg");
  // test << out_string;
}
