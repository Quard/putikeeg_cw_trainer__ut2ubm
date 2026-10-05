#ifndef QA_BUS_H
#define QA_BUS_H

/*
 * QA bus oracle, written apart from support/lcd_protocol_decoder.c and support/lcd_controller_model.c so that the
 * QA tests do not share an oracle with the code under test. Pin levels use bit n = fake_lcd_pin_t n, which is also
 * the channel order of lcd_dump.sr (D0 CS, D1 RST, D2 A0, D3 SCK, D4 SDA).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fake_lcd_pins.h"

#define QA_BIT_CS (1U << FAKE_LCD_PIN_CS)
#define QA_BIT_RST (1U << FAKE_LCD_PIN_RST)
#define QA_BIT_A0 (1U << FAKE_LCD_PIN_A0)
#define QA_BIT_SCK (1U << FAKE_LCD_PIN_SCK)
#define QA_BIT_SDA (1U << FAKE_LCD_PIN_SDA)
#define QA_ALL_HIGH (QA_BIT_CS | QA_BIT_RST | QA_BIT_A0 | QA_BIT_SCK | QA_BIT_SDA)

#define QA_RAM_PAGES 9U
#define QA_RAM_COLUMNS 132U

typedef struct {
  bool is_data;
  uint8_t value;
} qa_byte_t;

/* Mode 3 rule breaks. Every rule is checked only on a real level change. */
typedef struct {
  size_t sda_change_while_sck_high; /* CS low */
  size_t cs_change_while_sck_low;
  size_t a0_change_inside_byte; /* CS low and (bits shifted or SCK low) */
  size_t cs_rise_inside_byte;
  size_t rst_low_while_selected; /* either order */
} qa_rules_t;

typedef struct {
  uint8_t levels;
  uint8_t shift;
  uint8_t bit_count;
  qa_rules_t rules;
  size_t byte_count; /* may exceed capacity */
  size_t cs_fall_count;
  qa_byte_t* out;
  size_t capacity;
} qa_bus_t;

static inline void qa_bus_start(qa_bus_t* bus, uint8_t levels, qa_byte_t* out, size_t capacity) {
  memset(bus, 0, sizeof(*bus));
  bus->levels = levels;
  bus->out = out;
  bus->capacity = capacity;
}

static inline size_t qa_rules_total(const qa_rules_t* rules) {
  return rules->sda_change_while_sck_high + rules->cs_change_while_sck_low + rules->a0_change_inside_byte +
         rules->cs_rise_inside_byte + rules->rst_low_while_selected;
}

static inline bool qa_bus_is_high(const qa_bus_t* bus, uint8_t bit) { return (bus->levels & bit) != 0U; }

/* Applies one pin change; @p bit is one QA_BIT_* value. */
static inline void qa_bus_set(qa_bus_t* bus, uint8_t bit, bool is_high) {
  const bool is_selected = !qa_bus_is_high(bus, QA_BIT_CS);
  const bool is_sck_high = qa_bus_is_high(bus, QA_BIT_SCK);

  if (qa_bus_is_high(bus, bit) == is_high) {
    return;
  }
  if (bit == QA_BIT_SDA) {
    bus->rules.sda_change_while_sck_high += (is_selected && is_sck_high) ? 1U : 0U;
  } else if (bit == QA_BIT_A0) {
    bus->rules.a0_change_inside_byte += (is_selected && ((bus->bit_count != 0U) || !is_sck_high)) ? 1U : 0U;
  } else if (bit == QA_BIT_RST) {
    bus->rules.rst_low_while_selected += (!is_high && is_selected) ? 1U : 0U;
  } else if (bit == QA_BIT_CS) {
    bus->rules.cs_change_while_sck_low += is_sck_high ? 0U : 1U;
    if (is_high) {
      bus->rules.cs_rise_inside_byte += (bus->bit_count != 0U) ? 1U : 0U;
      bus->bit_count = 0U;
      bus->shift = 0U;
    } else {
      bus->rules.rst_low_while_selected += qa_bus_is_high(bus, QA_BIT_RST) ? 0U : 1U;
      ++bus->cs_fall_count;
    }
  } else if ((bit == QA_BIT_SCK) && is_high && is_selected) {
    bus->shift = (uint8_t)((bus->shift << 1) | (qa_bus_is_high(bus, QA_BIT_SDA) ? 1U : 0U));
    ++bus->bit_count;
    if (bus->bit_count == 8U) {
      if (bus->byte_count < bus->capacity) {
        bus->out[bus->byte_count].is_data = qa_bus_is_high(bus, QA_BIT_A0);
        bus->out[bus->byte_count].value = bus->shift;
      }
      ++bus->byte_count;
      bus->bit_count = 0U;
      bus->shift = 0U;
    }
  } else {
    /* SCK fall, or SCK edge while deselected: no data effect. */
  }
  bus->levels = (uint8_t)(is_high ? (bus->levels | bit) : (bus->levels & (uint8_t)~bit));
}

/*
 * Applies a sample with possibly several changed pins. A falling CS is applied last and a rising CS first, which
 * is the only order in which a simultaneous A0 change is legal.
 */
static inline void qa_bus_apply_sample(qa_bus_t* bus, uint8_t levels) {
  static const uint8_t data_bits[] = {QA_BIT_RST, QA_BIT_A0, QA_BIT_SDA, QA_BIT_SCK};
  const bool is_cs_high = (levels & QA_BIT_CS) != 0U;
  size_t i;

  if (is_cs_high) {
    qa_bus_set(bus, QA_BIT_CS, true);
  }
  for (i = 0U; i < sizeof(data_bits); ++i) {
    qa_bus_set(bus, data_bits[i], (levels & data_bits[i]) != 0U);
  }
  if (!is_cs_high) {
    qa_bus_set(bus, QA_BIT_CS, false);
  }
}

/* Feeds fake pin events [@p first, @p end) into @p bus; the configure-outputs marker is skipped. */
static inline void qa_bus_feed_events(qa_bus_t* bus, size_t first, size_t end) {
  size_t i;
  for (i = first; i < end; ++i) {
    const fake_lcd_pin_event_t* event = fake_lcd_pins_event(i);
    if ((event != NULL) && ((unsigned)event->pin < (unsigned)FAKE_LCD_PIN_COUNT)) {
      qa_bus_set(bus, (uint8_t)(1U << (unsigned)event->pin), event->level != 0U);
    }
  }
}

/* Independent RAM model: page (0xB0-0xB8), column (0x0X / 0x1X) and data auto-increment. Other commands only count. */
typedef struct {
  uint8_t ram[QA_RAM_PAGES][QA_RAM_COLUMNS];
  uint8_t page;
  unsigned column;
  bool is_contrast_value_next;
  size_t out_of_ram_writes;
} qa_ram_t;

static inline void qa_ram_reset(qa_ram_t* ram, uint8_t fill) {
  memset(ram, 0, sizeof(*ram));
  memset(ram->ram, fill, sizeof(ram->ram));
}

static inline void qa_ram_apply(qa_ram_t* ram, const qa_byte_t* bytes, size_t count) {
  size_t i;
  for (i = 0U; i < count; ++i) {
    const uint8_t value = bytes[i].value;
    if (bytes[i].is_data) {
      if ((ram->page < QA_RAM_PAGES) && (ram->column < QA_RAM_COLUMNS)) {
        ram->ram[ram->page][ram->column] = value;
      } else {
        ++ram->out_of_ram_writes;
      }
      ++ram->column;
    } else if (ram->is_contrast_value_next) {
      ram->is_contrast_value_next = false;
    } else if (value == 0x81U) {
      ram->is_contrast_value_next = true;
    } else if ((value & 0xF0U) == 0xB0U) {
      ram->page = (uint8_t)(value & 0x0FU);
    } else if ((value & 0xF0U) == 0x00U) {
      ram->column = (ram->column & 0xF0U) | value;
    } else if ((value & 0xF0U) == 0x10U) {
      ram->column = (ram->column & 0x0FU) | ((unsigned)(value & 0x0FU) << 4);
    } else {
      /* Display commands do not change the RAM. */
    }
  }
}

#endif /* QA_BUS_H */
