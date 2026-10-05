; Dotted P2 pin bits are single-bit operations and stay allowed.
	.area CSEG    (CODE)
	setb	p2.3
	clr	P2.3
	clr	_P2.0
	mov	p2.4,c
	cpl	0xa0.7
	jbc	p2.1,00101$
00101$:
