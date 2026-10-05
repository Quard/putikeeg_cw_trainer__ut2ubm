;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module tick
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _tick_has_elapsed_PARM_2
	.globl _AUXINTIF
	.globl _IE2
	.globl _T3L
	.globl _T3H
	.globl _T4T3M
	.globl _P2M0
	.globl _P2M1
	.globl _tick_count_ms
	.globl _tick_now
	.globl _tick_elapsed_since
	.globl _tick_has_elapsed
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
_tick_count_ms::
	.ds 2
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
	.area	OSEG    (OVR,DATA)
	.area	OSEG    (OVR,DATA)
	.area	OSEG    (OVR,DATA)
_tick_has_elapsed_PARM_2:
	.ds 2
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
;Allocation info for local variables in function 'tick_now'
;------------------------------------------------------------
;snapshot      Allocated to registers r6 r7 
;------------------------------------------------------------
;	firmware/src/tick/tick.c:16: uint16_t tick_now(void) {
;	-----------------------------------------
;	 function tick_now
;	-----------------------------------------
_tick_now:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	firmware/src/tick/tick.c:18: TICK_READ(snapshot);
00101$:
	mov	r6,_tick_count_ms
	mov	r7,(_tick_count_ms + 1)
	mov	a,r6
	cjne	a,_tick_count_ms,00101$
	mov	a,r7
	cjne	a,(_tick_count_ms + 1),00101$
;	firmware/src/tick/tick.c:19: return snapshot;
	mov	dpl, r6
	mov	dph, r7
;	firmware/src/tick/tick.c:20: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'tick_elapsed_since'
;------------------------------------------------------------
;start_ms      Allocated to registers r6 r7 
;snapshot      Allocated to registers r4 r5 
;------------------------------------------------------------
;	firmware/src/tick/tick.c:22: uint16_t tick_elapsed_since(uint16_t start_ms) {
;	-----------------------------------------
;	 function tick_elapsed_since
;	-----------------------------------------
_tick_elapsed_since:
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/tick/tick.c:24: TICK_READ(snapshot);
00101$:
	mov	r4,_tick_count_ms
	mov	r5,(_tick_count_ms + 1)
	mov	a,r4
	cjne	a,_tick_count_ms,00101$
	mov	a,r5
	cjne	a,(_tick_count_ms + 1),00101$
;	firmware/src/tick/tick.c:25: return (uint16_t)(snapshot - start_ms);
	mov	a,r4
	clr	c
	subb	a,r6
	mov	dpl,a
	mov	a,r5
	subb	a,r7
	mov	dph,a
;	firmware/src/tick/tick.c:26: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'tick_has_elapsed'
;------------------------------------------------------------
;duration_ms   Allocated with name '_tick_has_elapsed_PARM_2'
;start_ms      Allocated to registers r6 r7 
;snapshot      Allocated to registers r4 r5 
;------------------------------------------------------------
;	firmware/src/tick/tick.c:28: bool tick_has_elapsed(uint16_t start_ms, uint16_t duration_ms) {
;	-----------------------------------------
;	 function tick_has_elapsed
;	-----------------------------------------
_tick_has_elapsed:
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/tick/tick.c:30: TICK_READ(snapshot);
00101$:
	mov	r4,_tick_count_ms
	mov	r5,(_tick_count_ms + 1)
	mov	a,r4
	cjne	a,_tick_count_ms,00101$
	mov	a,r5
	cjne	a,(_tick_count_ms + 1),00101$
;	firmware/src/tick/tick.c:32: if ((uint16_t)(snapshot - start_ms) < duration_ms) {
	mov	a,r4
	clr	c
	subb	a,r6
	mov	r6,a
	mov	a,r5
	subb	a,r7
	mov	r7,a
	clr	c
	mov	a,r6
	subb	a,_tick_has_elapsed_PARM_2
	mov	a,r7
	subb	a,(_tick_has_elapsed_PARM_2 + 1)
	jnc	00105$
;	firmware/src/tick/tick.c:33: return false;
	mov	dpl, #0x00
	ret
00105$:
;	firmware/src/tick/tick.c:35: return true;
	mov	dpl, #0x01
;	firmware/src/tick/tick.c:36: }
	ret
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
