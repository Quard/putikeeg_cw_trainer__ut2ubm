#include "cw/keyer.h"

#include "board/paddle_pins.h"
#include "cw/keyer_shared.h"
#include "cw/timing.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"

#if (KEYER_KEY_STRAIGHT != SETTINGS_KEY_STRAIGHT) || (KEYER_KEY_IAMBIC_A != SETTINGS_KEY_IAMBIC_A) || \
    (KEYER_KEY_IAMBIC_B != SETTINGS_KEY_IAMBIC_B)
#error "The keyer key types must equal the settings key types"
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

/* Work value of keyer_start() in xdata. Volatile, so SDCC keeps no copy in internal RAM. */
static volatile MEM_XDATA uint8_t start_key_type;

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
  cw_timing_compute(wpm, eff_wpm);
  keyer_dit_ticks = cw_timing_dit_ticks;
  keyer_dah_ticks = (uint16_t)(3U * cw_timing_dit_ticks);
  keyer_two_dit_ticks = (uint16_t)(KEYER_CHAR_GAP_DITS * cw_timing_dit_ticks);
  keyer_word_gap_ticks = (uint16_t)(KEYER_WORD_GAP_UNITS * cw_timing_unit_ticks);

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
