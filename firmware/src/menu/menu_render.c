#include <stddef.h>

#include "gfx/gfx.h"
#include "koch/koch.h"
#include "lcd/lcd_fb.h"
#include "menu_internal.h"
#include "modes/receiving.h"
#include "modes/sending.h"
#include "progress/progress.h"
#include "settings/settings.h"

/* The target of a mode item goes to progress_level() as it is (D-T41). */
#if (MENU_MODE_RECEIVING != PROGRESS_MODE_RECEIVING) || (MENU_MODE_SENDING != PROGRESS_MODE_SENDING)
#error "The menu modes must equal the progress modes"
#endif
/* RX Speed of Receiving (mode 0), TX Speed of Sending (mode 1). */
#define MENU_MODE_SPEED_ID(mode) ((settings_id_t)(SETTINGS_ID_RX_WPM + (uint8_t)(2U * (mode))))
_Static_assert((MENU_MODE_SPEED_ID(MENU_MODE_RECEIVING) == SETTINGS_ID_RX_WPM) &&
                   (MENU_MODE_SPEED_ID(MENU_MODE_SENDING) == SETTINGS_ID_TX_WPM),
               "each mode must read its own speed");

/* Screen grid (references/internal/ui-ux.md sections 4 and 5): 8 rows x 25 columns of 5 x 8 cells from x = 2. */
#define GRID_X 2U
#define GRID_COLUMNS 25U
#define GRID_LAST_COLUMN 24U
#define VALUE_END_COLUMN 23U   /* the value ends in column 22 */
#define EDIT_MARKER_COLUMN 21U /* '<' is in column 21 - fw */
#define TITLE_ROW 0U
#define FIRST_ITEM_ROW 1U
#define MESSAGE_ROW 3U
#define BUTTON_ROW 5U
#define BAND_HEIGHT 7
#define VALUE_TEXT_SIZE 12U /* "Punctuation" and NUL */

/* Dialog and message buttons: first highlight cell; the text starts one cell later (ui-ux.md 5.7, 5.8). */
#define BUTTON_NO_FIRST_CELL 7U
#define BUTTON_YES_FIRST_CELL 13U
#define BUTTON_OK_FIRST_CELL 10U
#define BUTTON_SHORT_CELLS 4U /* No, OK */
#define BUTTON_LONG_CELLS 5U  /* Yes */

static MEM_XDATA char value_text[VALUE_TEXT_SIZE];
static MEM_XDATA uint8_t value_length;
static MEM_XDATA uint16_t item_value;
/* The text row that the draw helpers use. */
static MEM_XDATA uint8_t render_row;
static MEM_XDATA uint8_t render_index;
static MEM_XDATA int16_t band_x;
static MEM_XDATA int16_t band_width;

static int16_t cell_x(uint8_t column) { return (int16_t)(GRID_X + (uint8_t)(column * GFX_FONT_5X8_WIDTH)); }

static int16_t row_top(void) { return (int16_t)(render_row * GFX_FONT_5X8_HEIGHT); }

static void draw_text(uint8_t column, const char* MEM_XDATA text, MEM_XDATA uint8_t color) {
  (void)gfx_draw_text(cell_x(column), row_top(), text, color);
}

/* Highlight band of @p width pixels from @p x on rows 8r+1 .. 8r+7; the gap line 8r stays off. */
static void fill_band(int16_t x, MEM_XDATA int16_t width) {
  band_x = x;
  band_width = width;
  gfx_fill_rect(x, (int16_t)(row_top() + 1), width, BAND_HEIGHT, LCD_PIXEL_ON);
}

/*
 * Inverted text paints its cell background on the gap line; this clears the gap line over the last band again.
 * Normal text outside the band keeps its ink on the gap line.
 */
static void finish_band(void) { gfx_fill_rect(band_x, row_top(), band_width, 1, LCD_PIXEL_OFF); }

/* Starts at column floor((25 - length) / 2) (ui-ux.md section 5.7). */
static void draw_centered(const char* text) {
  const uint8_t length = (uint8_t)(gfx_text_width(text) / GFX_FONT_5X8_WIDTH);
  draw_text((uint8_t)((GRID_COLUMNS - length) / 2U), text, GFX_TEXT_NORMAL);
}

static void draw_title(const char* text) {
  render_row = TITLE_ROW;
  gfx_fill_rect(0, 0, (int16_t)LCD_WIDTH, (int16_t)GFX_FONT_5X8_HEIGHT, LCD_PIXEL_ON);
  draw_text(0U, text, GFX_TEXT_INVERTED);
}

/* Button in BUTTON_ROW over @p cells cells from @p first_cell; the focused one has a band and inverted text. */
static void draw_button(uint8_t first_cell, MEM_XDATA uint8_t cells, const char* MEM_XDATA text,
                        MEM_XDATA bool is_focused) {
  render_row = BUTTON_ROW;
  if (is_focused) {
    fill_band(cell_x(first_cell), (int16_t)(cells * GFX_FONT_5X8_WIDTH));
    draw_text((uint8_t)(first_cell + 1U), text, GFX_TEXT_INVERTED);
    finish_band();
  } else {
    draw_text((uint8_t)(first_cell + 1U), text, GFX_TEXT_NORMAL);
  }
}

static void append_text(const char MEM_CODE* text) {
  while ((*text != '\0') && (value_length < (VALUE_TEXT_SIZE - 1U))) {
    value_text[value_length] = *text;
    ++value_length;
    ++text;
  }
  value_text[value_length] = '\0';
}

/* Writes the value text of value or choice @p item into value_text (ui-ux.md section 5.3). */
static void format_value(const menu_item_t MEM_CODE* item) {
  value_length = 0U;
  if ((item->kind == MENU_KIND_CHOICE) || ((item->texts != NULL) && (item_value == 0U))) {
    append_text(item->texts[item_value]);
    return;
  }
  value_length = gfx_format_u16(item_value, value_text);
  if (item->unit != NULL) {
    append_text(" ");
    append_text(item->unit);
  }
}

/* Value field: value right-aligned to column 22; in edit mode a band from x 106 - 5 fw and the limit markers. */
static void draw_value(const menu_item_t MEM_CODE* item, MEM_XDATA bool is_edit) {
  const settings_id_t id = item->target;
  const uint8_t marker_column = (uint8_t)(EDIT_MARKER_COLUMN - item->field_width);
  uint8_t color = (render_index == menu_stack_cursors[menu_depth - 1U]) ? GFX_TEXT_INVERTED : GFX_TEXT_NORMAL;

  item_value = settings_get(id);
  format_value(item);
  if (is_edit) {
    fill_band((int16_t)(cell_x(marker_column) - 1), (int16_t)(LCD_WIDTH + 1U - (uint8_t)cell_x(marker_column)));
    if (item_value > settings_min(id)) {
      draw_text(marker_column, "<", GFX_TEXT_INVERTED);
    }
    if (item_value < settings_max(id)) {
      draw_text(GRID_LAST_COLUMN, ">", GFX_TEXT_INVERTED);
    }
    color = GFX_TEXT_INVERTED;
  }
  draw_text((uint8_t)(VALUE_END_COLUMN - value_length), value_text, color);
}

/*
 * ui-ux.md section 5.16.6: "L<n>", the earned level of mode @p mode at its own speed, right-aligned to column 22, in
 * the video of the label. A mode row has no edit mode, so the row selection gives the video.
 */
static void draw_mode_level(uint8_t mode) {
  item_value = koch_shown_level(progress_level(mode, (uint8_t)settings_get(MENU_MODE_SPEED_ID(mode))));
  value_text[0] = 'L';
  value_length = (uint8_t)(gfx_format_u16(item_value, &value_text[1]) + 1U);
  draw_text((uint8_t)(VALUE_END_COLUMN - value_length), value_text,
            (render_index == menu_stack_cursors[menu_depth - 1U]) ? GFX_TEXT_INVERTED : GFX_TEXT_NORMAL);
}

static void draw_item_row(const menu_item_t MEM_CODE* item) {
  const bool is_selected = (render_index == menu_stack_cursors[menu_depth - 1U]);
  const bool is_edit = is_selected && (menu_current_screen == MENU_SCREEN_EDIT);
  const uint8_t label_color = (is_selected && !is_edit) ? GFX_TEXT_INVERTED : GFX_TEXT_NORMAL;

  if (label_color == GFX_TEXT_INVERTED) {
    /* The row band covers x 0-127, also the margins outside the grid. */
    fill_band(0, (int16_t)LCD_WIDTH);
  }
  draw_text(0U, item->label, label_color);
  if (item->kind == MENU_KIND_SUBMENU) {
    draw_text(GRID_LAST_COLUMN, ">", label_color);
  } else if ((item->kind == MENU_KIND_VALUE) || (item->kind == MENU_KIND_CHOICE)) {
    draw_value(item, is_edit);
  } else if ((item->kind == MENU_KIND_MODE) && (item->target != MENU_MODE_FREE_INPUT)) {
    draw_mode_level(item->target);
  }
  if (is_selected) {
    finish_band();
  }
}

static void draw_list(void) {
  const menu_node_t MEM_CODE* node = &menu_nodes[menu_stack_nodes[menu_depth - 1U]];

  draw_title(node->title);
  for (render_index = 0U; render_index < node->item_count; ++render_index) {
    render_row = (uint8_t)(FIRST_ITEM_ROW + render_index);
    draw_item_row(&node->items[render_index]);
  }
}

static void draw_message(void) {
  const menu_message_t MEM_CODE* message = &menu_messages[menu_current_message];

  draw_title(message->title);
  render_row = MESSAGE_ROW;
  if (message->second_line != NULL) {
    render_row = (uint8_t)(MESSAGE_ROW - 1U);
    draw_centered(message->first_line);
    render_row = MESSAGE_ROW;
    draw_centered(message->second_line);
  } else {
    draw_centered(message->first_line);
  }
  draw_button(BUTTON_OK_FIRST_CELL, BUTTON_SHORT_CELLS, menu_text_ok, true);
}

void menu_render(void) {
  gfx_fill_rect(0, 0, (int16_t)LCD_WIDTH, (int16_t)LCD_HEIGHT, LCD_PIXEL_OFF);
  if ((menu_current_screen == MENU_SCREEN_LIST) || (menu_current_screen == MENU_SCREEN_EDIT)) {
    draw_list();
  } else if (menu_current_screen == MENU_SCREEN_MESSAGE) {
    draw_message();
  } else if (menu_current_screen == MENU_SCREEN_RECEIVING) {
    receiving_render();
  } else if (menu_current_screen == MENU_SCREEN_SENDING) {
    sending_render();
  } else {
    draw_title(menu_current_item()->label);
    render_row = MESSAGE_ROW;
    if (menu_current_screen == MENU_SCREEN_DIALOG) {
      draw_centered(menu_text_reset_question);
      draw_button(BUTTON_NO_FIRST_CELL, BUTTON_SHORT_CELLS, menu_text_no, menu_current_choice == MENU_DIALOG_NO);
      draw_button(BUTTON_YES_FIRST_CELL, BUTTON_LONG_CELLS, menu_text_yes, menu_current_choice == MENU_DIALOG_YES);
    }
  }
  menu_is_redraw_pending = false;
}
