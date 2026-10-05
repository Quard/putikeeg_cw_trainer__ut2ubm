; QA fixture: reads of P2, single-bit P2 writes, comments and labels that mention forbidden writes
	.module lcd_spi
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
	orl	_P2M0,#0x1f
	anl	_P2M1,#0xe0
	mov	a,_P2	; mov _P2,#0xff in a comment
	mov	c,_P2_0
	jb	_P2_1,00101$
00101$:
	clr	_P2_3
	mov	_P2_4,c
	setb	_P2_0
	cpl	_P2_2
;	mov	_P2,#0x00
;	clr	_EA
	push	_P2
	mov	r7,_P2
