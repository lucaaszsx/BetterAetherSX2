// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string_view>

// Keeps the latest error the core reported, so a session that ends because of it can tell the app why.
void recordCoreError(std::string_view message);
