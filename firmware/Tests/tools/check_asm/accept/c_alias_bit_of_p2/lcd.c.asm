; QA fixture: an sbit alias of P2.0 written with setb/clr is a single-bit write
	.module lcd
_LCD_CS	=	0x00a0
	.area CSEG    (CODE)
	setb	_LCD_CS
	clr	_LCD_CS
	mov	_LCD_CS,c
