/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the CW player (references/internal/training_spec.md section 9.1, AC-T14). Each tick() is one Timer 0
 * call of player_service().
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "fake_sidetone_pwm.h"
#include "sidetone/sidetone.h"
#include "test_assert.h"

#define PLAYER_SIDETONE_ON() sidetone_set_on(true)
#define PLAYER_SIDETONE_OFF() sidetone_set_on(false)
#include "cw/player.h"
#include "cw/player_service.h"

static void run(size_t calls) {
  size_t index;

  for (index = 0U; index < calls; ++index) {
    player_service();
  }
}

static bool start_text(const char* text, uint8_t length) {
  char buffer[PLAYER_TEXT_MAX + 2U];

  memset(buffer, 0, sizeof(buffer));
  memcpy(buffer, text, strlen(text) + 1U);
  return player_start(buffer, length, 20U, 20U);
}

/*
 * Section 9.1 step 1: player_start() first stops the playback, also when it then rejects the text. So a rejected
 * start during a mark never leaves the sidetone request on.
 */
static void test_rejected_start_during_mark_stops_tone(void) {
  static const struct {
    const char* text;
    uint8_t length;
  } rejects[] = {{"T", 0U}, {"TTTTTTTTT", 9U}, {"*", 1U}, {"T ", 2U}};
  size_t index;

  for (index = 0U; index < sizeof(rejects) / sizeof(rejects[0]); ++index) {
    fake_sidetone_pwm_reset();
    sidetone_init(600U, 8U);
    TEST_ASSERT(start_text("T", 1U));
    run(100U); /* inside the 720-call dah at 20 WPM */
    TEST_ASSERT(player_is_busy());
    TEST_ASSERT(player_is_mark);
    TEST_ASSERT(sidetone_is_busy());
    TEST_ASSERT(!start_text(rejects[index].text, rejects[index].length));
    TEST_ASSERT(!player_is_busy());
    TEST_ASSERT(!player_is_start_requested);
    /* The sidetone service did not run, so busy here means that the request bit is still on. */
    TEST_ASSERT(!sidetone_is_busy());
    run(3000U);
    TEST_ASSERT(!player_is_busy());
    TEST_ASSERT(!sidetone_is_busy());
  }
}

/* A restart in the gap of a character starts a new mark at once and plays the new text from its first element. */
static void test_restart_in_gap(void) {
  fake_sidetone_pwm_reset();
  sidetone_init(600U, 8U);
  TEST_ASSERT(start_text("E", 1U));
  run(240U + 10U); /* past the 240-call dit at 20 WPM: the playback has ended */
  TEST_ASSERT(!player_is_busy());
  TEST_ASSERT(start_text("TE", 2U));
  run(1U);
  TEST_ASSERT(player_is_mark);
  TEST_ASSERT_EQUAL(720U, player_ticks);
  TEST_ASSERT(sidetone_is_busy());
}

int main(void) {
  TEST_RUN(test_rejected_start_during_mark_stops_tone);
  TEST_RUN(test_restart_in_gap);
  return TEST_FINISH();
}
