#include <gtest/gtest.h>

// Single Test-Cases - TEST(test_suite_name, test_name)

TEST(ExampleTest, willsucceed) {
  EXPECT_EQ(5, 5);
  EXPECT_TRUE(true);
}