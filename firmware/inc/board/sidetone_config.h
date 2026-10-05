#ifndef BOARD_SIDETONE_CONFIG_H
#define BOARD_SIDETONE_CONFIG_H

/*
 * Sidetone timing and tone range. Source: references/internal/sidetone_spec.md sections 5.3, 6.1 and 8.
 * Host tests use the default target values (MCU_CLOCK_HZ = 24000000UL).
 */

#include <stdint.h>

#define SIDETONE_SERVICE_HZ 4000U /* envelope service rate */
#define SIDETONE_PRESCALER 1U     /* PWMA_PSCR; counter = MCU_CLOCK_HZ / 2 = 12 MHz */
/* f_CNT = f_SYS / (PSCR + 1), datasheet section 20.7.19. Free of casts, so #if can use it. */
#define SIDETONE_COUNTER_HZ (MCU_CLOCK_HZ / (SIDETONE_PRESCALER + 1UL))
#define SIDETONE_TONE_MIN_HZ 400U  /* equal to settings_min(SETTINGS_ID_TONE_HZ), host test AC-11 */
#define SIDETONE_TONE_MAX_HZ 1000U /* equal to settings_max(SETTINGS_ID_TONE_HZ), host test AC-11 */
#define SIDETONE_RAMP_STEPS 32U    /* the duty table has SIDETONE_RAMP_STEPS + 1 values */
#ifndef SIDETONE_PLUG_DEBOUNCE_MS
#define SIDETONE_PLUG_DEBOUNCE_MS 50U /* -D; not checked on the device */
#endif

/* One carrier period at the lowest tone, in service calls, plus a margin of 2: ceil(4000 / 400) + 2 = 12. */
#define SIDETONE_ZERO_WAIT_CALLS (((SIDETONE_SERVICE_HZ + SIDETONE_TONE_MIN_HZ - 1U) / SIDETONE_TONE_MIN_HZ) + 2U)
#define SIDETONE_ZERO_WAIT_LIMIT ((uint8_t)SIDETONE_ZERO_WAIT_CALLS)

/*
 * Timer 0 in 12T mode counts MCU_CLOCK_HZ / 12 = 2 MHz (datasheet section 13.2.12). 2 MHz / 4 kHz = 500 counts,
 * reload = 65536 - 500 = 0xFE0C: exactly 250 us, so the only error is the oscillator trim.
 */
#define SIDETONE_TIMER0_COUNTS (MCU_CLOCK_HZ / 12UL / SIDETONE_SERVICE_HZ)
#define SIDETONE_TIMER0_RELOAD ((uint16_t)(65536UL - SIDETONE_TIMER0_COUNTS))

#if (MCU_CLOCK_HZ % (12UL * SIDETONE_SERVICE_HZ)) != 0UL
#error "MCU_CLOCK_HZ must be a multiple of 12 * SIDETONE_SERVICE_HZ for an exact service rate"
#endif
#if (SIDETONE_TIMER0_COUNTS < 1UL) || (SIDETONE_TIMER0_COUNTS > 65536UL)
#error "Timer 0 cannot count one service period at this MCU_CLOCK_HZ"
#endif
#if (SIDETONE_TONE_MIN_HZ < 1U) || (SIDETONE_TONE_MIN_HZ > SIDETONE_TONE_MAX_HZ) || (SIDETONE_TONE_MAX_HZ > 65535UL)
#error "The tone range must be 1 <= SIDETONE_TONE_MIN_HZ <= SIDETONE_TONE_MAX_HZ <= 65535"
#endif
#if ((SIDETONE_COUNTER_HZ + (SIDETONE_TONE_MIN_HZ / 2U)) / SIDETONE_TONE_MIN_HZ) > 65536UL
#error "The period at SIDETONE_TONE_MIN_HZ does not fit the 16-bit ARR"
#endif
/* At least 1000 counts per period: each permille step then moves the compare value by at least one count. */
#if (SIDETONE_COUNTER_HZ / SIDETONE_TONE_MAX_HZ) < 1000UL
#error "The period at SIDETONE_TONE_MAX_HZ is too short for a strictly increasing compare table"
#endif
#if SIDETONE_ZERO_WAIT_CALLS > 255U
#error "SIDETONE_ZERO_WAIT_LIMIT must fit the 8-bit envelope index"
#endif

#endif /* BOARD_SIDETONE_CONFIG_H */
