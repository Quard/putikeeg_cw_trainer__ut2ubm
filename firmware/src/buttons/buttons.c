#include "buttons/buttons.h"

#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "delay/delay.h"
#include "mcu/compiler.h"
#include "tick/tick.h"

#define BUTTON_COUNT 4U
#define BUTTON_REPEAT_MASK (BUTTON_BIT_NEXT | BUTTON_BIT_PREV)
#define BUTTON_LONG_MASK (BUTTON_BIT_NEXT | BUTTON_BIT_PREV | BUTTON_BIT_MENU)
/* Indexes 0..2 are NEXT, PREV and MENU, the buttons with a long press. */
#define BUTTON_LONG_COUNT 3U
#define BUTTON_ENTER_INDEX 3U

/* A repeat event is the repeat button bit itself. */
#if (BUTTON_BIT_NEXT != BUTTON_EVENT_NEXT) || (BUTTON_BIT_PREV != BUTTON_EVENT_PREV)
#error "The bits of the repeat buttons must equal their event codes"
#endif

#if (BUTTON_LONG_MASK != ((1U << BUTTON_LONG_COUNT) - 1U)) || (BUTTON_BIT_ENTER != (1U << BUTTON_ENTER_INDEX))
#error "The long-press buttons must have indexes 0..2 and ENTER index 3"
#endif

/* Button index i has bit 1 << i and event code i + 1 (BUTTON_EVENT_NEXT .. BUTTON_EVENT_ENTER). */
static const MEM_CODE uint8_t buttons_bits[BUTTON_COUNT] = {BUTTON_BIT_NEXT, BUTTON_BIT_PREV, BUTTON_BIT_MENU,
                                                            BUTTON_BIT_ENTER};
/* Pending presses come out in this index order: MENU, ENTER, PREV, NEXT. */
static const MEM_CODE uint8_t buttons_event_order[BUTTON_COUNT] = {2U, 3U, 1U, 0U};
/* Long presses come out in this index order: MENU, PREV, NEXT. */
static const MEM_CODE uint8_t buttons_long_order[BUTTON_LONG_COUNT] = {2U, 1U, 0U};

static MEM_XDATA uint8_t buttons_candidate;
static MEM_XDATA uint8_t buttons_debounced;
static MEM_XDATA uint8_t buttons_pending;
static MEM_XDATA uint16_t buttons_change_times[BUTTON_COUNT];
/* The button bit that auto-repeats, or 0. */
static MEM_XDATA uint8_t buttons_repeat_bit;
static MEM_XDATA uint16_t buttons_repeat_time;
static MEM_XDATA uint16_t buttons_repeat_wait;
static MEM_XDATA bool buttons_is_recovery;
/* False while the keyer owns P3.2: ENTER then never gives a press edge. */
static MEM_XDATA bool buttons_is_enter_enabled;
/* MENU, PREV and NEXT bits that are pressed and have not given their long event yet. */
static MEM_XDATA uint8_t buttons_long_pending;
static MEM_XDATA uint16_t buttons_press_times[BUTTON_LONG_COUNT];

/* The sample of the current poll or recovery window step. */
static MEM_XDATA uint8_t sample_raw;
static MEM_XDATA uint16_t sample_now;
static MEM_XDATA uint16_t window_start;
static MEM_XDATA uint16_t window_previous;

/* Applies sample_raw at sample_now to the debounce state of every button. */
static void buttons_debounce(void) {
  uint8_t index;
  uint8_t bit;

  for (index = 0U; index < BUTTON_COUNT; ++index) {
    bit = buttons_bits[index];
    if (((sample_raw ^ buttons_candidate) & bit) != 0U) {
      buttons_candidate ^= bit;
      buttons_change_times[index] = sample_now;
    } else if ((((buttons_candidate ^ buttons_debounced) & bit) != 0U) &&
               ((uint16_t)(sample_now - buttons_change_times[index]) >= BUTTONS_DEBOUNCE_MS)) {
      buttons_debounced ^= bit;
      if ((buttons_debounced & bit) != 0U) {
        buttons_pending |= bit;
        if ((bit & BUTTON_REPEAT_MASK) != 0U) {
          buttons_repeat_bit = bit;
          buttons_repeat_time = sample_now;
          buttons_repeat_wait = BUTTONS_REPEAT_DELAY_MS;
        }
        if ((bit & BUTTON_LONG_MASK) != 0U) {
          buttons_long_pending |= bit;
          buttons_press_times[index] = sample_now;
        }
      } else {
        buttons_long_pending &= (uint8_t)~bit;
        if (bit == buttons_repeat_bit) {
          buttons_repeat_bit = 0U;
        }
      }
    }
  }
}

static void buttons_sample_pins(void) { sample_raw = BUTTON_PINS_READ_PRESSED(); }

void buttons_init(void) {
  BUTTON_PINS_CONFIGURE();
  delay_ms(BUTTONS_SETTLE_MS);
  buttons_sample_pins();
  window_start = tick_now();
  sample_now = window_start;
  buttons_is_recovery = false;
  /* One sample per tick while MENU is held; the first high sample ends the window. */
  while ((sample_raw & BUTTON_BIT_MENU) != 0U) {
    if ((uint16_t)(sample_now - window_start) >= BUTTONS_RECOVERY_HOLD_MS) {
      buttons_is_recovery = true;
      break;
    }
    window_previous = sample_now;
    do {
      sample_now = tick_now();
    } while (sample_now == window_previous);
    buttons_sample_pins();
  }
  buttons_candidate = sample_raw;
  buttons_debounced = sample_raw;
  buttons_change_times[0] = sample_now;
  buttons_change_times[1] = sample_now;
  buttons_change_times[2] = sample_now;
  buttons_change_times[3] = sample_now;
  buttons_pending = 0U;
  buttons_repeat_bit = 0U;
  buttons_long_pending = 0U;
  buttons_is_enter_enabled = true;
}

bool buttons_is_recovery_start(void) { return buttons_is_recovery; }

void buttons_set_enter_enabled(bool is_enabled) {
  if (!is_enabled) {
    buttons_is_enter_enabled = false;
    buttons_pending &= (uint8_t)~BUTTON_BIT_ENTER;
    return;
  }
  /* The debounce starts from the pin state now, so a contact held at the mode end gives no ENTER. */
  buttons_sample_pins();
  sample_now = tick_now();
  buttons_candidate = (uint8_t)((buttons_candidate & (uint8_t)~BUTTON_BIT_ENTER) | (sample_raw & BUTTON_BIT_ENTER));
  buttons_debounced = (uint8_t)((buttons_debounced & (uint8_t)~BUTTON_BIT_ENTER) | (sample_raw & BUTTON_BIT_ENTER));
  buttons_change_times[BUTTON_ENTER_INDEX] = sample_now;
  buttons_pending &= (uint8_t)~BUTTON_BIT_ENTER;
  buttons_is_enter_enabled = true;
}

button_event_t buttons_poll(void) {
  uint8_t order;
  uint8_t index;
  uint8_t bit;

  buttons_sample_pins();
  if (!buttons_is_enter_enabled) {
    sample_raw &= (uint8_t)~BUTTON_BIT_ENTER;
  }
  sample_now = tick_now();
  buttons_debounce();

  for (order = 0U; order < BUTTON_COUNT; ++order) {
    bit = buttons_bits[buttons_event_order[order]];
    if ((buttons_pending & bit) != 0U) {
      buttons_pending &= (uint8_t)~bit;
      return (button_event_t)(buttons_event_order[order] + 1U);
    }
  }
  for (order = 0U; order < BUTTON_LONG_COUNT; ++order) {
    index = buttons_long_order[order];
    bit = buttons_bits[index];
    if (((buttons_long_pending & bit) != 0U) &&
        ((uint16_t)(sample_now - buttons_press_times[index]) >= BUTTONS_LONG_PRESS_MS)) {
      buttons_long_pending &= (uint8_t)~bit;
      return (button_event_t)((index + 1U) | BUTTON_EVENT_LONG_FLAG);
    }
  }
  if ((buttons_repeat_bit != 0U) && ((uint16_t)(sample_now - buttons_repeat_time) >= buttons_repeat_wait)) {
    /* The repeat time moves by the wait, not to now, so a slow main loop loses no repeat step. */
    buttons_repeat_time = (uint16_t)(buttons_repeat_time + buttons_repeat_wait);
    buttons_repeat_wait = BUTTONS_REPEAT_PERIOD_MS;
    return (button_event_t)(buttons_repeat_bit | BUTTON_EVENT_REPEAT_FLAG);
  }
  return BUTTON_EVENT_NONE;
}
