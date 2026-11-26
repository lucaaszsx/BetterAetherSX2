#include "bax/jit/code_buffer.h"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

namespace bax::jit {
namespace {

std::string describeFailure(const char* what) {
    return std::string(what) + " failed: " + std::strerror(errno);
}

// The libc wrapper for memfd_create needs Android API 30; the app supports API 29, where the system call exists.
int createMemoryFile() {
    return static_cast<int>(syscall(SYS_memfd_create, "bax-jit", MFD_CLOEXEC));
}

std::size_t roundUpToPages(std::size_t size) {
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    return (size + page - 1) / page * page;
}

} // namespace

std::optional<CodeBuffer> CodeBuffer::create(std::size_t size, std::string& error) {
    if (size == 0) {
        error = "A code buffer needs a size above zero.";
        return std::nullopt;
    }
    const std::size_t mapped_size = roundUpToPages(size);
    const int file = createMemoryFile();
    if (file < 0) {
        error = describeFailure("memfd_create");
        return std::nullopt;
    }
    if (ftruncate(file, static_cast<off_t>(mapped_size)) != 0) {
        error = describeFailure("ftruncate");
        close(file);
        return std::nullopt;
    }
    void* writable = mmap(nullptr, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (writable == MAP_FAILED) {
        error = describeFailure("mmap of the writable view");
        close(file);
        return std::nullopt;
    }
    void* executable = mmap(nullptr, mapped_size, PROT_READ | PROT_EXEC, MAP_SHARED, file, 0);
    if (executable == MAP_FAILED) {
        error = describeFailure("mmap of the executable view");
        munmap(writable, mapped_size);
        close(file);
        return std::nullopt;
    }
    // Both mappings keep the file alive.
    close(file);
    return CodeBuffer(static_cast<std::uint8_t*>(writable), static_cast<std::uint8_t*>(executable), mapped_size);
}

CodeBuffer::CodeBuffer(std::uint8_t* writable, std::uint8_t* executable, std::size_t size)
    : writable_(writable), executable_(executable), size_(size) {}

CodeBuffer::CodeBuffer(CodeBuffer&& other) noexcept
    : writable_(std::exchange(other.writable_, nullptr)), executable_(std::exchange(other.executable_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

CodeBuffer& CodeBuffer::operator=(CodeBuffer&& other) noexcept {
    if (this == &other) return *this;
    release();
    writable_ = std::exchange(other.writable_, nullptr);
    executable_ = std::exchange(other.executable_, nullptr);
    size_ = std::exchange(other.size_, 0);
    return *this;
}

CodeBuffer::~CodeBuffer() {
    release();
}

void CodeBuffer::release() {
    if (writable_) munmap(writable_, size_);
    if (executable_) munmap(executable_, size_);
}

bool CodeBuffer::contains(const void* executable_address) const {
    // As integers: the address may belong to another mapping, where pointer comparisons are unspecified.
    const auto address = reinterpret_cast<std::uintptr_t>(executable_address);
    const auto start = reinterpret_cast<std::uintptr_t>(executable_);
    return address >= start && address - start < size_;
}

std::uint8_t* CodeBuffer::writableAt(const void* executable_address) const {
    return writable_ + (static_cast<const std::uint8_t*>(executable_address) - executable_);
}

// Cleaning through the executable view also cleans what was written through the writable view: the views share
// physical pages, and AArch64 data caches behave as physically tagged.
void CodeBuffer::flush(std::size_t offset, std::size_t length) const {
    char* begin = reinterpret_cast<char*>(executable_ + offset);
    __builtin___clear_cache(begin, begin + length);
}

} // namespace bax::jit
