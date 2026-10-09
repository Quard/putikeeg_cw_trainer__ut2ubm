/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FAKE_SIDETONE_PWM_H
#define FAKE_SIDETONE_PWM_H

/*
 * Host fake of the sidetone PWMA, pins and Timer 0 (references/internal/sidetone_spec.md section 12.1). It models
 * the preload and shadow registers of ARR, CCR1 and CCR2, the update flag, ENO and the plug pin, and it keeps an
 * ordered log of every macro call. These count as violations: an ENO value other than 0x00, 0x02 or 0x04, an
 * output enabled while its shadow compare is not 0, and a log overflow.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FAKE_SIDETONE_PWM_LOG_SIZE 4096U

/* ENO values; they must match PWMA_ENO_ENO1N and PWMA_ENO_ENO2P in firmware/inc/mcu/stc8h.h. */
#define FAKE_SIDETONE_PWM_ENO_OFF 0x00U
#define FAKE_SIDETONE_PWM_ENO_SPEAKER 0x02U
#define FAKE_SIDETONE_PWM_ENO_HEADPHONES 0x04U

#define FAKE_SIDETONE_PWM_EVENT_CONFIGURE 1U /* value: ARR */
#define FAKE_SIDETONE_PWM_EVENT_PINS_CONFIGURE 2U
#define FAKE_SIDETONE_PWM_EVENT_TIMER_START 3U
#define FAKE_SIDETONE_PWM_EVENT_SET_ARR 4U            /* value: ARR */
#define FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER 5U    /* value: CCR1 */
#define FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES 6U /* value: CCR2 */
#define FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE 7U
#define FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG 8U
#define FAKE_SIDETONE_PWM_EVENT_READ_UPDATE_FLAG 9U /* value: 1 when the flag was set */
#define FAKE_SIDETONE_PWM_EVENT_SET_ENO 10U         /* value: ENO */
#define FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT 11U
#define FAKE_SIDETONE_PWM_EVENT_UPDATE_EVENT 12U /* test stimulus fake_sidetone_pwm_update_event(), not a macro */

typedef struct {
  uint8_t kind; /* FAKE_SIDETONE_PWM_EVENT_* */
  uint16_t value;
} fake_sidetone_pwm_event_t;

/** Clears all registers, counters, the log and the hook. The plug reads "not inserted". */
void fake_sidetone_pwm_reset(void);
/** Sets the level of the plug pin that SIDETONE_PINS_IS_HEADPHONES() reads. */
void fake_sidetone_pwm_set_headphones(bool is_inserted);
/** Models a counter overflow: preloads move to the shadow registers and the update flag sets. */
void fake_sidetone_pwm_update_event(void);
/** Sets a hook that each SIDETONE_PREEMPTION_POINT() calls, or NULL for none. */
void fake_sidetone_pwm_set_preemption_hook(void (*hook)(void));

size_t fake_sidetone_pwm_preemption_count(void);
size_t fake_sidetone_pwm_configure_count(void);
size_t fake_sidetone_pwm_pins_configure_count(void);
size_t fake_sidetone_pwm_timer_start_count(void);
/** Returns the number of register macro calls before the first SIDETONE_PWM_CONFIGURE(). */
size_t fake_sidetone_pwm_writes_before_configure(void);
uint16_t fake_sidetone_pwm_configured_arr(void);
uint16_t fake_sidetone_pwm_arr(void); /* preload */
uint16_t fake_sidetone_pwm_shadow_arr(void);
uint16_t fake_sidetone_pwm_compare_speaker(void); /* preload */
uint16_t fake_sidetone_pwm_compare_headphones(void);
uint16_t fake_sidetone_pwm_shadow_compare_speaker(void);
uint16_t fake_sidetone_pwm_shadow_compare_headphones(void);
uint8_t fake_sidetone_pwm_eno(void);
bool fake_sidetone_pwm_is_update_flag_set(void);
size_t fake_sidetone_pwm_update_count(void); /* UG calls */
size_t fake_sidetone_pwm_violation_count(void);
/** Returns true when the log lost events; this is also a violation. */
bool fake_sidetone_pwm_is_log_overflowed(void);
/** Returns the number of SIDETONE_PINS_IS_HEADPHONES() reads. */
size_t fake_sidetone_pwm_plug_read_count(void);
size_t fake_sidetone_pwm_event_count(void);
/** Returns the event at @p index, or kind 0 when @p index is past the end. */
fake_sidetone_pwm_event_t fake_sidetone_pwm_event_at(size_t index);
/** Clears the log only; registers and counters stay. */
void fake_sidetone_pwm_clear_log(void);

/* Called by the macros of firmware/Tests/fakes/board/sidetone_pwm.h. */
void fake_sidetone_pwm_configure(uint16_t arr);
void fake_sidetone_pwm_pins_configure(void);
bool fake_sidetone_pwm_is_headphones(void);
void fake_sidetone_pwm_timer_start(void);
void fake_sidetone_pwm_set_arr(uint16_t value);
void fake_sidetone_pwm_set_compare_speaker(uint16_t value);
void fake_sidetone_pwm_set_compare_headphones(uint16_t value);
void fake_sidetone_pwm_generate_update(void);
void fake_sidetone_pwm_clear_update_flag(void);
bool fake_sidetone_pwm_read_update_flag(void);
void fake_sidetone_pwm_set_eno(uint8_t value);
void fake_sidetone_pwm_preemption_point(void);

#endif /* FAKE_SIDETONE_PWM_H */
