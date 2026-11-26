#include "bax/jit/code_buffer.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>

namespace bax::jit {
namespace {

// Not a whole number of pages, to see the rounding; above one page, to reach a second one.
constexpr std::size_t REQUESTED_SIZE = 10000;
constexpr std::size_t SECOND_PAGE_OFFSET = 4096;
constexpr std::size_t SOME_OFFSET = 100;

class CodeBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string error;
        maybe_buffer = CodeBuffer::create(REQUESTED_SIZE, error);
        ASSERT_TRUE(maybe_buffer) << error;
    }

    CodeBuffer& buffer() { return *maybe_buffer; }

private:
    std::optional<CodeBuffer> maybe_buffer;
};

// The permission field ("r-xs", "rw-s", ...) of the /proc/self/maps line that maps address.
std::string permissionsAt(const void* address) {
    const auto target = reinterpret_cast<std::uintptr_t>(address);
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream fields(line);
        std::uintptr_t begin = 0;
        std::uintptr_t end = 0;
        char dash = 0;
        std::string permissions;
        fields >> std::hex >> begin >> dash >> end >> permissions;
        if (target >= begin && target < end) return permissions;
    }
    return "";
}

TEST_F(CodeBufferTest, RoundsTheSizeUpToWholePages) {
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    EXPECT_GE(buffer().size(), REQUESTED_SIZE);
    EXPECT_EQ(buffer().size() % page, 0u);
}

TEST(CodeBuffer, RefusesAnEmptyBuffer) {
    std::string error;
    EXPECT_FALSE(CodeBuffer::create(0, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(CodeBufferTest, ShowsWritesInTheExecutableView) {
    constexpr std::array<std::uint8_t, 4> BYTES = {0x12, 0x34, 0x56, 0x78};
    std::memcpy(buffer().writable() + SOME_OFFSET, BYTES.data(), BYTES.size());
    EXPECT_EQ(std::memcmp(buffer().executable() + SOME_OFFSET, BYTES.data(), BYTES.size()), 0);
}

TEST_F(CodeBufferTest, NeverMapsAPageWritableAndExecutable) {
    EXPECT_EQ(permissionsAt(buffer().writable()), "rw-s");
    EXPECT_EQ(permissionsAt(buffer().executable()), "r-xs");
}

TEST_F(CodeBufferTest, FindsTheWritableAddressOfExecutableCode) {
    EXPECT_EQ(buffer().writableAt(buffer().executable() + SECOND_PAGE_OFFSET),
              buffer().writable() + SECOND_PAGE_OFFSET);
}

TEST_F(CodeBufferTest, ContainsOnlyItsExecutableView) {
    EXPECT_TRUE(buffer().contains(buffer().executable()));
    EXPECT_TRUE(buffer().contains(buffer().executable() + buffer().size() - 1));
    EXPECT_FALSE(buffer().contains(buffer().executable() + buffer().size()));
    EXPECT_FALSE(buffer().contains(buffer().writable()));
}

TEST_F(CodeBufferTest, KeepsTheMappingWhenMoved) {
    constexpr std::uint8_t MARKER = 0x5a;
    buffer().writable()[0] = MARKER;
    const CodeBuffer moved = std::move(buffer());
    EXPECT_EQ(moved.executable()[0], MARKER);
}

// A function that returns at once, in the host's machine code.
#if defined(__x86_64__)
constexpr std::array<std::uint8_t, 1> RETURN = {0xc3};
#elif defined(__aarch64__)
constexpr std::array<std::uint8_t, 4> RETURN = {0xc0, 0x03, 0x5f, 0xd6};
#endif

#if defined(__x86_64__) || defined(__aarch64__)
TEST_F(CodeBufferTest, RunsCodeWrittenThroughTheWritableView) {
    std::memcpy(buffer().writable(), RETURN.data(), RETURN.size());
    buffer().flush(0, RETURN.size());
    const auto function = reinterpret_cast<void (*)()>(const_cast<std::uint8_t*>(buffer().executable()));
    function();
    SUCCEED();
}
#endif

} // namespace
} // namespace bax::jit
