// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"
#include "iop_cases.h"

namespace bax::difftest {

// Runs `cases` IOP cases of one class on the IOP interpreter and on the candidate engine, compares GPRs, HI, LO and
// the data area, and returns the process exit code: 0 when they agree on every case, 1 on the first divergence.
// `mutate` flips a bit of the candidate's result in every case, which must fail.
int runIopCases(const char* class_name, IopClass iop_class, u64 cases, u64 seed, bool mutate);

} // namespace bax::difftest
