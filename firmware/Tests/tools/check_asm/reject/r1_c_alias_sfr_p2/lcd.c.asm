; QA fixture: SDCC output for "__sfr __at(0xA0) LCD_PORT; LCD_PORT = 0xFF;"
	.module lcd
	.globl _LCD_PORT
_LCD_PORT	=	0x00a0
	.area CSEG    (CODE)
_lcd_wake:
	mov	_LCD_PORT,#0xff
	ret
