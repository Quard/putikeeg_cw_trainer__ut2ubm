; QA fixture: SDCC output for "__sfr __at(0xA8) INT_EN; INT_EN &= 0x7F;"
	.module lcd
	.globl _INT_EN
_INT_EN	=	0x00a8
	.area CSEG    (CODE)
_lcd_wake:
	anl	_INT_EN,#0x7f
	ret
