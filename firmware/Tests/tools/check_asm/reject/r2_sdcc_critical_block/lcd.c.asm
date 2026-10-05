; QA fixture: exact SDCC 4.6 output for a __critical { } block
	.module lcd
	.globl _P2
_P2	=	0x00a0
_P2_0	=	0x00a0
_EA	=	0x00af
_IE	=	0x00a8
_IE2	=	0x00af
_T4T3M	=	0x00d1
_AUXINTIF	=	0x00ef
_P2M0	=	0x0096
_P2M1	=	0x0095
	.area CSEG    (CODE)
_f:
	setb	_f_sloc0_1_0
	jbc	ea,00103$
	clr	_f_sloc0_1_0
00103$:
	mov	a,_x
	inc	a
	mov	_x,a
	mov	c,_f_sloc0_1_0
	mov	ea,c
	ret
