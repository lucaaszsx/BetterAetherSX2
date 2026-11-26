// SPDX-License-Identifier: GPL-3.0-or-later
// Block links for upstream's BaseBlocks: a link site lies in the code memory of the recompiler that compiled it, and
// is rewritten through that memory's writable view.

#include "bax/jit/a64_branch.h"
#include "common/Assertions.h"
#include "ee/ee_rec.h"
#include "x86/BaseblockEx.h"

#include <cstring>

namespace {

bool setBranchTarget(const bax::jit::CodeBuffer* code, uptr site, uptr target) {
    if (!code || !code->contains(reinterpret_cast<const void*>(site))) return false;
    const std::optional<u32> branch = bax::jit::encodeBranch(site, target);
    pxAssertRel(branch.has_value(), "A block link is outside the range of a direct branch.");
    std::memcpy(code->writableAt(reinterpret_cast<const void*>(site)), &*branch, sizeof(u32));
    code->flush(site - reinterpret_cast<uptr>(code->executable()), sizeof(u32));
    return true;
}

} // namespace

void BaseBlocks_SetBranchTarget(uptr site, uptr target) {
    const bool linked = setBranchTarget(bax::jit::ee::codeBuffer(), site, target);
    pxAssertRel(linked, "A block link lies outside the recompiler's code memory.");
}
