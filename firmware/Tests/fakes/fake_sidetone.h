#ifndef FAKE_SIDETONE_H
#define FAKE_SIDETONE_H

/*
 * Host fake of sidetone/sidetone.h for the menu tests (references/internal/sidetone_spec.md section 12.2). It logs
 * the calls of sidetone_init(), sidetone_set_tone_hz() and sidetone_set_on() in order and counts the other calls.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FAKE_SIDETONE_LOG_SIZE 1024U

#define FAKE_SIDETONE_CALL_INIT 1U /* value: tone */
#define FAKE_SIDETONE_CALL_TONE 2U /* value: tone */
#define FAKE_SIDETONE_CALL_ON 3U   /* value: 1 for on, 0 for off */

typedef struct {
  uint8_t kind; /* FAKE_SIDETONE_CALL_* */
  uint16_t value;
} fake_sidetone_call_t;

/** Clears the log and the counters; busy reads false. */
void fake_sidetone_reset(void);
/** Sets the value that sidetone_is_busy() returns. */
void fake_sidetone_set_busy(bool is_busy);
size_t fake_sidetone_call_count(void);
/** Returns call @p index, or kind 0 past the end. */
fake_sidetone_call_t fake_sidetone_call_at(size_t index);
/** Returns the last sidetone_set_tone_hz() value, or 0 without a call. */
uint16_t fake_sidetone_last_tone(void);
/** Returns the last sidetone_set_on() value, false without a call. */
bool fake_sidetone_last_on(void);
size_t fake_sidetone_poll_count(void);
size_t fake_sidetone_service_count(void);
/** Returns true when the log lost calls. */
bool fake_sidetone_is_log_overflowed(void);

#endif /* FAKE_SIDETONE_H */
