; QA fixture: SDCC output for "__sbit __at(0xAF) IRQ_ENABLE; IRQ_ENABLE = 0;"
	.module lcd
	.globl _IRQ_ENABLE
_IRQ_ENABLE	=	0x00af
	.area CSEG    (CODE)
_lcd_wake:
	clr	_IRQ_ENABLE
	ret
