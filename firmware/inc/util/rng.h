#ifndef UTIL_RNG_H
#define UTIL_RNG_H

/* 16-bit xorshift for the training items (references/internal/training_spec.md section 5.5). Main loop only. */

#include <stdint.h>

#define RNG_SEED_FALLBACK 0xACE1U

/** Sets the state to @p seed; a seed of 0 gives RNG_SEED_FALLBACK. */
void rng_seed(uint16_t seed);
/** XORs @p entropy into the state; a result of 0 gives RNG_SEED_FALLBACK. Does not step. */
void rng_mix(uint16_t entropy);
/** Steps the xorshift (x ^= x << 7; x ^= x >> 9; x ^= x << 8) and returns the new state. Never 0. */
uint16_t rng_next(void);
/** Returns 0..limit-1 from the high byte of rng_next(): (hi * limit) >> 8. Returns 0 and does not step for 0. */
uint8_t rng_below(uint8_t limit);

#endif /* UTIL_RNG_H */
