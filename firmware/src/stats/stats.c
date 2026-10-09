/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stats/stats.h"

#include "progress/progress.h"

bool stats_reset(void) { return progress_reset() == PROGRESS_OK; }
