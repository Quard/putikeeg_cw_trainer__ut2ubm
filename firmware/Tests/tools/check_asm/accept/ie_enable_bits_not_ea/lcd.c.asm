; Single interrupt enables in IE (bits 0xA8-0xAE) may change anywhere; only EA (0xAF) is protected.
_EX0	=	0x00a8
_ET0	=	0x00a9
_ES	=	0x00ac
	.area CSEG    (CODE)
	setb	_ET0
	clr	_ET0
	setb	0xa9
	clr	0xa9
	setb	ie.1
	clr	_EX0
	cpl	0xa8.4
	mov	_ES,c
	setb	(_EX0 + 6)
