#include "bax/version.h"

#include <algorithm>
#include <gtest/gtest.h>

TEST(CoreVersion, FollowsSemanticVersioning) {
    const std::string_view version = bax::coreVersion();
    ASSERT_FALSE(version.empty());
    EXPECT_EQ(std::count(version.begin(), version.end(), '.'), 2);
}
