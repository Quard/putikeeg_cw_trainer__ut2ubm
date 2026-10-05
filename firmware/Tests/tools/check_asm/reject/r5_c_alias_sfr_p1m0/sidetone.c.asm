;--------------------------------------------------------
; A C alias of P1M0 written with mov.
;--------------------------------------------------------
_AUDIO_MODE	=	0x0092
	.area CSEG    (CODE)
_sidetone_init:
	mov	_AUDIO_MODE,#0x06
	ret
