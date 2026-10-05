#include "fake_sidetone_pwm.h"

static fake_sidetone_pwm_event_t fake_log[FAKE_SIDETONE_PWM_LOG_SIZE];
static size_t fake_log_count;
static bool fake_is_log_overflowed;

static bool fake_is_configured;
static size_t fake_early_writes;
static size_t fake_configures;
static size_t fake_pins_configures;
static size_t fake_timer_starts;
static uint16_t fake_configured_arr;

static uint16_t fake_arr_preload;
static uint16_t fake_arr_shadow;
static uint16_t fake_ccr1_preload;
static uint16_t fake_ccr1_shadow;
static uint16_t fake_ccr2_preload;
static uint16_t fake_ccr2_shadow;
static bool fake_is_update_flag;
static uint8_t fake_eno;
static size_t fake_updates;
static size_t fake_violations;

static bool fake_is_headphones_inserted;
static size_t fake_plug_reads;
static size_t fake_preemptions;
static void (*fake_preemption_hook)(void);

static void fake_record(uint8_t kind, uint16_t value) {
  if (fake_log_count >= FAKE_SIDETONE_PWM_LOG_SIZE) {
    if (!fake_is_log_overflowed) {
      ++fake_violations;
    }
    fake_is_log_overflowed = true;
    return;
  }
  fake_log[fake_log_count].kind = kind;
  fake_log[fake_log_count].value = value;
  ++fake_log_count;
}

static void fake_register_access(uint8_t kind, uint16_t value) {
  if (!fake_is_configured) {
    ++fake_early_writes;
  }
  fake_record(kind, value);
}

static void fake_load_shadows(void) {
  fake_arr_shadow = fake_arr_preload;
  fake_ccr1_shadow = fake_ccr1_preload;
  fake_ccr2_shadow = fake_ccr2_preload;
  fake_is_update_flag = true;
}

void fake_sidetone_pwm_reset(void) {
  fake_log_count = 0U;
  fake_is_log_overflowed = false;
  fake_is_configured = false;
  fake_early_writes = 0U;
  fake_configures = 0U;
  fake_pins_configures = 0U;
  fake_timer_starts = 0U;
  fake_configured_arr = 0U;
  fake_arr_preload = 0U;
  fake_arr_shadow = 0U;
  fake_ccr1_preload = 0U;
  fake_ccr1_shadow = 0U;
  fake_ccr2_preload = 0U;
  fake_ccr2_shadow = 0U;
  fake_is_update_flag = false;
  fake_eno = 0U;
  fake_updates = 0U;
  fake_violations = 0U;
  fake_is_headphones_inserted = false;
  fake_plug_reads = 0U;
  fake_preemptions = 0U;
  fake_preemption_hook = NULL;
}

void fake_sidetone_pwm_set_headphones(bool is_inserted) { fake_is_headphones_inserted = is_inserted; }

void fake_sidetone_pwm_update_event(void) {
  fake_load_shadows();
  fake_record(FAKE_SIDETONE_PWM_EVENT_UPDATE_EVENT, 0U);
}

void fake_sidetone_pwm_set_preemption_hook(void (*hook)(void)) { fake_preemption_hook = hook; }

size_t fake_sidetone_pwm_preemption_count(void) { return fake_preemptions; }
size_t fake_sidetone_pwm_configure_count(void) { return fake_configures; }
size_t fake_sidetone_pwm_pins_configure_count(void) { return fake_pins_configures; }
size_t fake_sidetone_pwm_timer_start_count(void) { return fake_timer_starts; }
size_t fake_sidetone_pwm_writes_before_configure(void) { return fake_early_writes; }
uint16_t fake_sidetone_pwm_configured_arr(void) { return fake_configured_arr; }
uint16_t fake_sidetone_pwm_arr(void) { return fake_arr_preload; }
uint16_t fake_sidetone_pwm_shadow_arr(void) { return fake_arr_shadow; }
uint16_t fake_sidetone_pwm_compare_speaker(void) { return fake_ccr1_preload; }
uint16_t fake_sidetone_pwm_compare_headphones(void) { return fake_ccr2_preload; }
uint16_t fake_sidetone_pwm_shadow_compare_speaker(void) { return fake_ccr1_shadow; }
uint16_t fake_sidetone_pwm_shadow_compare_headphones(void) { return fake_ccr2_shadow; }
uint8_t fake_sidetone_pwm_eno(void) { return fake_eno; }
bool fake_sidetone_pwm_is_update_flag_set(void) { return fake_is_update_flag; }
size_t fake_sidetone_pwm_update_count(void) { return fake_updates; }
size_t fake_sidetone_pwm_violation_count(void) { return fake_violations; }
bool fake_sidetone_pwm_is_log_overflowed(void) { return fake_is_log_overflowed; }
size_t fake_sidetone_pwm_plug_read_count(void) { return fake_plug_reads; }
size_t fake_sidetone_pwm_event_count(void) { return fake_log_count; }

fake_sidetone_pwm_event_t fake_sidetone_pwm_event_at(size_t index) {
  const fake_sidetone_pwm_event_t none = {0U, 0U};
  return (index < fake_log_count) ? fake_log[index] : none;
}

void fake_sidetone_pwm_clear_log(void) { fake_log_count = 0U; }

void fake_sidetone_pwm_configure(uint16_t arr) {
  fake_is_configured = true;
  ++fake_configures;
  fake_configured_arr = arr;
  fake_arr_preload = arr;
  fake_arr_shadow = arr;
  fake_ccr1_preload = 0U;
  fake_ccr1_shadow = 0U;
  fake_ccr2_preload = 0U;
  fake_ccr2_shadow = 0U;
  fake_eno = 0U;
  fake_is_update_flag = false;
  fake_record(FAKE_SIDETONE_PWM_EVENT_CONFIGURE, arr);
}

void fake_sidetone_pwm_pins_configure(void) {
  ++fake_pins_configures;
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_PINS_CONFIGURE, 0U);
}

bool fake_sidetone_pwm_is_headphones(void) {
  ++fake_plug_reads;
  return fake_is_headphones_inserted;
}

void fake_sidetone_pwm_timer_start(void) {
  ++fake_timer_starts;
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_TIMER_START, 0U);
}

void fake_sidetone_pwm_set_arr(uint16_t value) {
  fake_arr_preload = value;
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_SET_ARR, value);
}

void fake_sidetone_pwm_set_compare_speaker(uint16_t value) {
  fake_ccr1_preload = value;
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER, value);
}

void fake_sidetone_pwm_set_compare_headphones(uint16_t value) {
  fake_ccr2_preload = value;
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES, value);
}

void fake_sidetone_pwm_generate_update(void) {
  ++fake_updates;
  fake_load_shadows();
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE, 0U);
}

void fake_sidetone_pwm_clear_update_flag(void) {
  fake_is_update_flag = false;
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG, 0U);
}

bool fake_sidetone_pwm_read_update_flag(void) {
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_READ_UPDATE_FLAG, fake_is_update_flag ? 1U : 0U);
  return fake_is_update_flag;
}

void fake_sidetone_pwm_set_eno(uint8_t value) {
  const bool is_speaker_click = (value == FAKE_SIDETONE_PWM_ENO_SPEAKER) && (fake_ccr1_shadow != 0U);
  const bool is_headphones_click = (value == FAKE_SIDETONE_PWM_ENO_HEADPHONES) && (fake_ccr2_shadow != 0U);
  const bool is_allowed = (value == FAKE_SIDETONE_PWM_ENO_OFF) || (value == FAKE_SIDETONE_PWM_ENO_SPEAKER) ||
                          (value == FAKE_SIDETONE_PWM_ENO_HEADPHONES);

  if (!is_allowed || is_speaker_click || is_headphones_click) {
    ++fake_violations;
  }
  fake_eno = value;
  fake_register_access(FAKE_SIDETONE_PWM_EVENT_SET_ENO, value);
}

void fake_sidetone_pwm_preemption_point(void) {
  ++fake_preemptions;
  fake_record(FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT, 0U);
  if (fake_preemption_hook != NULL) {
    fake_preemption_hook();
  }
}
