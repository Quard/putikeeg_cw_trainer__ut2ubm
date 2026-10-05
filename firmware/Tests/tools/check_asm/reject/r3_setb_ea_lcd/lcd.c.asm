; QA fixture: enable interrupts outside main.c
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
	setb	_EA
