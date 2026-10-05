;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module lcd_fb
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _lcd_fb_get_pixel_PARM_2
	.globl _lcd_fb_set_pixel_PARM_3
	.globl _lcd_fb_set_pixel_PARM_2
	.globl _lcd_fb_clear
	.globl _lcd_fb_set_pixel
	.globl _lcd_fb_get_pixel
	.globl _lcd_fb_page
	.globl _lcd_fb_mark_dirty
	.globl _lcd_fb_mark_all_dirty
	.globl _lcd_fb_dirty_mask
	.globl _lcd_fb_clear_dirty
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
_lcd_fb_dirty_pages:
	.ds 1
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
	.area	OSEG    (OVR,DATA)
	.area	OSEG    (OVR,DATA)
_lcd_fb_set_pixel_PARM_2:
	.ds 2
_lcd_fb_set_pixel_PARM_3:
	.ds 1
	.area	OSEG    (OVR,DATA)
_lcd_fb_get_pixel_PARM_2:
	.ds 2
	.area	OSEG    (OVR,DATA)
	.area	OSEG    (OVR,DATA)
	.area	OSEG    (OVR,DATA)
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
_lcd_fb_pixels:
	.ds 1024
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
;Allocation info for local variables in function 'lcd_fb_clear'
;------------------------------------------------------------
;cell          Allocated to registers 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:21: void lcd_fb_clear(void) {
;	-----------------------------------------
;	 function lcd_fb_clear
;	-----------------------------------------
_lcd_fb_clear:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	firmware/src/lcd/lcd_fb.c:22: MEM_XDATA uint8_t* cell = lcd_fb_pixels;
	mov	r6,#_lcd_fb_pixels
	mov	r7,#(_lcd_fb_pixels >> 8)
;	firmware/src/lcd/lcd_fb.c:23: do {
00101$:
;	firmware/src/lcd/lcd_fb.c:24: *cell = 0U;
	mov	dpl,r6
	mov	dph,r7
	clr	a
	movx	@dptr,a
	inc	dptr
	mov	r6,dpl
	mov	r7,dph
;	firmware/src/lcd/lcd_fb.c:25: ++cell;
;	firmware/src/lcd/lcd_fb.c:26: } while (cell != &lcd_fb_pixels[LCD_FB_SIZE]);
	cjne	r6,#(_lcd_fb_pixels + 0x0400),00101$
	cjne	r7,#((_lcd_fb_pixels + 0x0400) >> 8),00101$
;	firmware/src/lcd/lcd_fb.c:27: lcd_fb_dirty_pages = LCD_FB_ALL_PAGES;
	mov	_lcd_fb_dirty_pages,#0xff
;	firmware/src/lcd/lcd_fb.c:28: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_fb_set_pixel'
;------------------------------------------------------------
;y             Allocated with name '_lcd_fb_set_pixel_PARM_2'
;color         Allocated with name '_lcd_fb_set_pixel_PARM_3'
;x             Allocated to registers r6 r7 
;cell          Allocated to registers r6 r7 
;mask          Allocated to registers r5 
;old_value     Allocated to registers r3 
;new_value     Allocated to registers r2 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:30: void lcd_fb_set_pixel(int16_t x, int16_t y, uint8_t color) {
;	-----------------------------------------
;	 function lcd_fb_set_pixel
;	-----------------------------------------
_lcd_fb_set_pixel:
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/lcd/lcd_fb.c:36: if (!LCD_FB_IS_ON_SCREEN(x, y)) {
	mov	ar4,r6
	mov	ar5,r7
	clr	c
	mov	a,r4
	subb	a,#0x80
	mov	a,r5
	subb	a,#0x00
	jnc	00101$
	mov	r4,_lcd_fb_set_pixel_PARM_2
	mov	r5,(_lcd_fb_set_pixel_PARM_2 + 1)
	clr	c
	mov	a,r4
	subb	a,#0x40
	mov	a,r5
	subb	a,#0x00
	jc	00102$
00101$:
;	firmware/src/lcd/lcd_fb.c:37: return;
	ret
00102$:
;	firmware/src/lcd/lcd_fb.c:39: cell = LCD_FB_CELL(x, y);
	mov	a,_lcd_fb_set_pixel_PARM_2
	mov	r5,a
	swap	a
	rl	a
	anl	a,#0x1f
	mov	r4,a
	mov	r2,a
	clr	a
	clr	c
	xch	a,r2
	rrc	a
	xch	a,r2
	rrc	a
	xch	a,r2
	mov	r3,a
	mov	r7,#0x00
	mov	a,r6
	add	a, r2
	mov	r2,a
	mov	a,r7
	addc	a, r3
	mov	r3,a
	mov	a,r2
	add	a, #_lcd_fb_pixels
	mov	r6,a
	mov	a,r3
	addc	a, #(_lcd_fb_pixels >> 8)
	mov	r7,a
;	firmware/src/lcd/lcd_fb.c:40: mask = lcd_fb_bit_masks[(uint8_t)y & 7U];
	mov	a,#0x07
	anl	a,r5
	mov	dptr,#_lcd_fb_bit_masks
	movc	a,@a+dptr
	mov	r5,a
;	firmware/src/lcd/lcd_fb.c:41: old_value = *cell;
	mov	dpl,r6
	mov	dph,r7
	movx	a,@dptr
	mov	r3,a
;	firmware/src/lcd/lcd_fb.c:42: if (color == LCD_PIXEL_ON) {
	mov	a,#0x01
	cjne	a,_lcd_fb_set_pixel_PARM_3,00111$
;	firmware/src/lcd/lcd_fb.c:43: new_value = (uint8_t)(old_value | mask);
	mov	a,r5
	orl	a,r3
	mov	r2,a
	sjmp	00112$
00111$:
;	firmware/src/lcd/lcd_fb.c:44: } else if (color == LCD_PIXEL_OFF) {
	mov	a,_lcd_fb_set_pixel_PARM_3
	jnz	00108$
;	firmware/src/lcd/lcd_fb.c:45: new_value = (uint8_t)(old_value & (uint8_t)~mask);
	mov	a,#0xff
	xrl	a,r5
	anl	a,r3
	mov	r2,a
	sjmp	00112$
00108$:
;	firmware/src/lcd/lcd_fb.c:46: } else if (color == LCD_PIXEL_INVERT) {
	mov	a,#0x02
	cjne	a,_lcd_fb_set_pixel_PARM_3,00105$
;	firmware/src/lcd/lcd_fb.c:47: new_value = (uint8_t)(old_value ^ mask);
	mov	a,r5
	xrl	a,r3
	mov	r2,a
	sjmp	00112$
00105$:
;	firmware/src/lcd/lcd_fb.c:49: return;
	ret
00112$:
;	firmware/src/lcd/lcd_fb.c:51: if (new_value != old_value) {
	mov	a,r2
	cjne	a,ar3,00159$
	ret
00159$:
;	firmware/src/lcd/lcd_fb.c:52: *cell = new_value;
	mov	dpl,r6
	mov	dph,r7
	mov	a,r2
	movx	@dptr,a
;	firmware/src/lcd/lcd_fb.c:53: lcd_fb_dirty_pages |= lcd_fb_bit_masks[(uint8_t)y >> 3];
	mov	a,r4
	mov	dptr,#_lcd_fb_bit_masks
	movc	a,@a+dptr
	orl	_lcd_fb_dirty_pages,a
;	firmware/src/lcd/lcd_fb.c:55: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_fb_get_pixel'
;------------------------------------------------------------
;y             Allocated with name '_lcd_fb_get_pixel_PARM_2'
;x             Allocated to registers r6 r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:57: bool lcd_fb_get_pixel(int16_t x, int16_t y) {
;	-----------------------------------------
;	 function lcd_fb_get_pixel
;	-----------------------------------------
_lcd_fb_get_pixel:
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/lcd/lcd_fb.c:58: if (!LCD_FB_IS_ON_SCREEN(x, y)) {
	mov	ar4,r6
	mov	ar5,r7
	clr	c
	mov	a,r4
	subb	a,#0x80
	mov	a,r5
	subb	a,#0x00
	jnc	00101$
	mov	r4,_lcd_fb_get_pixel_PARM_2
	mov	r5,(_lcd_fb_get_pixel_PARM_2 + 1)
	clr	c
	mov	a,r4
	subb	a,#0x40
	mov	a,r5
	subb	a,#0x00
	jc	00102$
00101$:
;	firmware/src/lcd/lcd_fb.c:59: return false;
	mov	dpl, #0x00
	ret
00102$:
;	firmware/src/lcd/lcd_fb.c:61: return (bool)(*LCD_FB_CELL(x, y) & lcd_fb_bit_masks[(uint8_t)y & 7U]);
	mov	a,_lcd_fb_get_pixel_PARM_2
	mov	r5,a
	swap	a
	rl	a
	anl	a,#0x1f
	mov	r4,a
	clr	a
	clr	c
	xch	a,r4
	rrc	a
	xch	a,r4
	rrc	a
	xch	a,r4
	mov	r3,a
	mov	r7,#0x00
	mov	a,r6
	add	a, r4
	mov	r4,a
	mov	a,r7
	addc	a, r3
	mov	r3,a
	mov	a,r4
	add	a, #_lcd_fb_pixels
	mov	dpl,a
	mov	a,r3
	addc	a, #(_lcd_fb_pixels >> 8)
	mov	dph,a
	movx	a,@dptr
	mov	r7,a
	mov	a,#0x07
	anl	a,r5
	mov	dptr,#_lcd_fb_bit_masks
	movc	a,@a+dptr
	anl	ar7,a
;	assignBit
	mov	a,r7
	add	a,#0xff
	clr	a
	rlc	a
	mov	dpl,a
;	firmware/src/lcd/lcd_fb.c:62: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_fb_page'
;------------------------------------------------------------
;page          Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:64: MEM_XDATA uint8_t* lcd_fb_page(uint8_t page) {
;	-----------------------------------------
;	 function lcd_fb_page
;	-----------------------------------------
_lcd_fb_page:
	mov	r7, dpl
;	firmware/src/lcd/lcd_fb.c:65: if (page >= LCD_PAGE_COUNT) {
	cjne	r7,#0x08,00112$
00112$:
	jc	00102$
;	firmware/src/lcd/lcd_fb.c:66: return NULL;
	mov	dptr,#0x0000
	ret
00102$:
;	firmware/src/lcd/lcd_fb.c:68: return &lcd_fb_pixels[(uint16_t)((uint16_t)page * LCD_WIDTH)];
	clr	a
	clr	c
	xch	a,r7
	rrc	a
	xch	a,r7
	rrc	a
	xch	a,r7
	mov	r6,a
	mov	a,r7
	add	a, #_lcd_fb_pixels
	mov	dpl,a
	mov	a,r6
	addc	a, #(_lcd_fb_pixels >> 8)
	mov	dph,a
;	firmware/src/lcd/lcd_fb.c:69: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_fb_mark_dirty'
;------------------------------------------------------------
;page          Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:71: void lcd_fb_mark_dirty(uint8_t page) {
;	-----------------------------------------
;	 function lcd_fb_mark_dirty
;	-----------------------------------------
_lcd_fb_mark_dirty:
	mov	r7, dpl
;	firmware/src/lcd/lcd_fb.c:72: if (page < LCD_PAGE_COUNT) {
	cjne	r7,#0x08,00112$
00112$:
	jnc	00103$
;	firmware/src/lcd/lcd_fb.c:73: lcd_fb_dirty_pages |= lcd_fb_bit_masks[page];
	mov	a,r7
	mov	dptr,#_lcd_fb_bit_masks
	movc	a,@a+dptr
	orl	_lcd_fb_dirty_pages,a
00103$:
;	firmware/src/lcd/lcd_fb.c:75: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_fb_mark_all_dirty'
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:77: void lcd_fb_mark_all_dirty(void) { lcd_fb_dirty_pages = LCD_FB_ALL_PAGES; }
;	-----------------------------------------
;	 function lcd_fb_mark_all_dirty
;	-----------------------------------------
_lcd_fb_mark_all_dirty:
	mov	_lcd_fb_dirty_pages,#0xff
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_fb_dirty_mask'
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:79: uint8_t lcd_fb_dirty_mask(void) { return lcd_fb_dirty_pages; }
;	-----------------------------------------
;	 function lcd_fb_dirty_mask
;	-----------------------------------------
_lcd_fb_dirty_mask:
	mov	dpl, _lcd_fb_dirty_pages
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_fb_clear_dirty'
;------------------------------------------------------------
;page          Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_fb.c:81: void lcd_fb_clear_dirty(uint8_t page) {
;	-----------------------------------------
;	 function lcd_fb_clear_dirty
;	-----------------------------------------
_lcd_fb_clear_dirty:
	mov	r7, dpl
;	firmware/src/lcd/lcd_fb.c:82: if (page < LCD_PAGE_COUNT) {
	cjne	r7,#0x08,00112$
00112$:
	jnc	00103$
;	firmware/src/lcd/lcd_fb.c:83: lcd_fb_dirty_pages &= (uint8_t)~lcd_fb_bit_masks[page];
	mov	a,r7
	mov	dptr,#_lcd_fb_bit_masks
	movc	a,@a+dptr
	mov	r7,a
	mov	a,#0xff
	xrl	a,r7
	anl	_lcd_fb_dirty_pages,a
00103$:
;	firmware/src/lcd/lcd_fb.c:85: }
	ret
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area CONST   (CODE)
_lcd_fb_bit_masks:
	.db #0x01	; 1
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x08	; 8
	.db #0x10	; 16
	.db #0x20	; 32
	.db #0x40	; 64
	.db #0x80	; 128
	.area CSEG    (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
