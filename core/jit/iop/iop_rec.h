// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bax/jit/code_buffer.h"

namespace bax::jit::iop {

// The IOP recompiler's code memory, or null when psxRec.Reserve could not get it.
const CodeBuffer* codeBuffer();

} // namespace bax::jit::iop
