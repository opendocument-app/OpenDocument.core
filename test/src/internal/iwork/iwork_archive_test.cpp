#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_budget.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <internal/iwork/iwork_test_util.hpp>

#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::iwork;
using odr::internal::AbsPath;
namespace builder = odr::test::iwork;

using builder::message_field;
using builder::number_field;
using builder::object;

TEST(ReadObjects, one_object) {
  const std::string data = object(1, {{10000, 5}}, "hello");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].identifier, 1);
  EXPECT_EQ(objects[0].type, 10000);
  EXPECT_EQ(objects[0].payload, "hello");
}

TEST(ReadObjects, objects_follow_one_another) {
  const std::string data =
      object(1, {{10000, 5}}, "hello") + object(1732514, {{2001, 5}}, "world");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 2);
  EXPECT_EQ(objects[1].identifier, 1732514);
  EXPECT_EQ(objects[1].type, 2001);
  EXPECT_EQ(objects[1].payload, "world");
}

// An object may hold more than one message; only the first is modelled, and
// the length of the rest is what keeps the reader in step.
TEST(ReadObjects, later_messages_are_skipped) {
  const std::string data = object(1732594, {{6247, 3}, {6247, 3}}, "onetwo") +
                           object(2, {{222, 1}}, "x");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 2);
  EXPECT_EQ(objects[0].type, 6247);
  EXPECT_EQ(objects[0].payload, "one");
  EXPECT_EQ(objects[1].identifier, 2);
}

// There is no spec and no schema registry, so a type we have not mapped is an
// app version we have not seen — the reader keeps it and moves on.
TEST(ReadObjects, unknown_type_is_kept) {
  const std::string data = object(7, {{123456, 1}}, "x");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].type, 123456);
}

TEST(ReadObjects, empty_payload) {
  const std::string data = object(1732550, {{3047, 0}}, "");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 1);
  EXPECT_TRUE(objects[0].payload.empty());
}

TEST(ReadObjects, empty_component) { EXPECT_TRUE(read_objects("").empty()); }

TEST(ReadObjects, archive_info_runs_past_the_component) {
  const std::string data = object(1, {{10000, 5}}, "hello");
  EXPECT_ANY_THROW(std::ignore = read_objects(data.substr(0, 4)));
}

TEST(ReadObjects, payload_runs_past_the_component) {
  EXPECT_ANY_THROW(
      std::ignore = read_objects(object(
          1, {{10000, std::numeric_limits<std::size_t>::max()}, {10000, 1}},
          "")));
  const std::string data = object(1, {{10000, 500}}, "hello");
  EXPECT_ANY_THROW(std::ignore = read_objects(data));
}

TEST(ReadIwa, undoes_the_framing) {
  const auto files =
      builder::filesystem({{"/Index/Document.iwa", builder::iwa("hello")}});

  EXPECT_EQ(read_iwa(*files, AbsPath("/Index/Document.iwa")), "hello");
}

TEST(ReadIwa, missing_file) {
  const auto files = builder::filesystem({});

  EXPECT_ANY_THROW(std::ignore =
                       read_iwa(*files, AbsPath("/Index/Document.iwa")));
}

TEST(IworkPackage, loads_a_component_by_name) {
  const auto files =
      builder::package({{"Document", object(1, {{10000, 5}}, "hello")}});

  Package package(*files);
  const std::vector<Object> &objects = package.component("Document").objects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].payload, "hello");
  EXPECT_EQ(package.object(1).type, 10000);
}

TEST(IworkPackage, missing_metadata) {
  const auto files = builder::filesystem({});

  EXPECT_ANY_THROW(Package{*files});
}

TEST(IworkPackage, empty_metadata) {
  const auto files =
      builder::filesystem({{"/Index/Metadata.iwa", builder::iwa("")}});

  EXPECT_ANY_THROW(Package{*files});
}

// A varint where a component info belongs is a package we cannot read.
TEST(IworkPackage, malformed_component_info) {
  const std::string list =
      number_field(builder::package_metadata_components, 1);
  const auto files = builder::filesystem(
      {{"/Index/Metadata.iwa",
        builder::iwa(object(2, {{builder::package_metadata_type, list.size()}},
                            list))}});

  EXPECT_ANY_THROW(Package{*files});
}

TEST(IworkPackage, component_without_a_name) {
  const std::string list =
      message_field(builder::package_metadata_components,
                    message_field(builder::component_info_locator, "Document"));
  const auto files = builder::filesystem(
      {{"/Index/Metadata.iwa",
        builder::iwa(object(2, {{builder::package_metadata_type, list.size()}},
                            list))}});

  EXPECT_ANY_THROW(Package{*files});
}

TEST(IworkPackage, no_component_of_that_name) {
  const auto files =
      builder::package({{"Document", object(1, {{10000, 5}}, "hello")}});

  Package package(*files);
  EXPECT_ANY_THROW(package.component("Stylesheet"));
}

TEST(IworkPackage, no_object_of_that_identifier) {
  const auto files =
      builder::package({{"Document", object(1, {{10000, 5}}, "hello")}});

  Package package(*files);
  EXPECT_ANY_THROW(package.object(1732514));
}

// A component the list names but the package does not hold is broken framing.
TEST(IworkPackage, component_file_is_missing) {
  const auto files = builder::filesystem(
      {{"/Index/Metadata.iwa",
        builder::iwa(builder::package_metadata({"Document"}))}});

  Package package(*files);
  EXPECT_ANY_THROW(package.component("Document"));
}

TEST(ReadObjects, wide_unknown_types_do_not_alias_known_types) {
  constexpr std::uint64_t type = (std::uint64_t{1} << 32) + 10000;
  const std::string info =
      number_field(1, 1) +
      message_field(2, number_field(1, type) + number_field(3, 0));
  const std::string data = builder::varint(info.size()) + info;
  const auto objects = read_objects(data);
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].type, type);
}

TEST(IworkBudget, overflow_does_not_reset_text_usage) {
  Budget budget;
  budget.spend_text(1);
  EXPECT_THROW(budget.spend_text(std::numeric_limits<std::size_t>::max()),
               std::runtime_error);
  budget.spend_text(64 * 1024 * 1024 - 1);
  EXPECT_THROW(budget.spend_text(1), std::runtime_error);
}
