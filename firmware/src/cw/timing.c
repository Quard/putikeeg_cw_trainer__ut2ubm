#include "cw/timing.h"

#include "mcu/compiler.h"

/* The Farnsworth factors share this factor, so the math needs only one 32-bit step. */
#define CW_TIMING_FARNSWORTH_SCALE 2400UL
#if ((CW_TIMING_FARNSWORTH_C_FACTOR % CW_TIMING_FARNSWORTH_SCALE) != 0UL) || \
    ((CW_TIMING_FARNSWORTH_S_FACTOR % CW_TIMING_FARNSWORTH_SCALE) != 0UL)
#error "CW_TIMING_FARNSWORTH_SCALE must divide both Farnsworth factors"
#endif

MEM_XDATA uint16_t cw_timing_dit_ticks;
MEM_XDATA uint16_t cw_timing_unit_ticks;

/* Work values in xdata. Volatile, so SDCC stores each step here and keeps no temporary in internal RAM. */
static volatile MEM_XDATA uint16_t start_dit;
static volatile MEM_XDATA uint16_t start_denominator;
static volatile MEM_XDATA uint32_t start_unit;

void cw_timing_compute(uint8_t wpm, MEM_XDATA uint8_t eff_wpm) {
  if (wpm < CW_TIMING_WPM_MIN) {
    wpm = CW_TIMING_WPM_MIN;
  } else if (wpm > CW_TIMING_WPM_MAX) {
    wpm = CW_TIMING_WPM_MAX;
  }
  if (eff_wpm < CW_TIMING_WPM_MIN) {
    eff_wpm = CW_TIMING_WPM_MIN;
  } else if (eff_wpm > wpm) {
    eff_wpm = wpm;
  }

  /* dit = round(4800 / wpm) calls = 1200 / wpm ms at 4000 calls per second; 960..96 at 5..50 WPM. */
  start_dit = (uint16_t)(((uint16_t)CW_TIMING_DIT_NUMERATOR + (uint16_t)(wpm / 2U)) / wpm);
  cw_timing_dit_ticks = start_dit;

  /*
   * Farnsworth unit fu = (60 c - 37.2 s) / (19 c s) seconds, with c = wpm and s = eff_wpm. In calls:
   * round((240000 c - 148800 s) / (19 c s)) = round(2400 (100 c - 62 s) / (19 c s)). For 5 <= s <= c <= 50,
   * 100 c - 62 s is 190..4690 and 19 c s is at most 47 500, so both fit uint16_t, and the numerator, at most
   * 11 256 000, fits uint32_t. fu = dit for s = c; the clamp catches a rounding below dit. fu is at most 2370.
   */
  start_denominator = (uint16_t)((uint16_t)(CW_TIMING_FARNSWORTH_C_FACTOR / CW_TIMING_FARNSWORTH_SCALE) * wpm);
  start_denominator -= (uint16_t)((uint16_t)(CW_TIMING_FARNSWORTH_S_FACTOR / CW_TIMING_FARNSWORTH_SCALE) * eff_wpm);
  start_unit = CW_TIMING_FARNSWORTH_SCALE * start_denominator;
  start_denominator = (uint16_t)((uint16_t)wpm * eff_wpm);
  start_denominator = (uint16_t)(start_denominator * (uint16_t)CW_TIMING_FARNSWORTH_DIVISOR);
  start_unit += (uint16_t)(start_denominator / 2U);
  start_unit /= start_denominator;
  if (start_unit < start_dit) {
    start_unit = start_dit;
  }
  cw_timing_unit_ticks = (uint16_t)start_unit;
}
