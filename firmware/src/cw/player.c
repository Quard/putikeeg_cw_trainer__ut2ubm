#include "cw/player.h"

#include "cw/morse.h"
#include "cw/player_shared.h"
#include "cw/timing.h"
#include "mcu/compiler.h"
#include "sidetone/sidetone.h"

/* The C start-up RAM clear leaves the player stopped, so there is no init function (D-T13). */
volatile MEM_BIT player_is_enabled;
volatile MEM_BIT player_is_start_requested;
volatile MEM_XDATA uint8_t player_codes[PLAYER_TEXT_MAX];
volatile MEM_XDATA uint8_t player_code_count;
volatile MEM_XDATA uint16_t player_dit_ticks;
volatile MEM_XDATA uint16_t player_dah_ticks;
volatile MEM_XDATA uint16_t player_char_gap_ticks;

/* Work values in xdata, so the calls below need no internal RAM to keep them. */
static MEM_XDATA uint8_t start_index;
static MEM_XDATA uint8_t start_pattern;
static MEM_XDATA uint8_t start_code;

bool player_start(const char MEM_XDATA* text, MEM_XDATA uint8_t length, MEM_XDATA uint8_t wpm,
                  MEM_XDATA uint8_t eff_wpm) {
  /* From here the interrupt skips the player, so the shared values can change. */
  player_stop();
  if ((length == 0U) || (length > PLAYER_TEXT_MAX)) {
    return false;
  }
  for (start_index = 0U; start_index < length; ++start_index) {
    start_pattern = morse_encode(text[start_index]);
    if (start_pattern == MORSE_PATTERN_INVALID) {
      return false;
    }
    /* The pattern has the first element in its highest bit below the start marker; the play form needs it in bit 0. */
    start_code = PLAYER_CODE_DONE;
    while (start_pattern > MORSE_PATTERN_EMPTY) {
      start_code = (uint8_t)((uint8_t)(start_code << 1) | (start_pattern & 1U));
      start_pattern >>= 1;
    }
    player_codes[start_index] = start_code;
  }
  cw_timing_compute(wpm, eff_wpm);
  player_dit_ticks = cw_timing_dit_ticks;
  player_dah_ticks = (uint16_t)(3U * cw_timing_dit_ticks);
  player_char_gap_ticks = (uint16_t)(PLAYER_CHAR_GAP_UNITS * cw_timing_unit_ticks);
  player_code_count = length;
  /* Volatile stores, so C keeps all stores above before these two. */
  player_is_start_requested = true;
  player_is_enabled = true;
  return true;
}

bool player_is_busy(void) { return player_is_enabled; }

void player_stop(void) {
  player_is_enabled = false;
  player_is_start_requested = false;
  sidetone_set_on(false);
}
