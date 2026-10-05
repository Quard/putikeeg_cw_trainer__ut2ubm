;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module lcd
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _lcd_spi_write_repeat
	.globl _lcd_spi_write_code
	.globl _lcd_spi_write_xdata
	.globl _lcd_spi_write_byte
	.globl _lcd_spi_deselect
	.globl _lcd_spi_select
	.globl _lcd_spi_set_reset
	.globl _lcd_spi_init
	.globl _lcd_fb_clear_dirty
	.globl _lcd_fb_dirty_mask
	.globl _lcd_fb_mark_all_dirty
	.globl _lcd_fb_page
	.globl _delay_ms
	.globl _lcd_write_PARM_4
	.globl _lcd_write_PARM_3
	.globl _lcd_write_PARM_2
	.globl _lcd_init
	.globl _lcd_set_display_on
	.globl _lcd_set_contrast
	.globl _lcd_set_inverted
	.globl _lcd_set_all_pixels_on
	.globl _lcd_set_orientation
	.globl _lcd_sleep
	.globl _lcd_wake
	.globl _lcd_write
	.globl _lcd_flush_step
	.globl _lcd_has_pending_flush
	.globl _lcd_flush
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
;--------------------------------------------------------
; special function bits
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
;--------------------------------------------------------
; overlayable register banks
;--------------------------------------------------------
	.area REG_BANK_0	(REL,OVR,DATA)
	.ds 8
;--------------------------------------------------------
; internal ram data
;--------------------------------------------------------
	.area DSEG    (DATA)
_lcd_contrast:
	.ds 1
_lcd_orientation:
	.ds 1
_lcd_is_inverted:
	.ds 1
_lcd_is_all_on:
	.ds 1
_lcd_flush_page:
	.ds 1
_lcd_flush_column:
	.ds 1
_lcd_last_flushed_page:
	.ds 1
_lcd_select_ram_PARM_2:
	.ds 1
_lcd_write_PARM_2:
	.ds 1
_lcd_write_PARM_3:
	.ds 2
_lcd_write_PARM_4:
	.ds 1
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
;--------------------------------------------------------
; indirectly addressable internal ram data
;--------------------------------------------------------
	.area ISEG    (DATA)
;--------------------------------------------------------
; absolute internal ram data
;--------------------------------------------------------
	.area IABS    (ABS,DATA)
	.area IABS    (ABS,DATA)
;--------------------------------------------------------
; bit data
;--------------------------------------------------------
	.area BSEG    (BIT)
;--------------------------------------------------------
; paged external ram data
;--------------------------------------------------------
	.area PSEG    (PAG,XDATA)
;--------------------------------------------------------
; uninitialized external ram data
;--------------------------------------------------------
	.area XSEG    (XDATA)
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area XABS    (ABS,XDATA)
;--------------------------------------------------------
; initialized external ram data
;--------------------------------------------------------
	.area XISEG   (XDATA)
	.area HOME    (CODE)
	.area GSINIT0 (CODE)
	.area GSINIT1 (CODE)
	.area GSINIT2 (CODE)
	.area GSINIT3 (CODE)
	.area GSINIT4 (CODE)
	.area GSINIT5 (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area CSEG    (CODE)
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area HOME    (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area GSINIT  (CODE)
;	firmware/src/lcd/lcd.c:28: static uint8_t lcd_contrast = LCD_DEFAULT_CONTRAST;
	mov	_lcd_contrast,#0x20
;	firmware/src/lcd/lcd.c:29: static uint8_t lcd_orientation = LCD_ORIENTATION_NORMAL;
	mov	_lcd_orientation,#0x00
;	firmware/src/lcd/lcd.c:34: static uint8_t lcd_flush_page = LCD_NO_PAGE;
	mov	_lcd_flush_page,#0xff
;	firmware/src/lcd/lcd.c:37: static uint8_t lcd_last_flushed_page = LCD_PAGE_COUNT - 1U;
	mov	_lcd_last_flushed_page,#0x07
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area HOME    (CODE)
	.area HOME    (CODE)
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area CSEG    (CODE)
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_column_offset'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:39: static uint8_t lcd_column_offset(void) {
;	-----------------------------------------
;	 function lcd_column_offset
;	-----------------------------------------
_lcd_column_offset:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	firmware/src/lcd/lcd.c:40: return (lcd_orientation == LCD_ORIENTATION_ROTATED) ? LCD_COLUMN_OFFSET_ROTATED : LCD_COLUMN_OFFSET_NORMAL;
	mov	a,#0x01
	cjne	a,_lcd_orientation,00103$
	mov	r7,#0x04
	sjmp	00104$
00103$:
	mov	r7,#0x00
00104$:
	mov	dpl,r7
;	firmware/src/lcd/lcd.c:41: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_select_ram'
;------------------------------------------------------------
;column        Allocated with name '_lcd_select_ram_PARM_2'
;page          Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:44: static void lcd_select_ram(uint8_t page, uint8_t column) {
;	-----------------------------------------
;	 function lcd_select_ram
;	-----------------------------------------
_lcd_select_ram:
	mov	r7, dpl
;	firmware/src/lcd/lcd.c:45: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	push	ar7
	lcall	_lcd_spi_select
	pop	ar7
;	firmware/src/lcd/lcd.c:46: lcd_spi_write_byte((uint8_t)(LCD_CMD_PAGE_ADDRESS | page));
	mov	a,#0xb0
	orl	a,r7
	mov	dpl,a
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:47: lcd_spi_write_byte((uint8_t)(LCD_CMD_COLUMN_LOW | (column & 0x0FU)));
	mov	r7,_lcd_select_ram_PARM_2
	mov	a,#0x0f
	anl	a,r7
	mov	dpl,a
	push	ar7
	lcall	_lcd_spi_write_byte
	pop	ar7
;	firmware/src/lcd/lcd.c:48: lcd_spi_write_byte((uint8_t)(LCD_CMD_COLUMN_HIGH | (column >> 4)));
	mov	a,r7
	swap	a
	anl	a,#0x0f
	mov	r7,a
	mov	a,#0x10
	orl	a,r7
	mov	dpl,a
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:49: lcd_spi_select(LCD_SPI_DATA);
	mov	dpl, #0x01
;	firmware/src/lcd/lcd.c:50: }
	ljmp	_lcd_spi_select
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_send_command'
;------------------------------------------------------------
;command       Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:52: static void lcd_send_command(uint8_t command) {
;	-----------------------------------------
;	 function lcd_send_command
;	-----------------------------------------
_lcd_send_command:
	mov	r7, dpl
;	firmware/src/lcd/lcd.c:53: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	push	ar7
	lcall	_lcd_spi_select
	pop	ar7
;	firmware/src/lcd/lcd.c:54: lcd_spi_write_byte(command);
	mov	dpl, r7
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:55: lcd_spi_deselect();
;	firmware/src/lcd/lcd.c:56: }
	ljmp	_lcd_spi_deselect
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_send_orientation_commands'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:58: static void lcd_send_orientation_commands(void) { lcd_spi_write_code(lcd_orientation_commands[lcd_orientation], 2U); }
;	-----------------------------------------
;	 function lcd_send_orientation_commands
;	-----------------------------------------
_lcd_send_orientation_commands:
	mov	a,_lcd_orientation
	mov	b,#0x02
	mul	ab
	add	a, #_lcd_orientation_commands
	mov	dpl,a
	mov	a,#(_lcd_orientation_commands >> 8)
	addc	a, b
	mov	dph,a
	mov	_lcd_spi_write_code_PARM_2,#0x02
	ljmp	_lcd_spi_write_code
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_all_pixels_command'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:60: static uint8_t lcd_all_pixels_command(void) {
;	-----------------------------------------
;	 function lcd_all_pixels_command
;	-----------------------------------------
_lcd_all_pixels_command:
;	firmware/src/lcd/lcd.c:61: return lcd_is_all_on ? LCD_CMD_ALL_PIXELS_ON : LCD_CMD_ALL_PIXELS_NORMAL;
	mov	a,_lcd_is_all_on
	jz	00103$
	mov	r7,#0xa5
	sjmp	00104$
00103$:
	mov	r7,#0xa4
00104$:
	mov	dpl,r7
;	firmware/src/lcd/lcd.c:62: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_clear_ram'
;------------------------------------------------------------
;page          Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:64: static void lcd_clear_ram(void) {
;	-----------------------------------------
;	 function lcd_clear_ram
;	-----------------------------------------
_lcd_clear_ram:
;	firmware/src/lcd/lcd.c:66: for (page = 0U; page < LCD_RAM_PAGE_COUNT; ++page) {
	mov	r7,#0x00
00102$:
;	firmware/src/lcd/lcd.c:67: lcd_select_ram(page, 0U);
	mov	_lcd_select_ram_PARM_2,#0x00
	mov	dpl, r7
	push	ar7
	lcall	_lcd_select_ram
	pop	ar7
;	firmware/src/lcd/lcd.c:68: lcd_spi_write_repeat(0U, LCD_RAM_COLUMN_COUNT);
	mov	_lcd_spi_write_repeat_PARM_2,#0x84
	mov	dpl, #0x00
	push	ar7
	lcall	_lcd_spi_write_repeat
	pop	ar7
;	firmware/src/lcd/lcd.c:66: for (page = 0U; page < LCD_RAM_PAGE_COUNT; ++page) {
	inc	r7
	cjne	r7,#0x09,00114$
00114$:
	jc	00102$
;	firmware/src/lcd/lcd.c:70: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_init'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:72: void lcd_init(void) {
;	-----------------------------------------
;	 function lcd_init
;	-----------------------------------------
_lcd_init:
;	firmware/src/lcd/lcd.c:73: lcd_spi_init();
	lcall	_lcd_spi_init
;	firmware/src/lcd/lcd.c:74: delay_ms(LCD_RESET_PULSE_MS);
	mov	dpl, #0x01
	lcall	_delay_ms
;	firmware/src/lcd/lcd.c:75: lcd_spi_set_reset(false);
	mov	dpl, #0x00
	lcall	_lcd_spi_set_reset
;	firmware/src/lcd/lcd.c:76: delay_ms(LCD_RESET_WAIT_MS);
	mov	dpl, #0x01
	lcall	_delay_ms
;	firmware/src/lcd/lcd.c:78: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	lcall	_lcd_spi_select
;	firmware/src/lcd/lcd.c:79: lcd_spi_write_code(&lcd_init_commands[LCD_INIT_BIAS_INDEX], 1U);
	mov	_lcd_spi_write_code_PARM_2,#0x01
	mov	dptr,#_lcd_init_commands
	lcall	_lcd_spi_write_code
;	firmware/src/lcd/lcd.c:80: lcd_send_orientation_commands();
	lcall	_lcd_send_orientation_commands
;	firmware/src/lcd/lcd.c:81: lcd_spi_write_code(&lcd_init_commands[LCD_INIT_MIDDLE_INDEX], LCD_INIT_MIDDLE_LENGTH);
	mov	_lcd_spi_write_code_PARM_2,#0x03
	mov	dptr,#(_lcd_init_commands + 0x0001)
	lcall	_lcd_spi_write_code
;	firmware/src/lcd/lcd.c:82: lcd_spi_write_byte(lcd_contrast);
	mov	dpl, _lcd_contrast
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:83: lcd_spi_write_byte(LCD_CMD_POWER_ALL_ON);
	mov	dpl, #0x2f
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:84: lcd_spi_deselect();
	lcall	_lcd_spi_deselect
;	firmware/src/lcd/lcd.c:86: delay_ms(LCD_POWER_SETTLE_MS);
	mov	dpl, #0x0a
	lcall	_delay_ms
;	firmware/src/lcd/lcd.c:88: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	lcall	_lcd_spi_select
;	firmware/src/lcd/lcd.c:89: lcd_spi_write_byte(lcd_is_inverted ? LCD_CMD_INVERSE_ON : LCD_CMD_INVERSE_OFF);
	mov	a,_lcd_is_inverted
	jz	00103$
	mov	r7,#0xa7
	sjmp	00104$
00103$:
	mov	r7,#0xa6
00104$:
	mov	dpl,r7
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:90: lcd_spi_write_byte(lcd_all_pixels_command());
	lcall	_lcd_all_pixels_command
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:92: lcd_clear_ram();
	lcall	_lcd_clear_ram
;	firmware/src/lcd/lcd.c:93: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	lcall	_lcd_spi_select
;	firmware/src/lcd/lcd.c:94: lcd_spi_write_byte(LCD_CMD_DISPLAY_ON);
	mov	dpl, #0xaf
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:95: lcd_spi_deselect();
	lcall	_lcd_spi_deselect
;	firmware/src/lcd/lcd.c:97: lcd_flush_page = LCD_NO_PAGE;
	mov	_lcd_flush_page,#0xff
;	firmware/src/lcd/lcd.c:98: lcd_last_flushed_page = LCD_PAGE_COUNT - 1U;
	mov	_lcd_last_flushed_page,#0x07
;	firmware/src/lcd/lcd.c:99: lcd_fb_mark_all_dirty();
;	firmware/src/lcd/lcd.c:100: }
	ljmp	_lcd_fb_mark_all_dirty
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_set_display_on'
;------------------------------------------------------------
;is_on         Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:102: void lcd_set_display_on(bool is_on) { lcd_send_command(is_on ? LCD_CMD_DISPLAY_ON : LCD_CMD_DISPLAY_OFF); }
;	-----------------------------------------
;	 function lcd_set_display_on
;	-----------------------------------------
_lcd_set_display_on:
	mov	a,dpl
	jz	00103$
	mov	r7,#0xaf
	sjmp	00104$
00103$:
	mov	r7,#0xae
00104$:
	mov	dpl,r7
	ljmp	_lcd_send_command
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_set_contrast'
;------------------------------------------------------------
;level         Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:104: void lcd_set_contrast(uint8_t level) {
;	-----------------------------------------
;	 function lcd_set_contrast
;	-----------------------------------------
_lcd_set_contrast:
;	firmware/src/lcd/lcd.c:105: if (level > LCD_CONTRAST_MAX) {
	mov	a,dpl
	mov	r7,a
	add	a,#0xff - 0x3f
	jnc	00102$
;	free result
;	firmware/src/lcd/lcd.c:106: level = LCD_CONTRAST_MAX;
	mov	r7,#0x3f
00102$:
;	firmware/src/lcd/lcd.c:108: lcd_contrast = level;
	mov	_lcd_contrast,r7
;	firmware/src/lcd/lcd.c:109: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	push	ar7
	lcall	_lcd_spi_select
;	firmware/src/lcd/lcd.c:110: lcd_spi_write_byte(LCD_CMD_SET_CONTRAST);
	mov	dpl, #0x81
	lcall	_lcd_spi_write_byte
	pop	ar7
;	firmware/src/lcd/lcd.c:111: lcd_spi_write_byte(level);
	mov	dpl, r7
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:112: lcd_spi_deselect();
;	firmware/src/lcd/lcd.c:113: }
	ljmp	_lcd_spi_deselect
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_set_inverted'
;------------------------------------------------------------
;is_inverted   Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:115: void lcd_set_inverted(bool is_inverted) {
;	-----------------------------------------
;	 function lcd_set_inverted
;	-----------------------------------------
_lcd_set_inverted:
;	firmware/src/lcd/lcd.c:116: lcd_is_inverted = is_inverted;
;	firmware/src/lcd/lcd.c:117: lcd_send_command(is_inverted ? LCD_CMD_INVERSE_ON : LCD_CMD_INVERSE_OFF);
	mov	a,dpl
	mov	_lcd_is_inverted,a
	jz	00103$
	mov	r7,#0xa7
	sjmp	00104$
00103$:
	mov	r7,#0xa6
00104$:
	mov	dpl,r7
;	firmware/src/lcd/lcd.c:118: }
	ljmp	_lcd_send_command
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_set_all_pixels_on'
;------------------------------------------------------------
;is_all_on     Allocated to registers 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:120: void lcd_set_all_pixels_on(bool is_all_on) {
;	-----------------------------------------
;	 function lcd_set_all_pixels_on
;	-----------------------------------------
_lcd_set_all_pixels_on:
	mov	_lcd_is_all_on,dpl
;	firmware/src/lcd/lcd.c:122: lcd_send_command(lcd_all_pixels_command());
	lcall	_lcd_all_pixels_command
;	firmware/src/lcd/lcd.c:123: }
	ljmp	_lcd_send_command
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_set_orientation'
;------------------------------------------------------------
;orientation   Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:125: void lcd_set_orientation(uint8_t orientation) {
;	-----------------------------------------
;	 function lcd_set_orientation
;	-----------------------------------------
_lcd_set_orientation:
;	firmware/src/lcd/lcd.c:126: if (orientation > LCD_ORIENTATION_ROTATED) {
	mov	a,dpl
	mov	r7,a
	add	a,#0xff - 0x01
	jnc	00102$
;	free result
;	firmware/src/lcd/lcd.c:127: return;
	ret
00102$:
;	firmware/src/lcd/lcd.c:129: lcd_orientation = orientation;
	mov	_lcd_orientation,r7
;	firmware/src/lcd/lcd.c:130: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	lcall	_lcd_spi_select
;	firmware/src/lcd/lcd.c:131: lcd_send_orientation_commands();
	lcall	_lcd_send_orientation_commands
;	firmware/src/lcd/lcd.c:132: lcd_spi_deselect();
	lcall	_lcd_spi_deselect
;	firmware/src/lcd/lcd.c:134: lcd_fb_mark_all_dirty();
;	firmware/src/lcd/lcd.c:135: }
	ljmp	_lcd_fb_mark_all_dirty
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_sleep'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:137: void lcd_sleep(void) {
;	-----------------------------------------
;	 function lcd_sleep
;	-----------------------------------------
_lcd_sleep:
;	firmware/src/lcd/lcd.c:139: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	lcall	_lcd_spi_select
;	firmware/src/lcd/lcd.c:140: lcd_spi_write_byte(LCD_CMD_DISPLAY_OFF);
	mov	dpl, #0xae
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:141: lcd_spi_write_byte(LCD_CMD_ALL_PIXELS_ON);
	mov	dpl, #0xa5
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:142: lcd_spi_deselect();
;	firmware/src/lcd/lcd.c:143: }
	ljmp	_lcd_spi_deselect
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_wake'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:145: void lcd_wake(void) {
;	-----------------------------------------
;	 function lcd_wake
;	-----------------------------------------
_lcd_wake:
;	firmware/src/lcd/lcd.c:146: lcd_spi_select(LCD_SPI_COMMAND);
	mov	dpl, #0x00
	lcall	_lcd_spi_select
;	firmware/src/lcd/lcd.c:147: lcd_spi_write_byte(lcd_all_pixels_command());
	lcall	_lcd_all_pixels_command
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:148: lcd_spi_write_byte(LCD_CMD_DISPLAY_ON);
	mov	dpl, #0xaf
	lcall	_lcd_spi_write_byte
;	firmware/src/lcd/lcd.c:149: lcd_spi_deselect();
;	firmware/src/lcd/lcd.c:150: }
	ljmp	_lcd_spi_deselect
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_write'
;------------------------------------------------------------
;column        Allocated with name '_lcd_write_PARM_2'
;data          Allocated with name '_lcd_write_PARM_3'
;length        Allocated with name '_lcd_write_PARM_4'
;page          Allocated to registers r7 
;room          Allocated to registers r6 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:152: void lcd_write(uint8_t page, uint8_t column, const MEM_XDATA uint8_t* data, uint8_t length) {
;	-----------------------------------------
;	 function lcd_write
;	-----------------------------------------
_lcd_write:
	mov	r7, dpl
;	firmware/src/lcd/lcd.c:155: if ((page >= LCD_PAGE_COUNT) || (column >= LCD_WIDTH)) {
	cjne	r7,#0x08,00122$
00122$:
	jnc	00101$
	mov	a,#0x100 - 0x80
	add	a,_lcd_write_PARM_2
	jnc	00102$
00101$:
;	firmware/src/lcd/lcd.c:156: return;
	ret
00102$:
;	firmware/src/lcd/lcd.c:158: room = (uint8_t)(LCD_WIDTH - column);
	mov	a,#0x80
	clr	c
	subb	a,_lcd_write_PARM_2
;	firmware/src/lcd/lcd.c:159: if (length > room) {
	mov	r6,a
	clr	c
	subb	a,_lcd_write_PARM_4
	jnc	00105$
;	free result
;	firmware/src/lcd/lcd.c:160: length = room;
	mov	_lcd_write_PARM_4,r6
00105$:
;	firmware/src/lcd/lcd.c:162: lcd_select_ram(page, (uint8_t)(column + lcd_column_offset()));
	push	ar7
	lcall	_lcd_column_offset
	mov	r6, dpl
	pop	ar7
	mov	a,r6
	add	a, _lcd_write_PARM_2
	mov	_lcd_select_ram_PARM_2,a
	mov	dpl, r7
	lcall	_lcd_select_ram
;	firmware/src/lcd/lcd.c:163: lcd_spi_write_xdata(data, length);
	mov	_lcd_spi_write_xdata_PARM_2,_lcd_write_PARM_4
	mov	dpl, _lcd_write_PARM_3
	mov	dph, (_lcd_write_PARM_3 + 1)
	lcall	_lcd_spi_write_xdata
;	firmware/src/lcd/lcd.c:164: lcd_spi_deselect();
;	firmware/src/lcd/lcd.c:165: }
	ljmp	_lcd_spi_deselect
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_start_next_page'
;------------------------------------------------------------
;dirty_pages   Allocated to registers r7 
;page          Allocated to registers r6 
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:167: static void lcd_start_next_page(void) {
;	-----------------------------------------
;	 function lcd_start_next_page
;	-----------------------------------------
_lcd_start_next_page:
;	firmware/src/lcd/lcd.c:168: uint8_t dirty_pages = lcd_fb_dirty_mask();
	lcall	_lcd_fb_dirty_mask
	mov	r7, dpl
;	firmware/src/lcd/lcd.c:169: uint8_t page = lcd_last_flushed_page;
	mov	r6,_lcd_last_flushed_page
;	firmware/src/lcd/lcd.c:171: if (dirty_pages == 0U) {
	mov	a,r7
	jnz	00103$
;	firmware/src/lcd/lcd.c:172: return;
;	firmware/src/lcd/lcd.c:174: do {
	ret
00103$:
;	firmware/src/lcd/lcd.c:175: page = (uint8_t)((page + 1U) & (LCD_PAGE_COUNT - 1U));
	mov	a,r6
	inc	a
	anl	a,#0x07
	mov	r6,a
;	firmware/src/lcd/lcd.c:176: } while ((dirty_pages & (uint8_t)(1U << page)) == 0U);
	mov	b,r6
	inc	b
	mov	a,#0x01
	sjmp	00131$
00130$:
	add	a,acc
00131$:
	djnz	b,00130$
	anl	a,r7
	jz	00103$
;	firmware/src/lcd/lcd.c:177: lcd_flush_page = page;
	mov	_lcd_flush_page,r6
;	firmware/src/lcd/lcd.c:178: lcd_flush_column = 0U;
	mov	_lcd_flush_column,#0x00
;	firmware/src/lcd/lcd.c:179: lcd_last_flushed_page = page;
	mov	_lcd_last_flushed_page,r6
;	firmware/src/lcd/lcd.c:181: lcd_fb_clear_dirty(page);
	mov	dpl, r6
;	firmware/src/lcd/lcd.c:182: }
	ljmp	_lcd_fb_clear_dirty
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_flush_step'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:184: void lcd_flush_step(void) {
;	-----------------------------------------
;	 function lcd_flush_step
;	-----------------------------------------
_lcd_flush_step:
;	firmware/src/lcd/lcd.c:185: if (lcd_flush_page == LCD_NO_PAGE) {
	mov	a,#0xff
	cjne	a,_lcd_flush_page,00104$
;	firmware/src/lcd/lcd.c:186: lcd_start_next_page();
	lcall	_lcd_start_next_page
;	firmware/src/lcd/lcd.c:187: if (lcd_flush_page == LCD_NO_PAGE) {
	mov	a,#0xff
	cjne	a,_lcd_flush_page,00104$
;	firmware/src/lcd/lcd.c:188: return;
	ret
00104$:
;	firmware/src/lcd/lcd.c:191: lcd_write(lcd_flush_page, lcd_flush_column, &lcd_fb_page(lcd_flush_page)[lcd_flush_column], LCD_FLUSH_CHUNK_COLUMNS);
	mov	dpl, _lcd_flush_page
	lcall	_lcd_fb_page
	mov	a, dpl
	mov	b, dph
	add	a, _lcd_flush_column
	mov	_lcd_write_PARM_3,a
	clr	a
	addc	a, b
	mov	(_lcd_write_PARM_3 + 1),a
	mov	_lcd_write_PARM_2,_lcd_flush_column
	mov	_lcd_write_PARM_4,#0x80
	mov	dpl, _lcd_flush_page
	lcall	_lcd_write
;	firmware/src/lcd/lcd.c:192: lcd_flush_column = (uint8_t)(lcd_flush_column + LCD_FLUSH_CHUNK_COLUMNS);
	mov	a,#0x80
	add	a, _lcd_flush_column
;	firmware/src/lcd/lcd.c:193: if (lcd_flush_column >= LCD_WIDTH) {
	mov	_lcd_flush_column,a
	clr	c
	subb	a,#0x80
	jc	00107$
;	firmware/src/lcd/lcd.c:194: lcd_flush_page = LCD_NO_PAGE;
	mov	_lcd_flush_page,#0xff
00107$:
;	firmware/src/lcd/lcd.c:196: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_has_pending_flush'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:198: bool lcd_has_pending_flush(void) {
;	-----------------------------------------
;	 function lcd_has_pending_flush
;	-----------------------------------------
_lcd_has_pending_flush:
;	firmware/src/lcd/lcd.c:199: if (lcd_flush_page != LCD_NO_PAGE) {
	mov	a,#0xff
	cjne	a,_lcd_flush_page,00112$
	sjmp	00102$
00112$:
;	firmware/src/lcd/lcd.c:200: return true;
	mov	dpl, #0x01
	ret
00102$:
;	firmware/src/lcd/lcd.c:202: return (bool)lcd_fb_dirty_mask();
	lcall	_lcd_fb_dirty_mask
;	assignBit
	mov	a,dpl
	add	a,#0xff
	clr	a
	rlc	a
	mov	dpl,a
;	firmware/src/lcd/lcd.c:203: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_flush'
;------------------------------------------------------------
;	firmware/src/lcd/lcd.c:205: void lcd_flush(void) {
;	-----------------------------------------
;	 function lcd_flush
;	-----------------------------------------
_lcd_flush:
;	firmware/src/lcd/lcd.c:206: while (lcd_has_pending_flush()) {
00101$:
	lcall	_lcd_has_pending_flush
	mov	a, dpl
	jz	00104$
;	firmware/src/lcd/lcd.c:207: lcd_flush_step();
	lcall	_lcd_flush_step
	sjmp	00101$
00104$:
;	firmware/src/lcd/lcd.c:209: }
	ret
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area CONST   (CODE)
_lcd_init_commands:
	.db #0xa2	; 162
	.db #0x40	; 64
	.db #0x25	; 37
	.db #0x81	; 129
	.area CSEG    (CODE)
	.area CONST   (CODE)
_lcd_orientation_commands:
	.db #0xa0	; 160
	.db #0xc8	; 200
	.db #0xa1	; 161
	.db #0xc0	; 192
	.area CSEG    (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
