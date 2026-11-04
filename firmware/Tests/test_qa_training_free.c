/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the callsign generator at the session seam (references/internal/training_spec.md sections 23.2, 23.3
 * and 23.8, AC-T55, Q-C4, mutants M1 to M4, M12 and M14). The oracle is a reference model typed from the document:
 * the prefix table of 23.2 in table order, the fair pick of D-T84, the step order of 23.3 and the xorshift of
 * util/rng.h. The model predicts every item of real free sessions from the seed. The engine part covers AC-T56 and
 * AC-T60 at the session seam: mutants M6 to M9 and the faults F3, F6, F7 and F8.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "callsign/callsign.h"
#include "eeprom/eeprom_map.h"
#include "fake_iap.h"
#include "fake_tick.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "progress/progress.h"
#include "support/progress_fixture.h"
#include "test_assert.h"
#include "util/rng.h"

#define RX TRAINING_MODE_RECEIVING
#define TX TRAINING_MODE_SENDING
#define FULL KOCH_WORD_ALPHABET_MAX
#define FIRST_POSSIBLE_COUNT 18U
#define STEP_MS 3000U
#define REF_PREFIX_COUNT 100U
#define REF_OWN_DIGIT_COUNT 10U
#define SEED_COUNT 5U
#define CALLSIGNS_PER_SEED 10000U
#define CALLSIGNS_PER_COUNT 2000U
#define LETTERS 26U
#define DIGITS 10U
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define WRONG_CHAR '?' /* in no callsign */

/* ---------- Reference model, typed from training_spec.md 23.2 and 23.3 ---------- */

static const char* const ref_prefixes[REF_PREFIX_COUNT] = {
    "UR",  "US",  "UT",  "UU",  "UV",  "UW", "UX",  "UY",  "UZ", "EM", "EN", "EO",  "K",  "W",  "N",  "AA", "KB",
    "KH6", "KL7", "KP4", "KH2", "VE",  "VA", "XE",  "CO",  "P4", "HK", "PY", "PU",  "LU", "CE", "CX", "DL", "DK",
    "DJ",  "DO",  "SP",  "SQ",  "OK",  "OM", "HA",  "YO",  "LZ", "YU", "9A", "S5",  "E7", "Z3", "SV", "5B", "TA",
    "ER",  "OE",  "HB",  "HB0", "I",   "IK", "IZ",  "IS0", "9H", "F",  "ON", "PA",  "LX", "G",  "M",  "GM", "GW",
    "EI",  "EA",  "EC",  "EA6", "EA8", "CT", "CT3", "OZ",  "SM", "LA", "OH", "OH0", "TF", "ES", "YL", "LY", "4X",
    "A6",  "UN",  "VU",  "BY",  "HL",  "JA", "JH",  "JR",  "DU", "YB", "VK", "ZL",  "ZS", "CN", "D4",
};

/* The first 18 Koch characters of section 5.6, as 23.2 lists them for the counts 17 and 18. */
static const char ref_koch_first18[] = "KMRSUAPTLOWI.NJEF0";

/* The 36 prefixes that 23.2 lists for count 18, in table order. */
static const char ref_allowed_at_18[] =
    "UR US UT UU UW EM EN EO K W N AA PU LU SP OK OM TA ER OE I IK IS0 F ON PA M EI EA SM LA TF ES UN JA JR";

/* util/rng.h: xorshift x ^= x << 7; x ^= x >> 9; x ^= x << 8; seed 0 gives RNG_SEED_FALLBACK (0xACE1). */
static uint16_t ref_state;

static void ref_seed(uint16_t seed) { ref_state = (seed == 0U) ? 0xACE1U : seed; }

static uint16_t ref_next(void) {
  ref_state ^= (uint16_t)(ref_state << 7);
  ref_state ^= (uint16_t)(ref_state >> 9);
  ref_state ^= (uint16_t)(ref_state << 8);
  return ref_state;
}

/* D-T84: 0..n-1 from the high byte below n * (255 / n); n = 0 gives 0 with no RNG step. */
static unsigned ref_fair(unsigned n) {
  unsigned limit;
  unsigned value;
  if (n == 0U) {
    return 0U;
  }
  limit = n * (255U / n);
  do {
    value = (unsigned)(ref_next() >> 8);
  } while (value >= limit);
  return value % n;
}

static unsigned ref_cap(unsigned count) { return (count > FULL) ? FULL : count; }

static bool ref_in_set(char character, unsigned count) { return koch_index_of(character) < ref_cap(count); }

static bool ref_prefix_allowed(const char* prefix, unsigned count) {
  for (; *prefix != '\0'; ++prefix) {
    if (!ref_in_set(*prefix, count)) {
      return false;
    }
  }
  return true;
}

static unsigned ref_allowed_prefixes(unsigned count, unsigned* indexes) {
  unsigned total = 0U;
  unsigned index;
  for (index = 0U; index < REF_PREFIX_COUNT; ++index) {
    if (ref_prefix_allowed(ref_prefixes[index], count)) {
      indexes[total] = index;
      ++total;
    }
  }
  return total;
}

/* The characters from @p first to @p last of the set, in Koch order. */
static unsigned ref_set_chars(unsigned count, char first, char last, char* out) {
  unsigned total = 0U;
  unsigned index;
  for (index = 0U; (index < ref_cap(count)) && (index < KOCH_CHAR_COUNT); ++index) {
    const char character = koch_char((uint8_t)index);
    if ((character >= first) && (character <= last)) {
      out[total] = character;
      ++total;
    }
  }
  return total;
}

/* Section 23.3 steps 1 to 6. Returns the length; @p out gets the callsign and a NUL. */
static unsigned ref_make(unsigned count, char* out, unsigned* prefix_index) {
  unsigned indexes[REF_PREFIX_COUNT];
  char digits[DIGITS];
  char letters[LETTERS];
  const unsigned prefix_total = ref_allowed_prefixes(count, indexes);
  const unsigned digit_total = ref_set_chars(count, '0', '9', digits);
  const unsigned letter_total = ref_set_chars(count, 'A', 'Z', letters);
  const unsigned chosen = indexes[ref_fair(prefix_total)];
  const char* prefix = ref_prefixes[chosen];
  unsigned length = (unsigned)strlen(prefix);

  *prefix_index = chosen;
  memcpy(out, prefix, length);
  if (length < 3U) {
    out[length] = digits[ref_fair(digit_total)];
    ++length;
  }
  out[length] = letters[ref_fair(letter_total)];
  ++length;
  if (ref_fair(5U) != 0U) {
    out[length] = letters[ref_fair(letter_total)];
    ++length;
    if (ref_fair(2U) != 0U) {
      out[length] = letters[ref_fair(letter_total)];
      ++length;
    }
  }
  out[length] = '\0';
  return length;
}

/* ---------- Session driver ---------- */

static uint16_t now;

static training_event_t poll_at(uint16_t time) {
  now = time;
  fake_tick_set_now(now);
  return training_poll();
}

static void load_empty_progress(void) {
  fake_iap_reset();
  (void)progress_load();
  fake_iap_clear_counters();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(0U);
  now = 1000U;
  fake_tick_set_now(now);
}

/* Polls to the next ITEM event. Returns false at END. */
static bool next_item(void) {
  unsigned guard;
  for (guard = 0U; guard < 50U; ++guard) {
    const training_event_t event = poll_at((uint16_t)(now + STEP_MS));
    if (event == TRAINING_EVENT_ITEM) {
      return true;
    }
    if (event == TRAINING_EVENT_END) {
      return false;
    }
  }
  return false;
}

static void answer_correct(void) {
  const char MEM_XDATA* item = training_item_text();
  uint8_t index;
  for (index = 0U; index < training_item_length(); ++index) {
    (void)training_add_char(item[index]);
  }
}

/* Starts a free session of @p count characters; the mode and Max Word Length change from session to session. */
static void start_free_session(unsigned session, uint8_t count) {
  training_prepare((uint8_t)(session & 1U), 20U, (uint8_t)(1U + (session % 8U)), (session % 3U) == 0U);
  training_set_free(count);
  training_start();
}

typedef struct {
  unsigned prefixes[REF_PREFIX_COUNT];
  unsigned own_digit_with_letter[REF_PREFIX_COUNT];
  unsigned suffix_lengths[4];
  unsigned digits[DIGITS];
  unsigned letters[LETTERS];
  unsigned with_digit;
  unsigned suffix_letters;
  unsigned total;
  unsigned mismatches;
  unsigned shape_errors;
} stats_t;

/* AC-T55 (1): an independent shape check from the table, with no use of the model's pick. */
static bool shape_is_valid(const char* text, unsigned length, unsigned* prefix_out, unsigned* suffix_out) {
  unsigned index;
  for (index = 0U; index < REF_PREFIX_COUNT; ++index) {
    const char* prefix = ref_prefixes[index];
    const unsigned prefix_length = (unsigned)strlen(prefix);
    unsigned position = prefix_length;
    unsigned suffix;
    if (strncmp(text, prefix, prefix_length) != 0) {
      continue;
    }
    if (prefix_length < 3U) {
      if ((text[position] < '0') || (text[position] > '9')) {
        continue;
      }
      ++position;
    }
    for (suffix = 0U;
         (position + suffix < length) && (text[position + suffix] >= 'A') && (text[position + suffix] <= 'Z');
         ++suffix) {
    }
    if ((position + suffix == length) && (suffix >= 1U) && (suffix <= 3U) && (text[length] == '\0') &&
        (length >= CALLSIGN_LENGTH_MIN) && (length <= CALLSIGN_LENGTH_MAX)) {
      *prefix_out = index;
      *suffix_out = suffix;
      return true;
    }
  }
  return false;
}

/*
 * Runs free sessions of @p count characters from @p seed until @p callsigns items came, and compares each item with
 * the model. Fills @p stats.
 */
static void run_and_compare(uint16_t seed, uint8_t count, unsigned callsigns, stats_t* stats) {
  unsigned made = 0U;
  unsigned session = 0U;
  rng_seed(seed);
  ref_seed(seed);
  while (made < callsigns) {
    unsigned item;
    start_free_session(session, count);
    ++session;
    if (!TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_count())) {
      return;
    }
    for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
      char expected[16];
      unsigned prefix_index;
      unsigned shape_prefix;
      unsigned suffix;
      unsigned position;
      const unsigned expected_length = ref_make(count, expected, &prefix_index);
      const char* text;
      if (!TEST_ASSERT(next_item())) {
        return;
      }
      text = (const char*)training_item_text();
      if ((training_item_length() != expected_length) || (strcmp(text, expected) != 0)) {
        if (stats->mismatches < 5U) {
          fprintf(stderr, "seed 0x%04X count %u item %u: got '%s' (%u), model '%s'\n", (unsigned)seed, (unsigned)count,
                  made, text, (unsigned)training_item_length(), expected);
        }
        ++stats->mismatches;
        ref_seed(0U);
        return;
      }
      if (!shape_is_valid(text, training_item_length(), &shape_prefix, &suffix)) {
        ++stats->shape_errors;
      }
      for (position = 0U; position < expected_length; ++position) {
        if (!ref_in_set(text[position], count)) {
          ++stats->shape_errors;
        }
      }
      ++stats->prefixes[prefix_index];
      ++stats->suffix_lengths[suffix];
      {
        const unsigned prefix_length = (unsigned)strlen(ref_prefixes[prefix_index]);
        if (prefix_length == 3U) {
          if ((text[3] >= 'A') && (text[3] <= 'Z')) {
            ++stats->own_digit_with_letter[prefix_index];
          }
        } else {
          ++stats->digits[text[prefix_length] - '0'];
          ++stats->with_digit;
        }
        for (position = expected_length - suffix; position < expected_length; ++position) {
          ++stats->letters[text[position] - 'A'];
          ++stats->suffix_letters;
        }
      }
      ++stats->total;
      answer_correct();
      ++made;
    }
    TEST_ASSERT(!next_item());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  }
}

static double percent(unsigned part, unsigned whole) { return (100.0 * (double)part) / (double)whole; }

/* ---------- Tests ---------- */

/* The reference table holds the facts of 23.2, and the code's Koch order matches the order that 23.2 uses. */
static void test_reference_table_facts(void) {
  unsigned indexes[REF_PREFIX_COUNT];
  char listed[256];
  unsigned own = 0U;
  unsigned ukrainian = 0U;
  unsigned index;
  unsigned total;

  for (index = 0U; index < REF_PREFIX_COUNT; ++index) {
    const size_t length = strlen(ref_prefixes[index]);
    TEST_ASSERT((length >= 1U) && (length <= CALLSIGN_PREFIX_SIZE));
    if (length == 3U) {
      ++own;
      TEST_ASSERT((ref_prefixes[index][2] >= '0') && (ref_prefixes[index][2] <= '9'));
    }
    if (((ref_prefixes[index][0] == 'U') && (ref_prefixes[index][1] >= 'R') && (ref_prefixes[index][1] <= 'Z')) ||
        ((ref_prefixes[index][0] == 'E') && (ref_prefixes[index][1] >= 'M') && (ref_prefixes[index][1] <= 'O'))) {
      ++ukrainian;
    }
  }
  TEST_ASSERT_EQUAL(REF_OWN_DIGIT_COUNT, own);
  TEST_ASSERT_EQUAL(12U, ukrainian);
  for (index = 0U; index < 18U; ++index) {
    TEST_ASSERT_EQUAL((unsigned char)ref_koch_first18[index], (unsigned char)koch_char((uint8_t)index));
  }
  TEST_ASSERT_EQUAL(35U, ref_allowed_prefixes(17U, indexes));
  total = ref_allowed_prefixes(18U, indexes);
  TEST_ASSERT_EQUAL(36U, total);
  listed[0] = '\0';
  for (index = 0U; index < total; ++index) {
    strcat(listed, ref_prefixes[indexes[index]]);
    if (index + 1U < total) {
      strcat(listed, " ");
    }
  }
  if (!TEST_ASSERT_EQUAL(0, strcmp(ref_allowed_at_18, listed))) {
    fprintf(stderr, "allowed at 18: %s\n", listed);
  }
  TEST_ASSERT_EQUAL(REF_PREFIX_COUNT, ref_allowed_prefixes(FULL, indexes));
}

/*
 * AC-T55 (1), (2) and (4): 5 seeds x 10 000 callsigns with Full through free sessions. Every item equals the model,
 * so the code table equals the 23.2 table and the RNG order is that of 23.3 (M1, M2, M3). The spread bounds of (2),
 * plus a class-ratio bound that catches the 3:2 bias of rng_below() (M1), which the 0.75-1.25 % bound can miss.
 */
static void test_full_model_and_statistics(void) {
  static const uint16_t seeds[SEED_COUNT] = {0x0001U, 0x1234U, 0xACE1U, 0xBEEFU, 0xFFFFU};
  static stats_t stats;
  unsigned seed;
  unsigned index;
  unsigned reached = 0U;

  memset(&stats, 0, sizeof(stats));
  load_empty_progress();
  for (seed = 0U; seed < SEED_COUNT; ++seed) {
    run_and_compare(seeds[seed], FULL, CALLSIGNS_PER_SEED, &stats);
  }
  TEST_ASSERT_EQUAL(0U, stats.mismatches);
  TEST_ASSERT_EQUAL(0U, stats.shape_errors);
  if (!TEST_ASSERT_EQUAL(SEED_COUNT * CALLSIGNS_PER_SEED, stats.total)) {
    return;
  }
  printf("suffix 1/2/3: %.2f %.2f %.2f %%\n", percent(stats.suffix_lengths[1], stats.total),
         percent(stats.suffix_lengths[2], stats.total), percent(stats.suffix_lengths[3], stats.total));
  TEST_ASSERT((percent(stats.suffix_lengths[1], stats.total) >= 18.0) &&
              (percent(stats.suffix_lengths[1], stats.total) <= 22.0));
  TEST_ASSERT((percent(stats.suffix_lengths[2], stats.total) >= 38.0) &&
              (percent(stats.suffix_lengths[2], stats.total) <= 42.0));
  TEST_ASSERT((percent(stats.suffix_lengths[3], stats.total) >= 38.0) &&
              (percent(stats.suffix_lengths[3], stats.total) <= 42.0));
  for (index = 0U; index < REF_PREFIX_COUNT; ++index) {
    const double share = percent(stats.prefixes[index], stats.total);
    if (stats.prefixes[index] != 0U) {
      ++reached;
    }
    if (!TEST_ASSERT((share >= 0.75) && (share <= 1.25))) {
      fprintf(stderr, "prefix %s: %.3f %%\n", ref_prefixes[index], share);
    }
    if (strlen(ref_prefixes[index]) == 3U) {
      if (!TEST_ASSERT(stats.own_digit_with_letter[index] == stats.prefixes[index])) {
        fprintf(stderr, "own-digit prefix %s: %u with a letter after it of %u\n", ref_prefixes[index],
                stats.own_digit_with_letter[index], stats.prefixes[index]);
      }
      TEST_ASSERT(stats.own_digit_with_letter[index] > 0U);
    }
  }
  TEST_ASSERT_EQUAL(REF_PREFIX_COUNT, reached);
  for (index = 0U; index < DIGITS; ++index) {
    const double share = percent(stats.digits[index], stats.with_digit);
    if (!TEST_ASSERT((share >= 8.5) && (share <= 11.5))) {
      fprintf(stderr, "digit %u: %.3f %%\n", index, share);
    }
  }
  for (index = 0U; index < LETTERS; ++index) {
    const double ratio = ((double)stats.letters[index] * LETTERS) / (double)stats.suffix_letters;
    if (!TEST_ASSERT((ratio >= 0.75) && (ratio <= 1.25))) {
      fprintf(stderr, "letter %c: %.3f of 1/26\n", (char)('A' + index), ratio);
    }
  }
  /*
   * M1: rng_below(100) maps 3 high-byte values to 56 prefixes and 2 to 44, a 3:2 ratio. Each share stays inside the
   * 0.75-1.25 % bound of AC-T55 (2) (1.17 % and 0.78 %), so that bound alone does not catch it. The ratio of the class
   * means does: about 1.0 for the fair pick, 1.5 for rng_below().
   */
  {
    unsigned sums[2] = {0U, 0U};
    unsigned members[2] = {0U, 0U};
    double ratio;
    for (index = 0U; index < REF_PREFIX_COUNT; ++index) {
      const unsigned bytes = (((index + 1U) * 256U + 99U) / 100U) - ((index * 256U + 99U) / 100U);
      const unsigned class_index = (bytes == 3U) ? 1U : 0U;
      sums[class_index] += stats.prefixes[index];
      ++members[class_index];
    }
    TEST_ASSERT_EQUAL(44U, members[0]);
    TEST_ASSERT_EQUAL(56U, members[1]);
    ratio = ((double)sums[1] / (double)members[1]) / ((double)sums[0] / (double)members[0]);
    printf("prefix class ratio (rng_below 3-byte / 2-byte): %.3f\n", ratio);
    TEST_ASSERT((ratio > 0.9) && (ratio < 1.1));
  }
}

/*
 * AC-T55 (3): counts 18 to 40, 2000 callsigns each. No character outside the set (M4, M14), each allowed prefix
 * comes at least once, every item equals the model. Counts above 40 give the items of 40 (D-T66 cap).
 */
static void test_each_count(void) {
  static stats_t stats;
  unsigned count;
  unsigned indexes[REF_PREFIX_COUNT];

  load_empty_progress();
  for (count = FIRST_POSSIBLE_COUNT; count <= FULL; ++count) {
    unsigned total;
    unsigned index;
    unsigned digit;
    char digits[DIGITS];
    unsigned digit_total;
    memset(&stats, 0, sizeof(stats));
    run_and_compare((uint16_t)(0x2000U + count), (uint8_t)count, CALLSIGNS_PER_COUNT, &stats);
    TEST_ASSERT_EQUAL(0U, stats.mismatches);
    TEST_ASSERT_EQUAL(0U, stats.shape_errors);
    total = ref_allowed_prefixes(count, indexes);
    for (index = 0U; index < REF_PREFIX_COUNT; ++index) {
      const bool is_allowed = ref_prefix_allowed(ref_prefixes[index], count);
      if (is_allowed) {
        if (!TEST_ASSERT(stats.prefixes[index] > 0U)) {
          fprintf(stderr, "count %u: prefix %s never came\n", count, ref_prefixes[index]);
        }
      } else {
        TEST_ASSERT_EQUAL(0U, stats.prefixes[index]);
      }
    }
    TEST_ASSERT(total > 0U);
    digit_total = ref_set_chars(count, '0', '9', digits);
    for (digit = 0U; digit < DIGITS; ++digit) {
      const bool is_in_set = memchr(digits, (int)('0' + digit), digit_total) != NULL;
      if (!is_in_set) {
        if (!TEST_ASSERT_EQUAL(0U, stats.digits[digit])) {
          fprintf(stderr, "count %u: area digit %u is not in the set\n", count, digit);
        }
      }
    }
  }
  for (count = FULL + 1U; count <= 255U; count = (count < KOCH_CHAR_COUNT + 1U) ? count + 1U : 255U) {
    memset(&stats, 0, sizeof(stats));
    run_and_compare(0x3333U, (uint8_t)count, 200U, &stats);
    TEST_ASSERT_EQUAL(0U, stats.mismatches);
    TEST_ASSERT_EQUAL(0U, stats.shape_errors);
    if (count == 255U) {
      break;
    }
  }
}

/* AC-T55 (5), M12: no digit up to count 17; from 18 on there is a digit, a letter and a prefix. */
static void test_is_possible(void) {
  unsigned count;
  char digits[DIGITS];
  unsigned indexes[REF_PREFIX_COUNT];
  for (count = 0U; count <= 255U; ++count) {
    const bool expected = (ref_set_chars(count, '0', '9', digits) > 0U) && (ref_allowed_prefixes(count, indexes) > 0U);
    TEST_ASSERT_EQUAL(count >= FIRST_POSSIBLE_COUNT, expected);
    if (!TEST_ASSERT_EQUAL(expected, callsign_is_possible((uint8_t)count))) {
      fprintf(stderr, "count %u\n", count);
    }
  }
}

/*
 * Q-C4: callsign_make() with a count of 0 to 17 breaks its precondition. It must still stay inside a buffer of
 * CALLSIGN_LENGTH_MAX + 1 bytes. The heap buffer has the exact size, so ASan reports any write past it. The same
 * counts through the session seam write the engine item buffer.
 */
static void test_out_of_contract_counts_stay_in_bounds(void) {
  unsigned count;
  unsigned round;
  char* text = malloc(CALLSIGN_LENGTH_MAX + 1U);

  if (!TEST_ASSERT(text != NULL)) {
    return;
  }
  for (count = 0U; count < FIRST_POSSIBLE_COUNT; ++count) {
    rng_seed((uint16_t)(0x0100U + count));
    for (round = 0U; round < 2000U; ++round) {
      const uint8_t length = callsign_make((uint8_t)count, text);
      TEST_ASSERT(length <= CALLSIGN_LENGTH_MAX);
    }
  }
  free(text);
  load_empty_progress();
  for (count = 0U; count < FIRST_POSSIBLE_COUNT; ++count) {
    unsigned item;
    rng_seed((uint16_t)(0x0200U + count));
    start_free_session(count, (uint8_t)count);
    for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
      if (!TEST_ASSERT(next_item())) {
        break;
      }
      TEST_ASSERT(training_item_length() < TRAINING_TEXT_SIZE);
      (void)training_skip();
    }
  }
}

/* ---------- Engine: free sessions (AC-T56, AC-T60) ---------- */

/* RX level 30 at 20 WPM and TX level 5 at 25 WPM, loaded; the IAP counters are cleared. */
static void load_levels(void) {
  fake_iap_reset();
  progress_fixture_write(RX, 20U, 30U);
  progress_fixture_add(TX, 25U, 5U);
  (void)progress_load();
  fake_iap_clear_counters();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(0U);
  now = 1000U;
  fake_tick_set_now(now);
}

static uint8_t mode_wpm(uint8_t mode) { return (mode == RX) ? 20U : 25U; }

typedef struct {
  uint8_t record[PROGRESS_RECORD_SIZE];
  uint16_t levels[2][PROGRESS_SPEED_COUNT];
} progress_image_t;

static void progress_image(progress_image_t* image) {
  uint8_t mode;
  uint8_t wpm;
  memcpy(image->record, &fake_iap_memory()[EEPROM_MAP_PROGRESS_ADDRESS], sizeof(image->record));
  for (mode = RX; mode <= TX; ++mode) {
    for (wpm = PROGRESS_WPM_MIN; wpm <= PROGRESS_WPM_MAX; ++wpm) {
      image->levels[mode][wpm - PROGRESS_WPM_MIN] = progress_level(mode, wpm);
    }
  }
}

static bool progress_is_unchanged(const progress_image_t* before) {
  progress_image_t after;
  progress_image(&after);
  return (memcmp(before, &after, sizeof(after)) == 0) && (fake_iap_command_count(CMD_PROGRAM) == 0U) &&
         (fake_iap_command_count(CMD_ERASE) == 0U);
}

#define ANSWER_CORRECT 0U
#define ANSWER_WRONG_ONCE 1U /* one wrong attempt, then correct */
#define ANSWER_SKIP 2U       /* the long next skip in ITEM */
#define ANSWER_WRONG_SKIP 3U /* one wrong attempt, then the skip in RESULT */
#define ANSWER_ALL_WRONG 4U  /* wrong until FAILED (3 attempts), or 5 wrong then correct with unlimited attempts */
#define ANSWER_KINDS 5U

/* One whole wrong answer: a wrong first character, then the rest (Receiving checks only a complete answer). */
static training_check_t answer_wrong(void) {
  const char MEM_XDATA* item = training_item_text();
  training_check_t check = training_add_char(WRONG_CHAR);
  uint8_t index;
  for (index = 1U; (index < training_item_length()) && (check == TRAINING_CHECK_PARTIAL); ++index) {
    check = training_add_char(item[index]);
  }
  return check;
}

/* Answers the current item. Returns true when it scores a point. */
static bool answer_item(uint8_t kind) {
  unsigned attempt;
  if (kind == ANSWER_CORRECT) {
    answer_correct();
    return TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  }
  if (kind == ANSWER_SKIP) {
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
    return false;
  }
  if (kind == ANSWER_ALL_WRONG) {
    for (attempt = 0U; attempt < 5U; ++attempt) {
      const training_check_t check = answer_wrong();
      if (check == TRAINING_CHECK_FAILED) {
        TEST_ASSERT(!training_has_unlimited_attempts());
        return false;
      }
      TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, check);
      TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at((uint16_t)(now + STEP_MS)));
    }
    TEST_ASSERT(training_has_unlimited_attempts());
    answer_correct();
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
    return false;
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
  if (kind == ANSWER_WRONG_SKIP) {
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
    return false;
  }
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at((uint16_t)(now + STEP_MS)));
  answer_correct();
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  return false;
}

/*
 * Runs a started free session to END with the answers of @p kinds (NULL: all correct), compares each item with the
 * model from @p seed (when @p seed is not 0) and returns the expected score.
 */
static unsigned run_session(const uint8_t* kinds, uint8_t count, uint16_t seed) {
  unsigned item;
  unsigned score = 0U;
  if (seed != 0U) {
    ref_seed(seed);
  }
  for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
    if (!TEST_ASSERT(next_item())) {
      return score;
    }
    TEST_ASSERT_EQUAL(item + 1U, training_item_number());
    TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_count());
    if (seed != 0U) {
      char expected[16];
      unsigned prefix_index;
      (void)ref_make(count, expected, &prefix_index);
      if (!TEST_ASSERT_EQUAL(0, strcmp(expected, (const char*)training_item_text()))) {
        fprintf(stderr, "item %u: got '%s', model '%s'\n", item + 1U, (const char*)training_item_text(), expected);
      }
    }
    if (answer_item((kinds != NULL) ? kinds[item] : ANSWER_CORRECT)) {
      ++score;
    }
  }
  TEST_ASSERT(!next_item());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(score, training_score());
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  return score;
}

/* M7, M9: training_prepare() ends the free state; the alphabet count is the free count, not the level's. */
static void test_free_flag_and_count(void) {
  static const uint8_t counts[] = {18U, 29U, FULL};
  uint8_t mode;
  uint8_t index;

  load_levels();
  for (mode = RX; mode <= TX; ++mode) {
    for (index = 0U; index < (uint8_t)sizeof(counts); ++index) {
      training_prepare(mode, mode_wpm(mode), 5U, false);
      training_set_free(counts[index]);
      TEST_ASSERT(training_is_free());
      TEST_ASSERT_EQUAL(counts[index], training_alphabet_count());
      training_start();
      TEST_ASSERT_EQUAL(counts[index], training_alphabet_count());
      TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_count());
      training_prepare(mode, mode_wpm(mode), 5U, false);
      TEST_ASSERT(!training_is_free());
      TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
      training_start();
      TEST_ASSERT(!training_is_free());
      TEST_ASSERT_EQUAL(koch_item_count(training_level()), training_item_count());
      TEST_ASSERT(koch_item_count(training_level()) != TRAINING_FREE_ITEMS);
      TEST_ASSERT_EQUAL(koch_alphabet_count(training_level()), training_alphabet_count());
      TEST_ASSERT(koch_alphabet_count(training_level()) != counts[index]);
    }
  }
}

/*
 * M6, AC-T56 (4), AC-T60 at the engine: all correct (a score that passes every Koch threshold), all wrong, skips
 * and mixed answers give TRAINING_OUTCOME_FREE, no IAP erase or write, the same record bytes, the same levels at every
 * speed and the same selected level. Both modes, 3 attempts and unlimited attempts.
 */
static void test_free_outcomes_keep_progress(void) {
  uint8_t kinds[TRAINING_FREE_ITEMS];
  progress_image_t before;
  uint8_t mode;
  uint8_t pattern;
  uint8_t item;
  uint8_t unlimited;

  load_levels();
  progress_image(&before);
  for (mode = RX; mode <= TX; ++mode) {
    for (unlimited = 0U; unlimited < 2U; ++unlimited) {
      for (pattern = 0U; pattern <= ANSWER_KINDS; ++pattern) {
        uint16_t selected;
        unsigned score;
        for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
          kinds[item] = (pattern < ANSWER_KINDS) ? pattern : (uint8_t)(item % ANSWER_KINDS);
        }
        rng_seed((uint16_t)(0x0700U + pattern));
        training_prepare(mode, mode_wpm(mode), 5U, unlimited != 0U);
        training_set_free(FULL);
        selected = training_selected_level();
        training_start();
        score = run_session(kinds, FULL, (uint16_t)(0x0700U + pattern));
        TEST_ASSERT_EQUAL((pattern == ANSWER_CORRECT) ? 20U : ((pattern < ANSWER_KINDS) ? 0U : 4U), score);
        TEST_ASSERT_EQUAL(selected, training_selected_level());
        TEST_ASSERT_EQUAL(selected, training_earned_level());
        TEST_ASSERT(progress_is_unchanged(&before));
      }
    }
  }
  TEST_ASSERT_EQUAL(30U, progress_level(RX, 20U));
  TEST_ASSERT_EQUAL(5U, progress_level(TX, 25U));
}

/* F3: a skip on item 20 (in ITEM, and in RESULT after a wrong attempt) scores nothing; the session still ends. */
static void test_skip_on_item_20(void) {
  uint8_t kinds[TRAINING_FREE_ITEMS];
  uint8_t mode;
  uint8_t last;

  load_levels();
  for (mode = RX; mode <= TX; ++mode) {
    for (last = ANSWER_SKIP; last <= ANSWER_WRONG_SKIP; ++last) {
      memset(kinds, ANSWER_CORRECT, sizeof(kinds));
      kinds[TRAINING_FREE_ITEMS - 1U] = last;
      rng_seed(0x0F03U);
      training_prepare(mode, mode_wpm(mode), 5U, false);
      training_set_free(FULL);
      training_start();
      TEST_ASSERT_EQUAL(19U, run_session(kinds, FULL, 0x0F03U));
      TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_number());
      TEST_ASSERT(training_is_skipped());
    }
  }
}

/* F7: unlimited attempts, 300 wrong answers then a correct one: no point, the counter stops at 255, still 20 items. */
static void test_300_wrong_then_correct(void) {
  uint8_t mode;
  unsigned attempt;
  unsigned item;

  load_levels();
  for (mode = RX; mode <= TX; ++mode) {
    rng_seed(0x0F07U);
    training_prepare(mode, mode_wpm(mode), 5U, true);
    training_set_free(FULL);
    training_start();
    TEST_ASSERT(next_item());
    for (attempt = 1U; attempt <= 300U; ++attempt) {
      TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
      TEST_ASSERT_EQUAL((attempt < TRAINING_WRONG_MAX) ? attempt : TRAINING_WRONG_MAX, training_wrong_attempts());
      TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at((uint16_t)(now + STEP_MS)));
    }
    answer_correct();
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
    TEST_ASSERT_EQUAL(0U, training_score());
    TEST_ASSERT_EQUAL(1U, training_item_number());
    TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_count());
    for (item = 2U; item <= TRAINING_FREE_ITEMS; ++item) {
      TEST_ASSERT(next_item());
      answer_correct();
    }
    TEST_ASSERT(!next_item());
    TEST_ASSERT_EQUAL(19U, training_score());
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  }
}

/*
 * F6, F5 at the engine: a Koch session that passes saves its level; a free session after it keeps that level; a Koch
 * session after the free session gets Koch items and the Koch item count, and its pass saves again.
 */
static void run_koch_session_to_pass(void) {
  while (next_item()) {
    answer_correct();
  }
}

static void test_koch_pass_then_free_then_koch(void) {
  progress_image_t before;
  uint8_t mode;

  for (mode = RX; mode <= TX; ++mode) {
    const uint16_t level = (mode == RX) ? 30U : 5U;
    load_levels();
    rng_seed(0x0F06U);
    training_prepare(mode, mode_wpm(mode), 5U, false);
    training_start();
    TEST_ASSERT(!training_is_free());
    run_koch_session_to_pass();
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
    TEST_ASSERT_EQUAL(level + 1U, progress_level(mode, mode_wpm(mode)));
    TEST_ASSERT(fake_iap_command_count(CMD_PROGRAM) > 0U);
    fake_iap_clear_counters();
    progress_image(&before);

    rng_seed(0x0F05U);
    training_prepare(mode, mode_wpm(mode), 5U, false);
    training_set_free(FULL);
    training_start();
    (void)run_session(NULL, FULL, 0x0F05U);
    TEST_ASSERT(progress_is_unchanged(&before));
    TEST_ASSERT_EQUAL(level + 1U, progress_level(mode, mode_wpm(mode)));

    training_prepare(mode, mode_wpm(mode), 5U, false);
    TEST_ASSERT_EQUAL(level + 1U, training_selected_level());
    training_start();
    TEST_ASSERT_EQUAL(koch_item_count(level + 1U), training_item_count());
    run_koch_session_to_pass();
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
    TEST_ASSERT_EQUAL(level + 2U, progress_level(mode, mode_wpm(mode)));
  }
}

/* koch_learned_count() capped at 40, as FT-3 computes the Learned count. */
static uint8_t learned_count(uint16_t level) {
  const uint8_t count = koch_learned_count(level);
  return (count > FULL) ? FULL : count;
}

/*
 * F8: 520 free sessions over Mode, Chars (Learned at random levels, or Full), attempts and seeds, with random answers.
 * Each has 20 items that equal the model for its count, outcome FREE, the expected score and no IAP write.
 */
static void test_random_walk_of_free_sessions(void) {
  uint8_t kinds[TRAINING_FREE_ITEMS];
  progress_image_t before;
  uint32_t state = 0x9E3779B9U;
  unsigned session;
  unsigned blocked = 0U;
  unsigned items = 0U;

  load_levels();
  progress_image(&before);
  for (session = 0U; items < 520U * TRAINING_FREE_ITEMS; ++session) {
    uint8_t mode;
    uint8_t count;
    uint16_t seed;
    uint8_t item;
    unsigned expected_score = 0U;
    state = state * 1664525U + 1013904223U;
    mode = (uint8_t)((state >> 8) & 1U);
    count = ((state >> 9) & 1U) ? FULL : learned_count((uint16_t)((state >> 10) % 70U));
    if ((state & 0x0F000000U) == 0U) {
      count = learned_count(65535U);
    }
    seed = (uint16_t)(state >> 16);
    if (!callsign_is_possible(count)) {
      ++blocked;
      continue;
    }
    for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
      state = state * 1664525U + 1013904223U;
      kinds[item] = (uint8_t)((state >> 16) % ANSWER_KINDS);
      if (kinds[item] == ANSWER_CORRECT) {
        ++expected_score;
      }
    }
    rng_seed(seed);
    training_prepare(mode, mode_wpm(mode), (uint8_t)(1U + (session % 8U)), ((state >> 24) & 1U) != 0U);
    training_set_free(count);
    training_start();
    TEST_ASSERT_EQUAL(expected_score, run_session(kinds, count, (seed == 0U) ? 0xACE1U : seed));
    items += TRAINING_FREE_ITEMS;
  }
  printf("free walk: %u sessions, %u blocked counts, %u items\n", session, blocked, items);
  TEST_ASSERT(items >= 500U * TRAINING_FREE_ITEMS);
  TEST_ASSERT(progress_is_unchanged(&before));
}

int main(void) {
  TEST_RUN(test_reference_table_facts);
  TEST_RUN(test_is_possible);
  TEST_RUN(test_full_model_and_statistics);
  TEST_RUN(test_each_count);
  TEST_RUN(test_out_of_contract_counts_stay_in_bounds);
  TEST_RUN(test_free_flag_and_count);
  TEST_RUN(test_free_outcomes_keep_progress);
  TEST_RUN(test_skip_on_item_20);
  TEST_RUN(test_300_wrong_then_correct);
  TEST_RUN(test_koch_pass_then_free_then_koch);
  TEST_RUN(test_random_walk_of_free_sessions);
  return TEST_FINISH();
}
