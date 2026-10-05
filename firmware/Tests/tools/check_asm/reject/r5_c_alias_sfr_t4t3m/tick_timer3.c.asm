; QA fixture: SDCC output for "__sfr __at(0xD1) TIMER34; TIMER34 = 0x08;"
	.module tick_timer3
_TIMER34	=	0x00d1
	.area CSEG    (CODE)
	mov	_TIMER34,#0x08
