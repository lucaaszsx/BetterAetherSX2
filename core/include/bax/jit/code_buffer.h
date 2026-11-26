#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace bax::jit {

// Memory for generated code that is never writable and executable at the same address. One shared memory file is
// mapped twice: the emitter writes through the writable view, and the CPU runs the same bytes from the executable
// view. This works on systems that refuse pages that are both writable and executable.
//
// Offsets are the same in both views. Code addresses that other code jumps to, or that a fault handler sees as the
// faulting PC, are always executable-view addresses.
class CodeBuffer {
public:
    // Maps size bytes, rounded up to whole pages. Returns nothing and sets error when the system refuses.
    static std::optional<CodeBuffer> create(std::size_t size, std::string& error);

    CodeBuffer(CodeBuffer&& other) noexcept;
    CodeBuffer& operator=(CodeBuffer&& other) noexcept;
    CodeBuffer(const CodeBuffer&) = delete;
    CodeBuffer& operator=(const CodeBuffer&) = delete;
    ~CodeBuffer();

    std::size_t size() const { return size_; }
    std::uint8_t* writable() const { return writable_; }
    const std::uint8_t* executable() const { return executable_; }

    // Whether an executable-view address lies inside the buffer.
    bool contains(const void* executable_address) const;

    // The writable address of the byte at an executable-view address inside the buffer.
    std::uint8_t* writableAt(const void* executable_address) const;

    // Makes code written at [offset, offset + length) visible to instruction fetch. Call it after every write and
    // before the code runs; on AArch64 the data and instruction caches are not coherent.
    void flush(std::size_t offset, std::size_t length) const;

private:
    CodeBuffer(std::uint8_t* writable, std::uint8_t* executable, std::size_t size);
    void release();

    std::uint8_t* writable_ = nullptr;
    std::uint8_t* executable_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace bax::jit
