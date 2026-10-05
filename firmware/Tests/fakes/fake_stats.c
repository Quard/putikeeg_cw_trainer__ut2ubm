#include "fake_menu_outputs.h"
#include "stats/stats.h"

static size_t fake_resets;

void fake_stats_reset(void) { fake_resets = 0U; }

size_t fake_stats_reset_count(void) { return fake_resets; }

void stats_reset(void) { ++fake_resets; }
