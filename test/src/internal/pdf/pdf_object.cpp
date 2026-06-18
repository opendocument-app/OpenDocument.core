#include <odr/internal/pdf/pdf_object.hpp>

#include <optional>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

// A default-constructed Object holds nothing and is the canonical null.
TEST(PdfObject, null) {
  const Object object;
  EXPECT_TRUE(object.is_null());
  EXPECT_FALSE(object.is_bool());
  EXPECT_FALSE(object.is_integer());
  EXPECT_FALSE(object.is_string());
  EXPECT_TRUE(Object::null().is_null());
}

TEST(PdfObject, type_predicates_are_exclusive) {
  EXPECT_TRUE(Object(Boolean{true}).is_bool());
  EXPECT_TRUE(Object(Integer{1}).is_integer());
  EXPECT_TRUE(Object(Real{1.5}).is_real());
  EXPECT_TRUE(Object(StandardString{"x"}).is_standard_string());
  EXPECT_TRUE(Object(HexString{"x"}).is_hex_string());
  EXPECT_TRUE(Object(Name{"x"}).is_name());
  EXPECT_TRUE(Object(Array{}).is_array());
  EXPECT_TRUE(Object(Dictionary{}).is_dictionary());
  EXPECT_TRUE(Object(ObjectReference(1, 0)).is_reference());

  // a bool is not an integer and an integer is not a bool
  EXPECT_FALSE(Object(Boolean{true}).is_integer());
  EXPECT_FALSE(Object(Integer{1}).is_bool());
}

// is_real() and as_real() accept an integer; is_integer() does not accept a
// real.
TEST(PdfObject, integer_promotes_to_real) {
  const Object integer(Integer{42});
  EXPECT_TRUE(integer.is_real());
  EXPECT_DOUBLE_EQ(integer.as_real(), 42.0);

  const Object real(Real{1.5});
  EXPECT_FALSE(real.is_integer());
}

TEST(PdfObject, as_throws_on_type_mismatch) {
  const Object object(Integer{1});
  EXPECT_EQ(object.as_integer(), 1);
  EXPECT_ANY_THROW((void)object.as_bool());
  EXPECT_ANY_THROW((void)object.as_array());
}

// as_string() and is_string() span all three string-like types.
TEST(PdfObject, as_string_spans_string_types) {
  EXPECT_EQ(Object(StandardString{"a"}).as_string(), "a");
  EXPECT_EQ(Object(HexString{"b"}).as_string(), "b");
  EXPECT_EQ(Object(Name{"c"}).as_string(), "c");

  const Object integer(Integer{1});
  EXPECT_FALSE(integer.is_string());
}

// as_<T>_opt() returns the value on an exact type match and nullopt otherwise.
TEST(PdfObject, value_opt_matches_exact_type) {
  EXPECT_EQ(Object(Boolean{true}).as_bool_opt(), std::optional<Boolean>{true});
  EXPECT_EQ(Object(Integer{7}).as_integer_opt(), std::optional<Integer>{7});
  EXPECT_EQ(Object(ObjectReference(3, 1)).as_reference_opt(),
            std::optional<ObjectReference>{ObjectReference(3, 1)});

  EXPECT_FALSE(Object(Integer{1}).as_bool_opt().has_value());
  EXPECT_FALSE(Object().as_integer_opt().has_value());
}

// as_real_opt() mirrors as_real(): it accepts an integer but yields nullopt for
// unrelated types.
TEST(PdfObject, real_opt_accepts_integer) {
  EXPECT_EQ(Object(Real{1.5}).as_real_opt(), std::optional<Real>{1.5});
  EXPECT_EQ(Object(Integer{2}).as_real_opt(), std::optional<Real>{2.0});
  EXPECT_FALSE(Object(Name{"x"}).as_real_opt().has_value());
  EXPECT_FALSE(Object().as_real_opt().has_value());
}

// The string _opt accessors are rvalue-only: they move the held string out.
TEST(PdfObject, string_opt_matches_exact_type) {
  EXPECT_EQ(Object(StandardString{"a"}).as_standard_string_opt(),
            std::optional<std::string>{"a"});
  EXPECT_EQ(Object(HexString{"b"}).as_hex_string_opt(),
            std::optional<std::string>{"b"});
  EXPECT_EQ(Object(Name{"c"}).as_name_opt(), std::optional<std::string>{"c"});

  // a hex string is not a standard string
  EXPECT_FALSE(Object(HexString{"b"}).as_standard_string_opt().has_value());
}

// as_string_opt() spans all three string types, like as_string().
TEST(PdfObject, as_string_opt_spans_string_types) {
  EXPECT_EQ(Object(StandardString{"a"}).as_string_opt(),
            std::optional<std::string>{"a"});
  EXPECT_EQ(Object(HexString{"b"}).as_string_opt(),
            std::optional<std::string>{"b"});
  EXPECT_EQ(Object(Name{"c"}).as_string_opt(), std::optional<std::string>{"c"});

  EXPECT_FALSE(Object(Integer{1}).as_string_opt().has_value());
}

// The container accessors return a pointer (nullptr on mismatch) to avoid
// copying the array/dictionary.
TEST(PdfObject, container_ptr_matches_exact_type) {
  Array array;
  array.holder().emplace_back(Integer{1});
  const Object object(std::move(array));

  const Array *array_ptr = object.as_array_ptr();
  ASSERT_NE(array_ptr, nullptr);
  EXPECT_EQ(array_ptr->size(), 1u);

  EXPECT_EQ(object.as_dictionary_ptr(), nullptr);
  EXPECT_EQ(Object().as_array_ptr(), nullptr);
}

// The generic as_ptr<T>()/as_opt<T>() back the named accessors.
TEST(PdfObject, generic_nullable_accessors) {
  const Object object(Integer{5});
  ASSERT_NE(object.as_ptr<Integer>(), nullptr);
  EXPECT_EQ(*object.as_ptr<Integer>(), 5);
  EXPECT_EQ(object.as_ptr<Boolean>(), nullptr);

  EXPECT_EQ(object.as_opt<Integer>(), std::optional<Integer>{5});
  EXPECT_FALSE(object.as_opt<Boolean>().has_value());
}

TEST(PdfObject, object_reference_ordering_and_equality) {
  EXPECT_EQ(ObjectReference(1, 0), ObjectReference(1, 0));
  EXPECT_FALSE(ObjectReference(1, 0) == ObjectReference(1, 1));
  // ordered by id first, then generation
  EXPECT_TRUE(ObjectReference(1, 5) < ObjectReference(2, 0));
  EXPECT_TRUE(ObjectReference(1, 0) < ObjectReference(1, 1));

  std::hash<ObjectReference> hash;
  EXPECT_EQ(hash(ObjectReference(1, 0)), hash(ObjectReference(1, 0)));
}

// get() returns the default for a missing key without inserting it; has_value()
// treats an explicit null as absent.
TEST(PdfObject, dictionary_lookup) {
  Dictionary dictionary;
  dictionary["A"] = Object(Integer{1});
  dictionary["B"] = Object();

  EXPECT_TRUE(dictionary.has_key("A"));
  EXPECT_TRUE(dictionary.has_value("A"));
  EXPECT_TRUE(dictionary.has_key("B"));
  EXPECT_FALSE(dictionary.has_value("B"));
  EXPECT_FALSE(dictionary.has_key("C"));

  EXPECT_EQ(dictionary.get("A").as_integer(), 1);
  EXPECT_TRUE(dictionary.get("C").is_null());
  EXPECT_FALSE(dictionary.has_key("C"));
}

TEST(PdfObject, to_string) {
  EXPECT_EQ(Object().to_string(), "null");
  EXPECT_EQ(Object(Boolean{true}).to_string(), "true");
  EXPECT_EQ(Object(Integer{42}).to_string(), "42");
  EXPECT_EQ(Object(Name{"Type"}).to_string(), "/Type");
  EXPECT_EQ(Object(ObjectReference(12, 0)).to_string(), "12 0 R");
}
