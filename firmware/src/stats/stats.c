#include "stats/stats.h"

#include "progress/progress.h"

bool stats_reset(void) { return progress_reset() == PROGRESS_OK; }
