#include "fake_player.h"

#include <string.h>

static size_t fake_starts;
static size_t fake_stops;
static bool fake_is_running;
static bool fake_is_accepted = true;
static char fake_text[PLAYER_TEXT_MAX + 1U];
static uint8_t fake_wpm;
static uint8_t fake_eff_wpm;
static uint8_t fake_calls[FAKE_PLAYER_LOG_SIZE];
static size_t fake_call_count;
static void (*fake_start_hook)(void);

static void fake_log(uint8_t call) {
  if (fake_call_count < FAKE_PLAYER_LOG_SIZE) {
    fake_calls[fake_call_count] = call;
  }
  ++fake_call_count;
}

void fake_player_reset(void) {
  fake_starts = 0U;
  fake_stops = 0U;
  fake_is_running = false;
  fake_is_accepted = true;
  fake_text[0] = '\0';
  fake_wpm = 0U;
  fake_eff_wpm = 0U;
  fake_call_count = 0U;
  fake_start_hook = NULL;
}

size_t fake_player_start_count(void) { return fake_starts; }
size_t fake_player_stop_count(void) { return fake_stops; }
bool fake_player_is_running(void) { return fake_is_running; }
void fake_player_finish(void) { fake_is_running = false; }
void fake_player_set_start_result(bool is_accepted) { fake_is_accepted = is_accepted; }
const char* fake_player_last_text(void) { return fake_text; }
uint8_t fake_player_last_wpm(void) { return fake_wpm; }
uint8_t fake_player_last_eff_wpm(void) { return fake_eff_wpm; }
size_t fake_player_call_count(void) { return fake_call_count; }
uint8_t fake_player_call_at(size_t index) { return (index < FAKE_PLAYER_LOG_SIZE) ? fake_calls[index] : 0U; }
void fake_player_set_start_hook(void (*hook)(void)) { fake_start_hook = hook; }

bool player_start(const char* text, uint8_t length, uint8_t wpm, uint8_t eff_wpm) {
  /* The real player stops a running playback first. */
  fake_is_running = false;
  ++fake_starts;
  fake_log(FAKE_PLAYER_CALL_START);
  if (!fake_is_accepted || (length == 0U) || (length > PLAYER_TEXT_MAX)) {
    return false;
  }
  memcpy(fake_text, text, length);
  fake_text[length] = '\0';
  fake_wpm = wpm;
  fake_eff_wpm = eff_wpm;
  fake_is_running = true;
  if (fake_start_hook != NULL) {
    fake_start_hook();
  }
  return true;
}

bool player_is_busy(void) { return fake_is_running; }

void player_stop(void) {
  ++fake_stops;
  fake_log(FAKE_PLAYER_CALL_STOP);
  fake_is_running = false;
}
