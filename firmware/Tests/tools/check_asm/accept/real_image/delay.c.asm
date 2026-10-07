;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module delay
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _tick_now
	.globl _P5_5
	.globl _RSTCFG
	.globl _IAP_TPS
	.globl _IAP_CONTR
	.globl _IAP_TRIG
	.globl _IAP_CMD
	.globl _IAP_ADDRL
	.globl _IAP_ADDRH
	.globl _IAP_DATA
	.globl _AUXR
	.globl _P5M0
	.globl _P5M1
	.globl _P5
	.globl _P1M0
	.globl _P1M1
	.globl _AUXINTIF
	.globl _IE2
	.globl _T3L
	.globl _T3H
	.globl _T4T3M
	.globl _P_SW2
	.globl _P3M0
	.globl _P3M1
	.globl _P2M0
	.globl _P2M1
	.globl _P0M0
	.globl _P0M1
	.globl _delay_ms
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
_P0M1	=	0x0093
_P0M0	=	0x0094
_P2M1	=	0x0095
_P2M0	=	0x0096
_P3M1	=	0x00b1
_P3M0	=	0x00b2
_P_SW2	=	0x00ba
_T4T3M	=	0x00d1
_T3H	=	0x00d4
_T3L	=	0x00d5
_IE2	=	0x00af
_AUXINTIF	=	0x00ef
_P1M1	=	0x0091
_P1M0	=	0x0092
_P5	=	0x00c8
_P5M1	=	0x00c9
_P5M0	=	0x00ca
_AUXR	=	0x008e
_IAP_DATA	=	0x00c2
_IAP_ADDRH	=	0x00c3
_IAP_ADDRL	=	0x00c4
_IAP_CMD	=	0x00c5
_IAP_TRIG	=	0x00c6
_IAP_CONTR	=	0x00c7
_IAP_TPS	=	0x00f5
_RSTCFG	=	0x00ff
;--------------------------------------------------------
; special function bits
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
_P5_5	=	0x00cd
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
;Allocation info for local variables in function 'delay_ms'
;------------------------------------------------------------
;ms            Allocated to registers r7 
;start_ms      Allocated to registers r5 r6 
;------------------------------------------------------------
;	firmware/src/delay/delay.c:5: void delay_ms(uint8_t ms) {
;	-----------------------------------------
;	 function delay_ms
;	-----------------------------------------
_delay_ms:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	firmware/src/delay/delay.c:8: if (ms == 0U) {
	mov	a,dpl
	mov	r7,a
	jnz	00102$
;	firmware/src/delay/delay.c:9: return;
	ret
00102$:
;	firmware/src/delay/delay.c:11: start_ms = tick_now();
	push	ar7
	lcall	_tick_now
	mov	r5, dpl
	mov	r6, dph
	pop	ar7
;	firmware/src/delay/delay.c:13: while ((uint16_t)(tick_now() - start_ms) <= ms) {
00103$:
	push	ar7
	push	ar6
	push	ar5
	lcall	_tick_now
	mov	a, dpl
	mov	b, dph
	pop	ar5
	pop	ar6
	pop	ar7
	clr	c
	subb	a,r5
	mov	r3,a
	mov	a,b
	subb	a,r6
	mov	r4,a
	mov	ar1,r7
	mov	r2,#0x00
	clr	c
	mov	a,r1
	subb	a,r3
	mov	a,r2
	subb	a,r4
	jnc	00103$
;	free result
;	firmware/src/delay/delay.c:15: }
	ret
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
