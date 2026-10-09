/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "util/rng.h"

#include "mcu/compiler.h"

/* The C start-up value 0 acts as RNG_SEED_FALLBACK on the first rng_next(). */
static MEM_XDATA uint16_t rng_state;

void rng_seed(uint16_t seed) { rng_state = (seed == 0U) ? RNG_SEED_FALLBACK : seed; }

void rng_mix(uint16_t entropy) {
  rng_state ^= entropy;
  if (rng_state == 0U) {
    rng_state = RNG_SEED_FALLBACK;
  }
}

uint16_t rng_next(void) {
  uint16_t state = rng_state;

  if (state == 0U) {
    state = RNG_SEED_FALLBACK;
  }
  /* The 7, 9, 8 xorshift has the full period 65535 over the non-zero states. */
  state ^= (uint16_t)(state << 7);
  state ^= (uint16_t)(state >> 9);
  state ^= (uint16_t)(state << 8);
  rng_state = state;
  return state;
}

uint8_t rng_below(uint8_t limit) {
  uint8_t high;

  if (limit == 0U) {
    return 0U;
  }
  high = (uint8_t)(rng_next() >> 8);
  /* hi * limit is at most 255 * 255, so the high byte is below limit. Two byte operands give one MUL AB. */
  return (uint8_t)((uint16_t)((uint16_t)high * limit) >> 8);
}
