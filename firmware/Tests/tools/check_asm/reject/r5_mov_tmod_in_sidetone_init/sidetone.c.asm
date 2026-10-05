;--------------------------------------------------------
; Timer 0 setup that overwrites the Timer 1 nibble of TMOD.
;--------------------------------------------------------
_TMOD	=	0x0089
_TR0	=	0x008c
	.area CSEG    (CODE)
_sidetone_init:
	clr	_TR0
	mov	_TMOD,#0x00
	setb	_TR0
	ret
