/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SIDETONE_SIDETONE_SHARED_H
#define SIDETONE_SIDETONE_SHARED_H

/* Sidetone state that the keyer and the CW player read in the Timer 0 interrupt (sidetone_spec.md section 5.7). */

#include <stdint.h>

#include "mcu/compiler.h"

/*
 * Service calls of one full rise or fall: 4 x the ramp in ms, 16..48. sidetone_set_ramp_ms() writes it. The keyer
 * and the CW player read it in the Timer 0 interrupt. One byte, so each access is atomic.
 */
extern volatile MEM_DATA uint8_t sidetone_ramp_calls;

#endif /* SIDETONE_SIDETONE_SHARED_H */
