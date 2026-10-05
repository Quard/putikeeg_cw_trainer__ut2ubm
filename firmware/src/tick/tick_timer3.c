#if !defined(__SDCC_mcs51)
#error "tick_timer3.c is for the SDCC mcs51 target only"
#endif

#include "mcu/stc8h.h"
#include "tick/tick.h"
#include "tick_counter.h"

/*
 * In 12T mode Timer 3 counts MCU_CLOCK_HZ / 12 per second and interrupts on overflow from 65535.
 * Counts per tick = 24000000 / 12 / 1000 = 2000, reload = 65536 - 2000 = 0xF830: exactly 1.000 ms, so the only
 * error is the oscillator trim.
 */
#define TICK_TIMER3_COUNTS (MCU_CLOCK_HZ / 12UL / TICK_HZ)

#if (MCU_CLOCK_HZ % (12UL * TICK_HZ)) != 0
#error "MCU_CLOCK_HZ must be a multiple of 12 * TICK_HZ for an exact tick"
#endif
#if (TICK_TIMER3_COUNTS == 0) || (TICK_TIMER3_COUNTS > 65536UL)
#error "Timer 3 cannot count one tick period at this MCU_CLOCK_HZ"
#endif

#define TICK_TIMER3_RELOAD ((uint16_t)(65536UL - TICK_TIMER3_COUNTS))

void tick_init(void) {
  /* Stop Timer 3 and select timer mode, 12T, no clock output. The upper nibble belongs to Timer 4. */
  T4T3M &= (uint8_t)~T4T3M_T3_MASK;
  T3L = (uint8_t)(TICK_TIMER3_RELOAD & 0xFFU);
  T3H = (uint8_t)(TICK_TIMER3_RELOAD >> 8);
  AUXINTIF &= (uint8_t)~AUXINTIF_T3IF;
  tick_count_ms = 0U;
  IE2 |= IE2_ET3;
  T4T3M |= T4T3M_T3R;
}

void tick_isr(void) __interrupt(TIMER3_INTERRUPT) {
  /* T2IF and T4IF also read as 0, so this clears them too: Timer 2 and Timer 4 interrupts must stay off. */
  AUXINTIF &= (uint8_t)~AUXINTIF_T3IF;
  ++tick_count_ms;
}
