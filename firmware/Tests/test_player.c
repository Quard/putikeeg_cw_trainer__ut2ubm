/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Host tests of the CW player (references/internal/training_spec.md sections 5.4 and 9, AC-T12 to AC-T14). Each
 * tick() is one Timer 0 call: player_service(), then, in the envelope tests, the real sidetone_service().
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "fake_sidetone_pwm.h"
#include "sidetone/sidetone.h"
#include "test_assert.h"

static void record_sidetone_on(void);
static void record_sidetone_off(void);
#define PLAYER_SIDETONE_ON() record_sidetone_on()
#define PLAYER_SIDETONE_OFF() record_sidetone_off()
#include "cw/player.h"
#include "cw/player_service.h"

#define MAX_MARKS 64U
#define EARLY_RELEASE 32U

static size_t call_count;
static bool is_envelope_run;
/* Per mark: the call of the ON request, the first OFF request and the last mark call. */
static size_t on_calls[MAX_MARKS];
static size_t first_off_calls[MAX_MARKS];
static size_t mark_first_calls[MAX_MARKS];
static size_t mark_last_calls[MAX_MARKS];
static size_t mark_count;
static size_t on_count;
static size_t off_count;
static bool was_mark;
static size_t busy_end_call;
static size_t fall_zero_calls[MAX_MARKS];
static size_t fall_zero_count;

static void record_sidetone_on(void) {
  if (on_count < MAX_MARKS) {
    on_calls[on_count] = call_count;
  }
  ++on_count;
  sidetone_set_on(true);
}

static void record_sidetone_off(void) {
  if ((on_count != 0U) && (on_count <= MAX_MARKS) && (first_off_calls[on_count - 1U] == 0U)) {
    first_off_calls[on_count - 1U] = call_count;
  }
  ++off_count;
  sidetone_set_on(false);
}

static bool is_compare_zero_write(void) {
  size_t index;

  for (index = 0U; index < fake_sidetone_pwm_event_count(); ++index) {
    const fake_sidetone_pwm_event_t event = fake_sidetone_pwm_event_at(index);

    if (((event.kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) ||
         (event.kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES)) &&
        (event.value == 0U)) {
      return true;
    }
  }
  return false;
}

static void reset_record(void) {
  call_count = 0U;
  mark_count = 0U;
  on_count = 0U;
  off_count = 0U;
  was_mark = false;
  busy_end_call = 0U;
  fall_zero_count = 0U;
  memset(on_calls, 0, sizeof(on_calls));
  memset(first_off_calls, 0, sizeof(first_off_calls));
  memset(mark_first_calls, 0, sizeof(mark_first_calls));
  memset(mark_last_calls, 0, sizeof(mark_last_calls));
  memset(fall_zero_calls, 0, sizeof(fall_zero_calls));
}

static void tick(void) {
  const bool was_busy = player_is_busy();

  ++call_count;
  if (is_envelope_run) {
    fake_sidetone_pwm_clear_log();
  }
  player_service();
  if (is_envelope_run) {
    sidetone_service();
    if (is_compare_zero_write() && (sidetone_state() == SIDETONE_STATE_ZERO_WAIT)) {
      if (fall_zero_count < MAX_MARKS) {
        fall_zero_calls[fall_zero_count] = call_count;
      }
      ++fall_zero_count;
    }
  }
  if (player_is_enabled && player_is_mark && !was_mark && (mark_count < MAX_MARKS)) {
    mark_first_calls[mark_count] = call_count;
    ++mark_count;
  }
  if (was_mark && !(player_is_enabled && player_is_mark) && (mark_count != 0U)) {
    mark_last_calls[mark_count - 1U] = call_count - 1U;
  }
  was_mark = player_is_enabled && player_is_mark;
  if (was_busy && !player_is_busy()) {
    busy_end_call = call_count;
  }
}

static void run(size_t calls) {
  size_t index;

  for (index = 0U; index < calls; ++index) {
    tick();
  }
}

/* Runs until the playback ends, with a limit far above the longest text. */
static void run_to_end(void) {
  size_t guard = 0U;

  while (player_is_busy() && (guard < 2000000U)) {
    tick();
    ++guard;
  }
}

typedef struct {
  char character;
  const char* code;
} code_t;

/* The International Morse code, written as text, so the test does not share the table of morse.c. */
static const code_t codes[] = {
    {'A', ".-"},    {'E', "."},         {'I', ".."},       {'K', "-.-"},      {'M', "--"},     {'O', "---"},
    {'P', ".--."},  {'R', ".-."},       {'S', "..."},      {'T', "-"},        {'Y', "-.--"},   {'5', "....."},
    {'0', "-----"}, {'\x01', "...-.-"}, {'\x02', ".-.-."}, {'\x03', "-...-"}, {'?', "..--.."}, {'/', "-..-."},
};

static const char* code_of(char character) {
  size_t index;

  for (index = 0U; index < sizeof(codes) / sizeof(codes[0]); ++index) {
    if (codes[index].character == character) {
      return codes[index].code;
    }
  }
  return NULL;
}

static uint16_t dit_of(uint8_t wpm) { return (uint16_t)((4800U + (wpm / 2U)) / wpm); }

/* Section 9.2 reference values of the character gap: 3 Farnsworth units. */
static uint16_t char_gap_of(uint8_t wpm, uint8_t eff_wpm) {
  static const struct {
    uint8_t wpm;
    uint8_t eff_wpm;
    uint16_t char_gap;
  } rows[] = {{5U, 5U, 2880U},   {20U, 20U, 720U}, {20U, 10U, 2616U}, {20U, 5U, 6405U},
              {25U, 15U, 1587U}, {50U, 50U, 288U}, {50U, 5U, 7110U}};
  size_t index;

  for (index = 0U; index < sizeof(rows) / sizeof(rows[0]); ++index) {
    if ((rows[index].wpm == wpm) && (rows[index].eff_wpm == eff_wpm)) {
      return rows[index].char_gap;
    }
  }
  return 0U;
}

static bool start_text(const char* text, uint8_t wpm, uint8_t eff_wpm) {
  char buffer[PLAYER_TEXT_MAX + 2U];
  const size_t length = strlen(text);

  memcpy(buffer, text, length + 1U);
  return player_start(buffer, (uint8_t)length, wpm, eff_wpm);
}

/* AC-T12: every mark and gap has the exact length of section 9.2. */
static void timing_case(const char* text, uint8_t wpm, uint8_t eff_wpm) {
  const uint16_t dit = dit_of(wpm);
  const uint16_t char_gap = char_gap_of(wpm, eff_wpm);
  size_t expected_start = 1U;
  size_t mark = 0U;
  size_t position;
  size_t element;

  reset_record();
  TEST_ASSERT(char_gap != 0U);
  TEST_ASSERT(start_text(text, wpm, eff_wpm));
  TEST_ASSERT(player_is_busy());
  TEST_ASSERT_EQUAL(dit, player_dit_ticks);
  TEST_ASSERT_EQUAL(3U * dit, player_dah_ticks);
  TEST_ASSERT_EQUAL(char_gap, player_char_gap_ticks);
  run_to_end();

  for (position = 0U; text[position] != '\0'; ++position) {
    const char* code = code_of(text[position]);

    if (!TEST_ASSERT(code != NULL)) {
      return;
    }
    for (element = 0U; code[element] != '\0'; ++element) {
      const size_t length = (code[element] == '-') ? (3U * dit) : dit;

      if (!TEST_ASSERT(mark < mark_count)) {
        return;
      }
      TEST_ASSERT_EQUAL(expected_start, mark_first_calls[mark]);
      TEST_ASSERT_EQUAL(expected_start + length - 1U, mark_last_calls[mark]);
      TEST_ASSERT_EQUAL(expected_start, on_calls[mark]);
      /* The request goes off on the call where 32 mark calls remain. */
      TEST_ASSERT_EQUAL(expected_start + length - EARLY_RELEASE, first_off_calls[mark]);
      expected_start += length;
      expected_start += (code[element + 1U] != '\0') ? dit : char_gap;
      ++mark;
    }
  }
  TEST_ASSERT_EQUAL(mark, mark_count);
  TEST_ASSERT_EQUAL(mark, on_count);
  /* Busy clears on the call after the last mark call. */
  TEST_ASSERT_EQUAL(mark_last_calls[mark - 1U] + 1U, busy_end_call);
  TEST_ASSERT(!player_is_busy());
  /* Nothing more happens. */
  run(1000U);
  TEST_ASSERT_EQUAL(mark, on_count);
  TEST_ASSERT_EQUAL(mark, mark_count);
}

static void test_exact_timing(void) {
  static const char* const texts[] = {"E", "T", "K", "5", "0", "PARIS", "\x01", "\x02", "MORSEKEY", "?/"};
  static const uint8_t speeds[][2] = {{5U, 5U}, {20U, 20U}, {50U, 50U}, {20U, 10U}, {50U, 5U}};
  size_t text;
  size_t speed;

  for (speed = 0U; speed < sizeof(speeds) / sizeof(speeds[0]); ++speed) {
    for (text = 0U; text < sizeof(texts) / sizeof(texts[0]); ++text) {
      timing_case(texts[text], speeds[speed][0], speeds[speed][1]);
    }
  }
}

static void test_reference_ticks(void) {
  TEST_ASSERT(start_text("E", 25U, 15U));
  TEST_ASSERT_EQUAL(192U, player_dit_ticks);
  TEST_ASSERT_EQUAL(576U, player_dah_ticks);
  TEST_ASSERT_EQUAL(1587U, player_char_gap_ticks);
  TEST_ASSERT(start_text("E", 20U, 5U));
  TEST_ASSERT_EQUAL(6405U, player_char_gap_ticks);
  /* Clamps of cw_timing_compute(). */
  TEST_ASSERT(start_text("E", 0U, 0U));
  TEST_ASSERT_EQUAL(960U, player_dit_ticks);
  TEST_ASSERT_EQUAL(2880U, player_char_gap_ticks);
  TEST_ASSERT(start_text("E", 255U, 255U));
  TEST_ASSERT_EQUAL(96U, player_dit_ticks);
  TEST_ASSERT_EQUAL(288U, player_char_gap_ticks);
  player_stop();
}

/* Section 5.4: A (.-) has pattern 0b101 and play form 0b110. */
static void test_play_form(void) {
  TEST_ASSERT(start_text("AE0\x01", 20U, 20U));
  TEST_ASSERT_EQUAL(0x06U, player_codes[0]);
  TEST_ASSERT_EQUAL(0x02U, player_codes[1]);
  TEST_ASSERT_EQUAL(0x3FU, player_codes[2]);
  /* SK ...-.- : first element in bit 0, so 0b1101000 with the end marker at bit 6. */
  TEST_ASSERT_EQUAL(0x68U, player_codes[3]);
  TEST_ASSERT_EQUAL(4U, player_code_count);
  player_stop();
}

/* AC-T13: with the real sidetone, compare 0 is written on the last mark call of every element. */
static void envelope_case(uint8_t wpm, uint8_t eff_wpm, bool is_headphones) {
  size_t index;

  fake_sidetone_pwm_reset();
  fake_sidetone_pwm_set_headphones(is_headphones);
  sidetone_init(600U);
  is_envelope_run = true;
  reset_record();
  TEST_ASSERT(start_text("PARIS", wpm, eff_wpm));
  run_to_end();
  run(200U);
  is_envelope_run = false;

  TEST_ASSERT_EQUAL(14U, mark_count);
  TEST_ASSERT_EQUAL(mark_count, fall_zero_count);
  for (index = 0U; (index < mark_count) && (index < fall_zero_count); ++index) {
    TEST_ASSERT_EQUAL(mark_last_calls[index], fall_zero_calls[index]);
  }
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT(!sidetone_is_busy());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_violation_count());
}

static void test_envelope(void) {
  envelope_case(5U, 5U, false);
  envelope_case(5U, 5U, true);
  envelope_case(20U, 20U, false);
  envelope_case(20U, 20U, true);
  envelope_case(50U, 50U, false);
  envelope_case(50U, 50U, true);
  envelope_case(20U, 10U, false);
  envelope_case(50U, 5U, true);
}

/* AC-T14: a rejected start plays nothing. */
static void check_rejected(const char* text, uint8_t length) {
  char buffer[16];

  memset(buffer, 'E', sizeof(buffer));
  memcpy(buffer, text, strlen(text));
  reset_record();
  TEST_ASSERT(!player_start(buffer, length, 20U, 20U));
  TEST_ASSERT(!player_is_busy());
  run(3000U);
  TEST_ASSERT_EQUAL(0U, on_count);
  TEST_ASSERT_EQUAL(0U, mark_count);
}

static void test_rejects(void) {
  check_rejected("E", 0U);
  check_rejected("EEEEEEEEE", 9U);
  check_rejected("EEEEEEEEE", 255U);
  check_rejected("*", 1U);
  check_rejected("E*", 2U);
  check_rejected("a", 1U);
  check_rejected(" ", 1U);
  check_rejected("EE+", 3U);
  {
    char nul_text[2] = {'\0', '\0'};

    reset_record();
    TEST_ASSERT(!player_start(nul_text, 1U, 20U, 20U));
    TEST_ASSERT(!player_is_busy());
  }
  /* A rejected start also stops a running playback. */
  reset_record();
  TEST_ASSERT(start_text("T", 20U, 20U));
  run(100U);
  TEST_ASSERT(player_is_busy());
  TEST_ASSERT(!start_text("*", 20U, 20U));
  TEST_ASSERT(!player_is_busy());
  run(2000U);
  TEST_ASSERT_EQUAL(1U, on_count);
  /* 8 characters is the maximum. */
  TEST_ASSERT(start_text("EEEEEEEE", 50U, 50U));
  player_stop();
}

/* AC-T14: a start during a playback restarts from the first element. */
static void test_restart(void) {
  reset_record();
  TEST_ASSERT(start_text("T", 20U, 20U));
  run(100U);
  TEST_ASSERT(player_is_mark);
  TEST_ASSERT(start_text("EE", 20U, 20U));
  tick();
  TEST_ASSERT(player_is_mark);
  TEST_ASSERT_EQUAL(0U, player_index);
  TEST_ASSERT_EQUAL(240U, player_ticks);
  TEST_ASSERT_EQUAL(2U, on_count);
  TEST_ASSERT_EQUAL(101U, on_calls[1]);
  run_to_end();
  /* E, gap 720, E: the end comes 240 + 720 + 240 calls after the restart call. */
  TEST_ASSERT_EQUAL(101U + 240U + 720U + 240U, busy_end_call);
  TEST_ASSERT_EQUAL(3U, on_count);
}

/* AC-T14: stop clears busy; the disabled service changes no player state and reads no shared value. */
static void test_stop(void) {
  uint16_t ticks;
  uint8_t code;
  uint8_t index;
  bool is_mark;

  fake_sidetone_pwm_reset();
  sidetone_init(600U);
  is_envelope_run = true;
  reset_record();
  TEST_ASSERT(start_text("0", 20U, 20U));
  run(300U);
  TEST_ASSERT(player_is_busy());
  TEST_ASSERT(sidetone_is_busy());
  player_stop();
  TEST_ASSERT(!player_is_busy());
  ticks = player_ticks;
  code = player_code;
  index = player_index;
  is_mark = player_is_mark;
  /* Values the disabled service must ignore. */
  player_is_start_requested = true;
  player_codes[0] = 0x02U;
  player_code_count = 0U;
  player_dit_ticks = 1U;
  run(2000U);
  is_envelope_run = false;
  TEST_ASSERT_EQUAL(ticks, player_ticks);
  TEST_ASSERT_EQUAL(code, player_code);
  TEST_ASSERT_EQUAL(index, player_index);
  TEST_ASSERT_EQUAL(is_mark, player_is_mark);
  TEST_ASSERT_EQUAL(1U, on_count);
  TEST_ASSERT(!player_is_busy());
  /* A stop during a mark gives the normal fall, with no violation. */
  TEST_ASSERT(!sidetone_is_busy());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_violation_count());
  /* A second stop has no other effect. */
  player_stop();
  TEST_ASSERT(!player_is_busy());
  TEST_ASSERT(!player_is_start_requested);
  run(10U);
  TEST_ASSERT_EQUAL(ticks, player_ticks);
  TEST_ASSERT_EQUAL(1U, on_count);
}

int main(void) {
  TEST_RUN(test_exact_timing);
  TEST_RUN(test_reference_ticks);
  TEST_RUN(test_play_form);
  TEST_RUN(test_envelope);
  TEST_RUN(test_rejects);
  TEST_RUN(test_restart);
  TEST_RUN(test_stop);
  return TEST_FINISH();
}
