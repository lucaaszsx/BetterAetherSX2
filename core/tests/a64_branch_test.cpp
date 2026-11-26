#include "bax/jit/a64_branch.h"

#include <gtest/gtest.h>

namespace bax::jit {
namespace {

constexpr std::uintptr_t SITE = 0x7000'0000;

// Expected words are from the Arm Architecture Reference Manual's B encoding (0b000101 followed by imm26).
TEST(EncodeBranch, JumpsForward) {
    EXPECT_EQ(encodeBranch(SITE, SITE + 8), 0x14000002u);
}

TEST(EncodeBranch, JumpsBackward) {
    EXPECT_EQ(encodeBranch(SITE, SITE - 4), 0x17ffffffu);
}

TEST(EncodeBranch, JumpsToItself) {
    EXPECT_EQ(encodeBranch(SITE, SITE), 0x14000000u);
}

TEST(EncodeBranch, ReachesBothEndsOfTheRange) {
    EXPECT_EQ(encodeBranch(SITE, SITE + BRANCH_RANGE_BYTES - 4), 0x15ffffffu);
    EXPECT_EQ(encodeBranch(SITE, SITE - BRANCH_RANGE_BYTES), 0x16000000u);
}

TEST(EncodeBranch, RefusesTargetsOutOfRange) {
    EXPECT_FALSE(encodeBranch(SITE, SITE + BRANCH_RANGE_BYTES));
    EXPECT_FALSE(encodeBranch(SITE, SITE - BRANCH_RANGE_BYTES - 4));
}

TEST(EncodeBranch, RefusesMisalignedTargets) {
    EXPECT_FALSE(encodeBranch(SITE, SITE + 2));
}

} // namespace
} // namespace bax::jit
