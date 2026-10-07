#ifndef CW_PLAYER_SERVICE_H
#define CW_PLAYER_SERVICE_H

/*
 * ISR side of the CW player (references/internal/training_spec.md decision D-T1 and section 9.2). On the target
 * only sidetone.c includes this header, so player_service() is a function of the Timer 0 interrupt file. Every
 * includer defines PLAYER_SIDETONE_ON() and PLAYER_SIDETONE_OFF() first and calls player_service().
 */

#if !defined(PLAYER_SIDETONE_ON) || !defined(PLAYER_SIDETONE_OFF)
#error "Define PLAYER_SIDETONE_ON() and PLAYER_SIDETONE_OFF() before including cw/player_service.h"
#endif

#include <stdbool.h>
#include <stdint.h>

#include "cw/player_shared.h"
#include "mcu/compiler.h"

/* ISR-private state. Host tests read it, so the names are part of the contract. */
static MEM_BIT player_is_mark;         /* 1: a mark runs, 0: a gap runs */
static MEM_DATA uint16_t player_ticks; /* calls left in the current mark or gap */
static MEM_DATA uint8_t player_code;   /* play form of the current character */
static MEM_DATA uint8_t player_index;  /* index of the current character */

/* The interrupt calls it, so its locals must not share the overlay with main-loop functions. */
#if defined(__SDCC_mcs51)
#pragma save
#pragma nooverlay
#endif
/**
 * Runs one player step. The call that starts a mark or a gap loads its length N, so the phase lasts exactly N calls,
 * the start call included.
 */
static void player_service(void) {
  if (!player_is_enabled) {
    return;
  }
  if (player_is_start_requested) {
    player_is_start_requested = false;
    player_index = 0U;
    player_code = player_codes[0];
    /* A gap that ends in this call: the countdown below starts the first mark now. */
    player_is_mark = false;
    player_ticks = 1U;
  }
  --player_ticks;
  if (player_is_mark) {
    if (player_ticks == 0U) {
      player_code >>= 1;
      if (player_code != PLAYER_CODE_DONE) {
        player_is_mark = false;
        player_ticks = player_dit_ticks;
      } else {
        ++player_index;
        if (player_index == player_code_count) {
          player_is_enabled = false;
          return;
        }
        player_code = player_codes[player_index];
        player_is_mark = false;
        player_ticks = player_char_gap_ticks;
      }
    } else if (player_ticks <= PLAYER_EARLY_RELEASE_TICKS) {
      /* The sidetone runs after the player, so the 32-step fall then writes compare 0 on the last mark call. */
      PLAYER_SIDETONE_OFF();
    }
  } else if (player_ticks == 0U) {
    player_is_mark = true;
    player_ticks = ((player_code & 1U) != 0U) ? player_dah_ticks : player_dit_ticks;
    PLAYER_SIDETONE_ON();
  }
}
#if defined(__SDCC_mcs51)
#pragma restore
#endif

#endif /* CW_PLAYER_SERVICE_H */
