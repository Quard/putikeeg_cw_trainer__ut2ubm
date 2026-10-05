#include "cw/keyer.h"

#include "board/paddle_pins.h"
#include "cw/keyer_shared.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"

#if (KEYER_KEY_STRAIGHT != SETTINGS_KEY_STRAIGHT) || (KEYER_KEY_IAMBIC_A != SETTINGS_KEY_IAMBIC_A) || \
    (KEYER_KEY_IAMBIC_B != SETTINGS_KEY_IAMBIC_B)
#error "The keyer key types must equal the settings key types"
#endif
/* The Farnsworth factors share this factor, so the math in keyer_start() needs only one 32-bit step. */
#define KEYER_FARNSWORTH_SCALE 2400UL
#if ((KEYER_FARNSWORTH_C_FACTOR % KEYER_FARNSWORTH_SCALE) != 0UL) || \
    ((KEYER_FARNSWORTH_S_FACTOR % KEYER_FARNSWORTH_SCALE) != 0UL)
#error "KEYER_FARNSWORTH_SCALE must divide both Farnsworth factors"
#endif
/* The early release must come after the rise, and ZERO_WAIT must end before the next element starts. */
#if ((KEYER_DIT_TICKS_NUMERATOR + (KEYER_WPM_MAX / 2U)) / KEYER_WPM_MAX) <= (2U * SIDETONE_RAMP_STEPS)
#error "The dit at KEYER_WPM_MAX must be longer than two sidetone ramps"
#endif

volatile MEM_BIT keyer_is_enabled;
volatile MEM_BIT keyer_is_start_requested;
volatile MEM_BIT keyer_is_overflow;
volatile MEM_BIT keyer_is_straight;
volatile MEM_BIT keyer_is_iambic_a;
volatile MEM_BIT keyer_is_swapped;
volatile MEM_DATA uint8_t keyer_queue_head;
volatile MEM_DATA uint8_t keyer_queue_tail;
volatile MEM_XDATA uint8_t keyer_queue[KEYER_QUEUE_SIZE];
volatile MEM_XDATA uint16_t keyer_dit_ticks;
volatile MEM_XDATA uint16_t keyer_dah_ticks;
volatile MEM_XDATA uint16_t keyer_two_dit_ticks;
volatile MEM_XDATA uint16_t keyer_word_gap_ticks;

/*
 * Work values of keyer_start() in xdata. Volatile, so SDCC stores each step here and keeps no temporary in internal
 * RAM.
 */
static volatile MEM_XDATA uint8_t start_key_type;
static volatile MEM_XDATA uint16_t start_dit;
static volatile MEM_XDATA uint16_t start_denominator;
static volatile MEM_XDATA uint32_t start_unit;

void keyer_init(void) {
  keyer_is_enabled = false;
  keyer_is_start_requested = false;
  PADDLE_PINS_CONFIGURE();
  keyer_queue_tail = keyer_queue_head;
  keyer_is_overflow = false;
}

void keyer_start(uint8_t key_type, MEM_XDATA bool is_swapped, MEM_XDATA uint8_t wpm, MEM_XDATA uint8_t eff_wpm) {
  /* Kept in xdata, so SDCC needs no internal RAM to hold it across the calls below. */
  start_key_type = key_type;
  /* From here the interrupt skips the keyer, so the parameters can change. */
  keyer_stop();
  if (wpm < KEYER_WPM_MIN) {
    wpm = KEYER_WPM_MIN;
  } else if (wpm > KEYER_WPM_MAX) {
    wpm = KEYER_WPM_MAX;
  }
  if (eff_wpm < KEYER_WPM_MIN) {
    eff_wpm = KEYER_WPM_MIN;
  } else if (eff_wpm > wpm) {
    eff_wpm = wpm;
  }

  /* dit = round(4800 / wpm) calls = 1200 / wpm ms at 4000 calls per second; 960..96 at 5..50 WPM. */
  start_dit = (uint16_t)(((uint16_t)KEYER_DIT_TICKS_NUMERATOR + (uint16_t)(wpm / 2U)) / wpm);
  keyer_dit_ticks = start_dit;
  keyer_dah_ticks = (uint16_t)(3U * start_dit);
  keyer_two_dit_ticks = (uint16_t)(KEYER_CHAR_GAP_DITS * start_dit);

  /*
   * Farnsworth unit fu = (60 c - 37.2 s) / (19 c s) seconds, with c = wpm and s = eff_wpm. In calls:
   * round((240000 c - 148800 s) / (19 c s)) = round(2400 (100 c - 62 s) / (19 c s)). For 5 <= s <= c <= 50,
   * 100 c - 62 s is 190..4690 and 19 c s is at most 47 500, so both fit uint16_t, and the numerator, at most
   * 11 256 000, fits uint32_t. fu = dit for s = c; the clamp catches a rounding below dit. Word gap at most
   * 11 850 calls.
   */
  start_denominator = (uint16_t)((uint16_t)(KEYER_FARNSWORTH_C_FACTOR / KEYER_FARNSWORTH_SCALE) * wpm);
  start_denominator -= (uint16_t)((uint16_t)(KEYER_FARNSWORTH_S_FACTOR / KEYER_FARNSWORTH_SCALE) * eff_wpm);
  start_unit = KEYER_FARNSWORTH_SCALE * start_denominator;
  start_denominator = (uint16_t)((uint16_t)wpm * eff_wpm);
  start_denominator = (uint16_t)(start_denominator * (uint16_t)KEYER_FARNSWORTH_DIVISOR);
  start_unit += (uint16_t)(start_denominator / 2U);
  start_unit /= start_denominator;
  if (start_unit < start_dit) {
    start_unit = start_dit;
  }
  keyer_word_gap_ticks = (uint16_t)(KEYER_WORD_GAP_UNITS * (uint16_t)start_unit);

  keyer_is_straight = (start_key_type == KEYER_KEY_STRAIGHT);
  keyer_is_iambic_a = (start_key_type == KEYER_KEY_IAMBIC_A);
  keyer_is_swapped = is_swapped;
  /* The interrupt does not write the head while the keyer is stopped. */
  keyer_queue_tail = keyer_queue_head;
  keyer_is_overflow = false;
  /* Volatile stores, so C keeps all stores above before these two. */
  keyer_is_start_requested = true;
  keyer_is_enabled = true;
}

void keyer_stop(void) {
  keyer_is_enabled = false;
  keyer_is_start_requested = false;
  sidetone_set_on(false);
}

keyer_event_t keyer_read_event(void) {
  keyer_event_t event;

  if (keyer_queue_tail != keyer_queue_head) {
    /* The slot read comes before the tail moves, so the interrupt cannot reuse the slot first. */
    event = keyer_queue[keyer_queue_tail & KEYER_QUEUE_MASK];
    ++keyer_queue_tail;
    return event;
  }
  /* The interrupt drops every event while the flag is set, so all events from before the loss are read. */
  if (keyer_is_overflow) {
    keyer_is_overflow = false;
    return KEYER_EVENT_OVERFLOW;
  }
  return KEYER_EVENT_NONE;
}
