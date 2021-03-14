#include <gtest/gtest.h>
#include <odr/document.h>
#include <odr/exceptions.h>

using namespace odr;

TEST(Document, open) { EXPECT_THROW(Document("/"), FileNotFound); }

TEST(DocumentNoExcept, open) { EXPECT_FALSE(DocumentNoExcept::open("/")); }
