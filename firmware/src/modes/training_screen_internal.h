#ifndef MODES_TRAINING_SCREEN_INTERNAL_H
#define MODES_TRAINING_SCREEN_INTERNAL_H

/*
 * Draw helpers of training_screen.c for receiving.c and sending.c (ui-ux.md sections 5.11 to 5.15). Private to the
 * training modes. A text is composed in training_screen_text and then drawn.
 */

#include <stdbool.h>
#include <stdint.h>

#include "gfx/gfx.h"
#include "mcu/compiler.h"

#define TRAINING_SCREEN_COLUMNS 25U
#define TRAINING_SCREEN_STATUS_ROW 1U
#define TRAINING_SCREEN_ITEM_FIRST_ROW 3U /* the item line is rows 3 and 4, y 24-39 */
#define TRAINING_SCREEN_ITEM_LAST_ROW 4U
#define TRAINING_SCREEN_LAST_ROW 7U

extern MEM_XDATA char training_screen_text[TRAINING_SCREEN_COLUMNS + 1U];

/** Empties training_screen_text. */
void training_screen_compose(void);
/** Appends @p text to training_screen_text, at most up to TRAINING_SCREEN_COLUMNS characters. */
void training_screen_append(const char* text);
/** Appends the decimal digits of @p value. */
void training_screen_append_u16(uint16_t value);
/** Sets every pixel of rows @p first_row to @p last_row off. */
void training_screen_clear_rows(uint8_t first_row, MEM_XDATA uint8_t last_row);
/** Clears row @p row and draws training_screen_text centered in it (ui-ux.md section 5.11: (25 - length) / 2). */
void training_screen_draw_centered(uint8_t row);
/** Draws @p text in normal video, 5 x 8 font, from cell (@p row, @p column). */
void training_screen_draw_text(uint8_t row, MEM_XDATA uint8_t column, const char* MEM_XDATA text);
/**
 * Clears the item line and draws the characters of @p text in @p style, in the 8 x 16 font, at the slots of an item of
 * training_item_length() characters (ui-ux.md section 5.11.2): an item of one character is centered, a word has one
 * cell per character.
 */
void training_screen_draw_item_text(const char MEM_XDATA* text, MEM_XDATA uint8_t style);
/** Draws the status row of ui-ux.md section 5.14 for the engine state; Sending shows the keyed text in @p style. */
void training_screen_draw_status(uint8_t style);
/** Draws the block (@p is_block) or the bar under slot @p slot of an item of @p count characters. */
void training_screen_draw_mark(uint8_t slot, MEM_XDATA uint8_t count, MEM_XDATA bool is_block);

#endif /* MODES_TRAINING_SCREEN_INTERNAL_H */
