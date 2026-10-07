#include "fake_menu_outputs.h"
#include "stats/stats.h"

static size_t fake_resets;
static bool fake_is_reset_stored = true;

void fake_stats_reset(void) {
  fake_resets = 0U;
  fake_is_reset_stored = true;
}

size_t fake_stats_reset_count(void) { return fake_resets; }

void fake_stats_set_reset_result(bool is_stored) { fake_is_reset_stored = is_stored; }

bool stats_reset(void) {
  ++fake_resets;
  return fake_is_reset_stored;
}
