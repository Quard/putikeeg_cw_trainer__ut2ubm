;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module tick_timer3
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _AUXINTIF
	.globl _IE2
	.globl _T3L
	.globl _T3H
	.globl _T4T3M
	.globl _P2M0
	.globl _P2M1
	.globl _tick_init
	.globl _tick_isr
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
_P2M1	=	0x0095
_P2M0	=	0x0096
_T4T3M	=	0x00d1
_T3H	=	0x00d4
_T3L	=	0x00d5
_IE2	=	0x00af
_AUXINTIF	=	0x00ef
;--------------------------------------------------------
; special function bits
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
;--------------------------------------------------------
; overlayable register banks
;--------------------------------------------------------
	.area REG_BANK_0	(REL,OVR,DATA)
	.ds 8
;--------------------------------------------------------
; internal ram data
;--------------------------------------------------------
	.area DSEG    (DATA)
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
;--------------------------------------------------------
; indirectly addressable internal ram data
;--------------------------------------------------------
	.area ISEG    (DATA)
;--------------------------------------------------------
; absolute internal ram data
;--------------------------------------------------------
	.area IABS    (ABS,DATA)
	.area IABS    (ABS,DATA)
;--------------------------------------------------------
; bit data
;--------------------------------------------------------
	.area BSEG    (BIT)
;--------------------------------------------------------
; paged external ram data
;--------------------------------------------------------
	.area PSEG    (PAG,XDATA)
;--------------------------------------------------------
; uninitialized external ram data
;--------------------------------------------------------
	.area XSEG    (XDATA)
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area XABS    (ABS,XDATA)
;--------------------------------------------------------
; initialized external ram data
;--------------------------------------------------------
	.area XISEG   (XDATA)
	.area HOME    (CODE)
	.area GSINIT0 (CODE)
	.area GSINIT1 (CODE)
	.area GSINIT2 (CODE)
	.area GSINIT3 (CODE)
	.area GSINIT4 (CODE)
	.area GSINIT5 (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area CSEG    (CODE)
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area HOME    (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area GSINIT  (CODE)
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area HOME    (CODE)
	.area HOME    (CODE)
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area CSEG    (CODE)
;------------------------------------------------------------
;Allocation info for local variables in function 'tick_init'
;------------------------------------------------------------
;	firmware/src/tick/tick_timer3.c:25: void tick_init(void) {
;	-----------------------------------------
;	 function tick_init
;	-----------------------------------------
_tick_init:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	firmware/src/tick/tick_timer3.c:27: T4T3M &= (uint8_t)~T4T3M_T3_MASK;
	anl	_T4T3M,#0xf0
;	firmware/src/tick/tick_timer3.c:28: T3L = (uint8_t)(TICK_TIMER3_RELOAD & 0xFFU);
	mov	_T3L,#0x30
;	firmware/src/tick/tick_timer3.c:29: T3H = (uint8_t)(TICK_TIMER3_RELOAD >> 8);
	mov	_T3H,#0xf8
;	firmware/src/tick/tick_timer3.c:30: AUXINTIF &= (uint8_t)~AUXINTIF_T3IF;
	anl	_AUXINTIF,#0xfd
;	firmware/src/tick/tick_timer3.c:31: tick_count_ms = 0U;
	clr	a
	mov	_tick_count_ms,a
	mov	(_tick_count_ms + 1),a
;	firmware/src/tick/tick_timer3.c:32: IE2 |= IE2_ET3;
	orl	_IE2,#0x20
;	firmware/src/tick/tick_timer3.c:33: T4T3M |= T4T3M_T3R;
	orl	_T4T3M,#0x08
;	firmware/src/tick/tick_timer3.c:34: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'tick_isr'
;------------------------------------------------------------
;	firmware/src/tick/tick_timer3.c:36: void tick_isr(void) __interrupt(TIMER3_INTERRUPT) {
;	-----------------------------------------
;	 function tick_isr
;	-----------------------------------------
_tick_isr:
	push	acc
	push	psw
;	firmware/src/tick/tick_timer3.c:38: AUXINTIF &= (uint8_t)~AUXINTIF_T3IF;
	anl	_AUXINTIF,#0xfd
;	firmware/src/tick/tick_timer3.c:39: ++tick_count_ms;
	mov	a,#0x01
	add	a, _tick_count_ms
	mov	_tick_count_ms,a
	clr	a
	addc	a, (_tick_count_ms + 1)
	mov	(_tick_count_ms + 1),a
;	firmware/src/tick/tick_timer3.c:40: }
	pop	psw
	pop	acc
	reti
;	eliminated unneeded mov psw,# (no regs used in bank)
;	eliminated unneeded push/pop dpl
;	eliminated unneeded push/pop dph
;	eliminated unneeded push/pop b
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
