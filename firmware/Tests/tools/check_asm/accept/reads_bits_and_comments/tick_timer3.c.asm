; QA fixture: orl/anl on shared SFRs and byte writes to T3H/T3L are allowed
	.module tick_timer3
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
	anl	_T4T3M,#0xf0
	mov	_T3L,#0x30
	mov	_T3H,#0xf8
	anl	_AUXINTIF,#0xfd
	orl	_IE2,#0x20
	orl	_T4T3M,#0x08
	mov	a,_IE
