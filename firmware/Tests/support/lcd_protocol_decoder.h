#ifndef LCD_PROTOCOL_DECODER_H
#define LCD_PROTOCOL_DECODER_H

/* Decodes logged fake pin events into the bytes an SPI mode 3, MSB-first LCD controller would receive. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  bool is_data; /* A0 level at the 8th rising SCK edge */
  uint8_t value;
} lcd_decoded_byte_t;

/**
 * Decodes events [@p first_event, @p end_event). Events before @p first_event only set the pin levels, starting
 * from the fake_lcd_pins_reset() state (all 1). @p end_event is clamped to the event count.
 *
 * Shifts in SDA on each SCK rise while CS is low; CS high resets the bit counter.
 * Counts these violations into @p violation_count (may be NULL): SDA change while CS low and SCK high; A0 change
 * inside a byte (CS low and a bit counted or SCK low); CS change while SCK low; CS rise inside a byte; RST low
 * while CS low.
 *
 * Writes at most @p capacity bytes to @p out and returns the number of bytes decoded, which can be larger.
 */
size_t lcd_protocol_decode(size_t first_event, size_t end_event, lcd_decoded_byte_t* out, size_t capacity,
                           size_t* violation_count);

#endif /* LCD_PROTOCOL_DECODER_H */
