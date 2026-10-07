;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module sidetone
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _tick_now
	.globl _CY
	.globl _AC
	.globl _F0
	.globl _RS1
	.globl _RS0
	.globl _OV
	.globl _F1
	.globl _P
	.globl _PS
	.globl _PT1
	.globl _PX1
	.globl _PT0
	.globl _PX0
	.globl _RD
	.globl _WR
	.globl _T1
	.globl _T0
	.globl _INT1
	.globl _INT0
	.globl _TXD
	.globl _RXD
	.globl _P3_7
	.globl _P3_6
	.globl _P3_5
	.globl _P3_4
	.globl _P3_3
	.globl _P3_2
	.globl _P3_1
	.globl _P3_0
	.globl _EA
	.globl _ES
	.globl _ET1
	.globl _EX1
	.globl _ET0
	.globl _EX0
	.globl _P2_7
	.globl _P2_6
	.globl _P2_5
	.globl _P2_4
	.globl _P2_3
	.globl _P2_2
	.globl _P2_1
	.globl _P2_0
	.globl _SM0
	.globl _SM1
	.globl _SM2
	.globl _REN
	.globl _TB8
	.globl _RB8
	.globl _TI
	.globl _RI
	.globl _P1_7
	.globl _P1_6
	.globl _P1_5
	.globl _P1_4
	.globl _P1_3
	.globl _P1_2
	.globl _P1_1
	.globl _P1_0
	.globl _TF1
	.globl _TR1
	.globl _TF0
	.globl _TR0
	.globl _IE1
	.globl _IT1
	.globl _IE0
	.globl _IT0
	.globl _P0_7
	.globl _P0_6
	.globl _P0_5
	.globl _P0_4
	.globl _P0_3
	.globl _P0_2
	.globl _P0_1
	.globl _P0_0
	.globl _P5_5
	.globl _B
	.globl _ACC
	.globl _PSW
	.globl _IP
	.globl _P3
	.globl _IE
	.globl _P2
	.globl _SBUF
	.globl _SCON
	.globl _P1
	.globl _TH1
	.globl _TH0
	.globl _TL1
	.globl _TL0
	.globl _TMOD
	.globl _TCON
	.globl _PCON
	.globl _DPH
	.globl _DPL
	.globl _SP
	.globl _P0
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
	.globl _sidetone_init
	.globl _sidetone_set_tone_hz
	.globl _sidetone_set_on
	.globl _sidetone_is_busy
	.globl _sidetone_state
	.globl _sidetone_poll
	.globl _sidetone_service
	.globl _sidetone_timer0_isr
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
_P0	=	0x0080
_SP	=	0x0081
_DPL	=	0x0082
_DPH	=	0x0083
_PCON	=	0x0087
_TCON	=	0x0088
_TMOD	=	0x0089
_TL0	=	0x008a
_TL1	=	0x008b
_TH0	=	0x008c
_TH1	=	0x008d
_P1	=	0x0090
_SCON	=	0x0098
_SBUF	=	0x0099
_P2	=	0x00a0
_IE	=	0x00a8
_P3	=	0x00b0
_IP	=	0x00b8
_PSW	=	0x00d0
_ACC	=	0x00e0
_B	=	0x00f0
;--------------------------------------------------------
; special function bits
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
_P5_5	=	0x00cd
_P0_0	=	0x0080
_P0_1	=	0x0081
_P0_2	=	0x0082
_P0_3	=	0x0083
_P0_4	=	0x0084
_P0_5	=	0x0085
_P0_6	=	0x0086
_P0_7	=	0x0087
_IT0	=	0x0088
_IE0	=	0x0089
_IT1	=	0x008a
_IE1	=	0x008b
_TR0	=	0x008c
_TF0	=	0x008d
_TR1	=	0x008e
_TF1	=	0x008f
_P1_0	=	0x0090
_P1_1	=	0x0091
_P1_2	=	0x0092
_P1_3	=	0x0093
_P1_4	=	0x0094
_P1_5	=	0x0095
_P1_6	=	0x0096
_P1_7	=	0x0097
_RI	=	0x0098
_TI	=	0x0099
_RB8	=	0x009a
_TB8	=	0x009b
_REN	=	0x009c
_SM2	=	0x009d
_SM1	=	0x009e
_SM0	=	0x009f
_P2_0	=	0x00a0
_P2_1	=	0x00a1
_P2_2	=	0x00a2
_P2_3	=	0x00a3
_P2_4	=	0x00a4
_P2_5	=	0x00a5
_P2_6	=	0x00a6
_P2_7	=	0x00a7
_EX0	=	0x00a8
_ET0	=	0x00a9
_EX1	=	0x00aa
_ET1	=	0x00ab
_ES	=	0x00ac
_EA	=	0x00af
_P3_0	=	0x00b0
_P3_1	=	0x00b1
_P3_2	=	0x00b2
_P3_3	=	0x00b3
_P3_4	=	0x00b4
_P3_5	=	0x00b5
_P3_6	=	0x00b6
_P3_7	=	0x00b7
_RXD	=	0x00b0
_TXD	=	0x00b1
_INT0	=	0x00b2
_INT1	=	0x00b3
_T0	=	0x00b4
_T1	=	0x00b5
_WR	=	0x00b6
_RD	=	0x00b7
_PX0	=	0x00b8
_PT0	=	0x00b9
_PX1	=	0x00ba
_PT1	=	0x00bb
_PS	=	0x00bc
_P	=	0x00d0
_F1	=	0x00d1
_OV	=	0x00d2
_RS0	=	0x00d3
_RS1	=	0x00d4
_F0	=	0x00d5
_AC	=	0x00d6
_CY	=	0x00d7
;--------------------------------------------------------
; overlayable register banks
;--------------------------------------------------------
	.area REG_BANK_0	(REL,OVR,DATA)
	.ds 8
;--------------------------------------------------------
; overlayable bit register bank
;--------------------------------------------------------
	.area BIT_BANK	(REL,OVR,DATA)
bits:
	.ds 1
	b0 = bits[0]
	b1 = bits[1]
	b2 = bits[2]
	b3 = bits[3]
	b4 = bits[4]
	b5 = bits[5]
	b6 = bits[6]
	b7 = bits[7]
;--------------------------------------------------------
; internal ram data
;--------------------------------------------------------
	.area DSEG    (DATA)
_sidetone_state_value:
	.ds 1
_sidetone_index:
	.ds 1
_keyer_phase:
	.ds 1
_keyer_ticks:
	.ds 2
_keyer_space_ticks:
	.ds 2
_keyer_tip_history:
	.ds 1
_keyer_ring_history:
	.ds 1
_keyer_tip_count:
	.ds 1
_keyer_ring_count:
	.ds 1
_player_ticks:
	.ds 2
_player_code:
	.ds 1
_player_index:
	.ds 1
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
	.area	OSEG    (OVR,DATA)
	.area	OSEG    (OVR,DATA)
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
_sidetone_is_on_requested:
	.ds 1
_sidetone_is_tone_pending:
	.ds 1
_sidetone_is_table_b_active:
	.ds 1
_sidetone_is_headphones_requested:
	.ds 1
_sidetone_is_headphones_active:
	.ds 1
_keyer_is_tip_closed:
	.ds 1
_keyer_is_ring_closed:
	.ds 1
_keyer_is_tip_masked:
	.ds 1
_keyer_is_ring_masked:
	.ds 1
_keyer_has_dit_memory:
	.ds 1
_keyer_has_dah_memory:
	.ds 1
_keyer_was_squeezed:
	.ds 1
_keyer_is_element_dah:
	.ds 1
_keyer_is_char_gap_pending:
	.ds 1
_keyer_is_word_gap_pending:
	.ds 1
_player_is_mark:
	.ds 1
;--------------------------------------------------------
; paged external ram data
;--------------------------------------------------------
	.area PSEG    (PAG,XDATA)
;--------------------------------------------------------
; uninitialized external ram data
;--------------------------------------------------------
	.area XSEG    (XDATA)
_sidetone_table_a:
	.ds 66
_sidetone_table_b:
	.ds 66
_sidetone_arr_a:
	.ds 2
_sidetone_arr_b:
	.ds 2
_sidetone_requested_hz:
	.ds 2
_sidetone_is_plug_candidate_headphones:
	.ds 1
_sidetone_plug_change_time:
	.ds 2
_fill_period:
	.ds 4
_fill_step:
	.ds 1
_sidetone_fill_table_PARM_2:
	.ds 2
_sidetone_fill_table_table_10000_20:
	.ds 2
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
;Allocation info for local variables in function 'sidetone_clamp'
;------------------------------------------------------------
;tone_hz       Allocated to registers r6 r7 
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:46: static uint16_t sidetone_clamp(uint16_t tone_hz) {
;	-----------------------------------------
;	 function sidetone_clamp
;	-----------------------------------------
_sidetone_clamp:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/sidetone/sidetone.c:47: if (tone_hz < SIDETONE_TONE_MIN_HZ) {
	mov	ar4,r6
	mov	ar5,r7
	clr	c
	mov	a,r4
	subb	a,#0x90
	mov	a,r5
	subb	a,#0x01
	jnc	00102$
;	firmware/src/sidetone/sidetone.c:48: return SIDETONE_TONE_MIN_HZ;
	mov	dptr,#0x0190
	ret
00102$:
;	firmware/src/sidetone/sidetone.c:50: if (tone_hz > SIDETONE_TONE_MAX_HZ) {
	clr	c
	mov	a,#0xe8
	subb	a,r4
	mov	a,#0x03
	subb	a,r5
	jnc	00104$
;	free result
;	firmware/src/sidetone/sidetone.c:51: return SIDETONE_TONE_MAX_HZ;
	mov	dptr,#0x03e8
	ret
00104$:
;	firmware/src/sidetone/sidetone.c:53: return tone_hz;
	mov	dpl, r6
	mov	dph, r7
;	firmware/src/sidetone/sidetone.c:54: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_fill_table'
;------------------------------------------------------------
;tone_hz       Allocated with name '_sidetone_fill_table_PARM_2'
;table         Allocated with name '_sidetone_fill_table_table_10000_20'
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:61: static uint16_t sidetone_fill_table(volatile uint16_t MEM_XDATA* MEM_XDATA table, MEM_XDATA uint16_t tone_hz) {
;	-----------------------------------------
;	 function sidetone_fill_table
;	-----------------------------------------
_sidetone_fill_table:
	mov	r7,dph
	mov	a,dpl
	mov	dptr,#_sidetone_fill_table_table_10000_20
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:62: fill_period = (SIDETONE_COUNTER_HZ + ((uint32_t)tone_hz / 2UL)) / (uint32_t)tone_hz;
	mov	dptr,#_sidetone_fill_table_PARM_2
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	clr	a
	mov	r6,a
	mov	r7,a
	clr	c
	rrc	a
	mov	r3,a
	mov	a,r6
	rrc	a
	mov	r2,a
	mov	a,r5
	rrc	a
	mov	r1,a
	mov	a,r4
	rrc	a
	mov	r0,a
	mov	a,#0x1b
	add	a, r1
	mov	r1,a
	mov	a,#0xb7
	addc	a, r2
	mov	r2,a
	clr	a
	addc	a, r3
	mov	r3,a
	mov	__divulong_PARM_2,r4
	mov	(__divulong_PARM_2 + 1),r5
	mov	(__divulong_PARM_2 + 2),r6
	mov	(__divulong_PARM_2 + 3),r7
	mov	dpl, r0
	mov	dph, r1
	mov	b, r2
	mov	a, r3
	lcall	__divulong
	mov	r4, dpl
	mov	r5, dph
	mov	r6, b
	mov	r7, a
	mov	dptr,#_fill_period
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:63: for (fill_step = 0U; fill_step < SIDETONE_TABLE_SIZE; ++fill_step) {
	mov	dptr,#_fill_step
	clr	a
	movx	@dptr,a
	mov	dptr,#_sidetone_fill_table_table_10000_20
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
00104$:
;	firmware/src/sidetone/sidetone.c:64: table[fill_step] = (uint16_t)((fill_period * sidetone_duty_permille[fill_step]) / (uint32_t)1000U);
	mov	dptr,#_fill_step
	movx	a,@dptr
	mov	b,#0x02
	mul	ab
	mov	r4,a
	mov	r5,b
	add	a, r6
	mov	r2,a
	mov	a,r5
	addc	a, r7
	mov	r3,a
	mov	a,r4
	add	a, #_sidetone_duty_permille
	mov	dpl,a
	mov	a,r5
	addc	a, #(_sidetone_duty_permille >> 8)
	mov	dph,a
	clr	a
	movc	a,@a+dptr
	mov	r4,a
	inc	dptr
	clr	a
	movc	a,@a+dptr
	mov	r5,a
	push	ar6
	push	ar7
	mov	dptr,#_fill_period
	movx	a,@dptr
	mov	r0,a
	inc	dptr
	movx	a,@dptr
	mov	r1,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	__mullong_PARM_2,r4
	mov	(__mullong_PARM_2 + 1),r5
	mov	(__mullong_PARM_2 + 2),#0x00
	mov	(__mullong_PARM_2 + 3),#0x00
	mov	dpl, r0
	mov	dph, r1
	mov	b, r6
	mov	a, r7
	push	ar3
	push	ar2
	lcall	__mullong
	mov	r4, dpl
	mov	r5, dph
	mov	r6, b
	mov	r7, a
	mov	__divulong_PARM_2,#0xe8
	mov	(__divulong_PARM_2 + 1),#0x03
	clr	a
	mov	(__divulong_PARM_2 + 2),a
	mov	(__divulong_PARM_2 + 3),a
	mov	dpl, r4
	mov	dph, r5
	mov	b, r6
	mov	a, r7
	lcall	__divulong
	mov	r4, dpl
	mov	r5, dph
	pop	ar2
	pop	ar3
	mov	dpl,r2
	mov	dph,r3
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:63: for (fill_step = 0U; fill_step < SIDETONE_TABLE_SIZE; ++fill_step) {
	mov	dptr,#_fill_step
	movx	a,@dptr
	add	a, #0x01
	movx	@dptr,a
	movx	a,@dptr
	mov	r7,a
	cjne	r7,#0x21,00122$
00122$:
	pop	ar7
	pop	ar6
	jnc	00123$
	ljmp	00104$
00123$:
;	firmware/src/sidetone/sidetone.c:67: return (uint16_t)(fill_period - 1UL);
	mov	dptr,#_fill_period
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	mov	a,r4
	add	a,#0xff
	mov	dpl,a
	mov	a,r5
	addc	a,#0xff
	mov	dph,a
;	firmware/src/sidetone/sidetone.c:68: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_init'
;------------------------------------------------------------
;tone_hz       Allocated to registers 
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:70: void sidetone_init(uint16_t tone_hz) {
;	-----------------------------------------
;	 function sidetone_init
;	-----------------------------------------
_sidetone_init:
;	firmware/src/sidetone/sidetone.c:72: sidetone_is_on_requested = false;
;	assignBit
	clr	_sidetone_is_on_requested
;	firmware/src/sidetone/sidetone.c:73: sidetone_is_tone_pending = false;
;	assignBit
	clr	_sidetone_is_tone_pending
;	firmware/src/sidetone/sidetone.c:74: sidetone_is_table_b_active = false;
;	assignBit
	clr	_sidetone_is_table_b_active
;	firmware/src/sidetone/sidetone.c:75: sidetone_state_value = SIDETONE_STATE_OFF;
	mov	_sidetone_state_value,#0x00
;	firmware/src/sidetone/sidetone.c:76: sidetone_index = 0U;
	mov	_sidetone_index,#0x00
;	firmware/src/sidetone/sidetone.c:77: sidetone_requested_hz = sidetone_clamp(tone_hz);
	lcall	_sidetone_clamp
	mov	r6, dpl
	mov	r7, dph
	mov	dptr,#_sidetone_requested_hz
	mov	a,r6
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:78: sidetone_arr_a = sidetone_fill_table(sidetone_table_a, sidetone_requested_hz);
	mov	dptr,#_sidetone_fill_table_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dptr,#_sidetone_table_a
	lcall	_sidetone_fill_table
	mov	a, dpl
	mov	b, dph
	mov	dptr,#_sidetone_arr_a
	movx	@dptr,a
	mov	a,b
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:80: SIDETONE_PWM_CONFIGURE(sidetone_arr_a);
	orl	_P_SW2,#0x80
	mov	dptr,#0xfeb1
	clr	a
	movx	@dptr,a
	mov	dptr,#0xfecc
	movx	@dptr,a
	mov	dptr,#0xfec0
	movx	@dptr,a
	mov	dptr,#0xfec4
	movx	@dptr,a
	mov	dptr,#0xfec1
	movx	@dptr,a
	mov	dptr,#0xfeb3
	movx	@dptr,a
	dec	dpl
	movx	@dptr,a
	mov	dptr,#0xfed0
	movx	@dptr,a
	inc	dptr
	inc	a
	movx	@dptr,a
	mov	dptr,#_sidetone_arr_a
	movx	a,@dptr
	inc	dptr
	movx	a,@dptr
	mov	dptr,#0xfed2
	movx	@dptr,a
	mov	dptr,#_sidetone_arr_a
	movx	a,@dptr
	mov	dptr,#0xfed3
	movx	@dptr,a
	mov	dptr,#0xfec8
	mov	a,#0x68
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dptr,#0xfed5
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dptr,#0xfecc
	mov	a,#0x3c
	movx	@dptr,a
	mov	dptr,#0xfedd
	mov	a,#0x80
	movx	@dptr,a
	mov	dptr,#0xfec7
	rl	a
	movx	@dptr,a
	mov	dptr,#0xfec5
	clr	a
	movx	@dptr,a
	mov	dptr,#0xfec0
	mov	a,#0x81
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:81: SIDETONE_PINS_CONFIGURE();
;	assignBit
	setb	_P1_1
;	assignBit
	setb	_P1_2
	orl	_P1M0,#0x06
	anl	_P1M1,#0xf9
	mov	dptr,#0xfe11
	movx	a,@dptr
	orl	a,#0x01
	movx	@dptr,a
	orl	_P1M1,#0x01
	anl	_P1M0,#0xfe
;	firmware/src/sidetone/sidetone.c:84: sidetone_is_headphones_requested = SIDETONE_PINS_IS_HEADPHONES();
	mov	c,_P1_0
	cpl	c
	mov	_sidetone_is_headphones_requested,c
;	firmware/src/sidetone/sidetone.c:85: sidetone_is_headphones_active = sidetone_is_headphones_requested;
;	assignBit
	mov	c,_sidetone_is_headphones_requested
	mov	_sidetone_is_headphones_active,c
;	firmware/src/sidetone/sidetone.c:86: sidetone_is_plug_candidate_headphones = sidetone_is_headphones_requested;
	mov	dptr,#_sidetone_is_plug_candidate_headphones
	mov	c,_sidetone_is_headphones_requested
	clr	a
	rlc	a
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:87: sidetone_plug_change_time = 0U;
	mov	dptr,#_sidetone_plug_change_time
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:89: SIDETONE_TIMER_START();
;	assignBit
	clr	_TR0
;	assignBit
	clr	_ET0
	anl	_TMOD,#0xf0
	anl	_AUXR,#0x7f
	mov	_TL0,#0x0c
	mov	_TH0,#0xfe
;	assignBit
	clr	_TF0
;	assignBit
	setb	_ET0
;	assignBit
	setb	_TR0
;	firmware/src/sidetone/sidetone.c:90: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_set_tone_hz'
;------------------------------------------------------------
;tone_hz       Allocated to registers 
;clamped       Allocated to registers r6 r7 
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:92: void sidetone_set_tone_hz(uint16_t tone_hz) {
;	-----------------------------------------
;	 function sidetone_set_tone_hz
;	-----------------------------------------
_sidetone_set_tone_hz:
;	firmware/src/sidetone/sidetone.c:93: const uint16_t clamped = sidetone_clamp(tone_hz);
	lcall	_sidetone_clamp
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/sidetone/sidetone.c:95: if (clamped == sidetone_requested_hz) {
	mov	dptr,#_sidetone_requested_hz
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	mov	a,r6
	cjne	a,ar4,00102$
	mov	a,r7
	cjne	a,ar5,00102$
;	firmware/src/sidetone/sidetone.c:96: return;
	ret
00102$:
;	firmware/src/sidetone/sidetone.c:99: sidetone_is_tone_pending = false;
;	assignBit
	clr	_sidetone_is_tone_pending
;	firmware/src/sidetone/sidetone.c:101: sidetone_requested_hz = clamped;
	mov	dptr,#_sidetone_requested_hz
	mov	a,r6
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:102: if (sidetone_is_table_b_active) {
	jnb	_sidetone_is_table_b_active,00106$
;	firmware/src/sidetone/sidetone.c:103: sidetone_arr_a = sidetone_fill_table(sidetone_table_a, clamped);
	mov	dptr,#_sidetone_fill_table_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dptr,#_sidetone_table_a
	lcall	_sidetone_fill_table
	mov	a, dpl
	mov	b, dph
	mov	dptr,#_sidetone_arr_a
	movx	@dptr,a
	mov	a,b
	inc	dptr
	movx	@dptr,a
	sjmp	00108$
00106$:
;	firmware/src/sidetone/sidetone.c:105: sidetone_arr_b = sidetone_fill_table(sidetone_table_b, clamped);
	mov	dptr,#_sidetone_fill_table_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dptr,#_sidetone_table_b
	lcall	_sidetone_fill_table
	mov	a, dpl
	mov	b, dph
	mov	dptr,#_sidetone_arr_b
	movx	@dptr,a
	mov	a,b
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:107: SIDETONE_PREEMPTION_POINT();
00108$:
;	firmware/src/sidetone/sidetone.c:108: sidetone_is_tone_pending = true;
;	assignBit
	setb	_sidetone_is_tone_pending
;	firmware/src/sidetone/sidetone.c:109: SIDETONE_PREEMPTION_POINT();
;	firmware/src/sidetone/sidetone.c:110: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_set_on'
;------------------------------------------------------------
;is_on         Allocated to registers 
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:112: void sidetone_set_on(bool is_on) { sidetone_is_on_requested = is_on; }
;	-----------------------------------------
;	 function sidetone_set_on
;	-----------------------------------------
_sidetone_set_on:
	mov	a, dpl
	add	a,#0xff
	mov	_sidetone_is_on_requested,c
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_is_busy'
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:114: bool sidetone_is_busy(void) {
;	-----------------------------------------
;	 function sidetone_is_busy
;	-----------------------------------------
_sidetone_is_busy:
;	firmware/src/sidetone/sidetone.c:115: if (sidetone_state_value != SIDETONE_STATE_OFF) {
	mov	a,_sidetone_state_value
	jz	00102$
;	firmware/src/sidetone/sidetone.c:116: return true;
	mov	dpl, #0x01
	ret
00102$:
;	firmware/src/sidetone/sidetone.c:118: return sidetone_is_on_requested;
	mov	c,_sidetone_is_on_requested
	clr	a
	rlc	a
	mov	dpl, a
;	firmware/src/sidetone/sidetone.c:119: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_state'
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:121: sidetone_state_t sidetone_state(void) { return sidetone_state_value; }
;	-----------------------------------------
;	 function sidetone_state
;	-----------------------------------------
_sidetone_state:
	mov	dpl, _sidetone_state_value
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_poll'
;------------------------------------------------------------
;now           Allocated to registers r6 r7 
;is_raw_headphones Allocated to registers r5 
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:123: void sidetone_poll(void) {
;	-----------------------------------------
;	 function sidetone_poll
;	-----------------------------------------
_sidetone_poll:
;	firmware/src/sidetone/sidetone.c:124: const uint16_t now = tick_now();
	lcall	_tick_now
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/sidetone/sidetone.c:125: const bool is_raw_headphones = SIDETONE_PINS_IS_HEADPHONES();
	mov	c,_P1_0
	cpl	c
	clr	a
	rlc	a
	mov	r5,a
;	firmware/src/sidetone/sidetone.c:127: if (is_raw_headphones != sidetone_is_plug_candidate_headphones) {
	mov	dptr,#_sidetone_is_plug_candidate_headphones
	movx	a,@dptr
	mov	r4,a
	mov	a,r5
	cjne	a,ar4,00130$
	sjmp	00105$
00130$:
;	firmware/src/sidetone/sidetone.c:128: sidetone_is_plug_candidate_headphones = is_raw_headphones;
	mov	dptr,#_sidetone_is_plug_candidate_headphones
	mov	a,r5
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:129: sidetone_plug_change_time = now;
	mov	dptr,#_sidetone_plug_change_time
	mov	a,r6
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	ret
00105$:
;	firmware/src/sidetone/sidetone.c:130: } else if ((sidetone_is_plug_candidate_headphones != sidetone_is_headphones_requested) &&
	mov	b,r4
	mov	c,_sidetone_is_headphones_requested
	clr	a
	rlc	a
	cjne	a,b,00131$
	ret
00131$:
;	firmware/src/sidetone/sidetone.c:131: ((uint16_t)(now - sidetone_plug_change_time) >= SIDETONE_PLUG_DEBOUNCE_MS)) {
	mov	dptr,#_sidetone_plug_change_time
	movx	a,@dptr
	mov	r3,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	mov	a,r6
	clr	c
	subb	a,r3
	mov	r6,a
	mov	a,r7
	subb	a,r5
	mov	r7,a
	clr	c
	mov	a,r6
	subb	a,#0x32
	mov	a,r7
	subb	a,#0x00
	jc	00107$
;	firmware/src/sidetone/sidetone.c:132: sidetone_is_headphones_requested = sidetone_is_plug_candidate_headphones;
;	assignBit
	mov	a,r4
	rrc	a
	mov	_sidetone_is_headphones_requested,c
00107$:
;	firmware/src/sidetone/sidetone.c:134: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_service'
;------------------------------------------------------------
;state         Allocated to registers r7 
;index         Allocated to registers r7 
;compare       Allocated to registers r5 r6 
;is_up         Allocated to registers 
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:141: void sidetone_service(void) {
;	-----------------------------------------
;	 function sidetone_service
;	-----------------------------------------
_sidetone_service:
;	firmware/src/sidetone/sidetone.c:142: const uint8_t state = sidetone_state_value;
	mov	r7,_sidetone_state_value
;	firmware/src/sidetone/sidetone.c:143: uint8_t index = sidetone_index;
	mov	r6,_sidetone_index
;	firmware/src/sidetone/sidetone.c:147: if (state == SIDETONE_STATE_OFF) {
	mov	a,r7
	jnz	00133$
;	firmware/src/sidetone/sidetone.c:148: if (!sidetone_is_on_requested) {
	jb	_sidetone_is_on_requested,00102$
;	firmware/src/sidetone/sidetone.c:149: return;
	ret
00102$:
;	firmware/src/sidetone/sidetone.c:151: if (sidetone_is_tone_pending) {
	jnb	_sidetone_is_tone_pending,00110$
;	firmware/src/sidetone/sidetone.c:152: sidetone_is_table_b_active = !sidetone_is_table_b_active;
	cpl	_sidetone_is_table_b_active
;	firmware/src/sidetone/sidetone.c:153: sidetone_is_tone_pending = false;
;	assignBit
	clr	_sidetone_is_tone_pending
;	firmware/src/sidetone/sidetone.c:155: if (sidetone_is_table_b_active) {
	jnb	_sidetone_is_table_b_active,00104$
;	firmware/src/sidetone/sidetone.c:156: compare = sidetone_arr_b;
	mov	dptr,#_sidetone_arr_b
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	sjmp	00106$
00104$:
;	firmware/src/sidetone/sidetone.c:158: compare = sidetone_arr_a;
	mov	dptr,#_sidetone_arr_a
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
;	firmware/src/sidetone/sidetone.c:160: SIDETONE_PWM_SET_ARR(compare);
00106$:
	mov	ar3,r5
	mov	dptr,#0xfed2
	mov	a,r3
	movx	@dptr,a
	inc	dptr
	mov	a,r4
	movx	@dptr,a
00110$:
;	firmware/src/sidetone/sidetone.c:162: sidetone_is_headphones_active = sidetone_is_headphones_requested;
;	assignBit
	mov	c,_sidetone_is_headphones_requested
;	firmware/src/sidetone/sidetone.c:164: if (sidetone_is_headphones_active) {
	mov	_sidetone_is_headphones_active,c
	jnc	00120$
;	firmware/src/sidetone/sidetone.c:165: SIDETONE_PWM_SET_COMPARE_HEADPHONES(0U);
	mov	dptr,#0xfed7
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:166: SIDETONE_PWM_GENERATE_UPDATE();
	mov	dptr,#0xfec7
	inc	a
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:167: SIDETONE_PWM_ENABLE_HEADPHONES();
	mov	dptr,#0xfeb1
	mov	a,#0x04
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:169: SIDETONE_PWM_SET_COMPARE_SPEAKER(0U);
	sjmp	00131$
00120$:
	mov	dptr,#0xfed5
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:170: SIDETONE_PWM_GENERATE_UPDATE();
	mov	dptr,#0xfec7
	inc	a
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:171: SIDETONE_PWM_ENABLE_SPEAKER();
	mov	dptr,#0xfeb1
	inc	a
	movx	@dptr,a
00131$:
;	firmware/src/sidetone/sidetone.c:173: sidetone_index = 0U;
	mov	_sidetone_index,#0x00
;	firmware/src/sidetone/sidetone.c:174: sidetone_state_value = SIDETONE_STATE_RISING;
	mov	_sidetone_state_value,#0x01
;	firmware/src/sidetone/sidetone.c:175: return;
	ret
00133$:
;	firmware/src/sidetone/sidetone.c:178: if (state == SIDETONE_STATE_ZERO_WAIT) {
	cjne	r7,#0x04,00141$
;	firmware/src/sidetone/sidetone.c:180: ++index;
	inc	r6
;	firmware/src/sidetone/sidetone.c:181: if (SIDETONE_PWM_IS_UPDATE_FLAG_SET() || (index >= SIDETONE_ZERO_WAIT_LIMIT)) {
	mov	dptr,#0xfec5
	movx	a,@dptr
	jb	acc.0,00134$
	cjne	r6,#0x0c,00313$
00313$:
	jc	00138$
;	firmware/src/sidetone/sidetone.c:182: SIDETONE_PWM_DISABLE_OUTPUTS();
00134$:
	mov	dptr,#0xfeb1
	clr	a
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:183: index = 0U;
	mov	r6,a
;	firmware/src/sidetone/sidetone.c:184: sidetone_state_value = SIDETONE_STATE_OFF;
	mov	_sidetone_state_value,a
00138$:
;	firmware/src/sidetone/sidetone.c:186: sidetone_index = index;
	mov	_sidetone_index,r6
;	firmware/src/sidetone/sidetone.c:187: return;
	ret
00141$:
;	firmware/src/sidetone/sidetone.c:191: is_up = sidetone_is_on_requested && !sidetone_is_tone_pending;
	jnb	_sidetone_is_on_requested,00150$
	jb	_sidetone_is_tone_pending,00150$
;	firmware/src/sidetone/sidetone.c:193: if (index >= SIDETONE_RAMP_STEPS) {
	cjne	r6,#0x20,00317$
00317$:
	jc	00143$
;	firmware/src/sidetone/sidetone.c:194: sidetone_state_value = SIDETONE_STATE_STEADY;
	mov	_sidetone_state_value,#0x02
;	firmware/src/sidetone/sidetone.c:195: return;
	ret
00143$:
;	firmware/src/sidetone/sidetone.c:197: ++index;
	mov	a,r6
	inc	a
	mov	r7,a
;	firmware/src/sidetone/sidetone.c:198: sidetone_state_value = (index == SIDETONE_RAMP_STEPS) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING;
	cjne	r7,#0x20,00174$
	mov	r5,#0x02
	sjmp	00175$
00174$:
	mov	r5,#0x01
00175$:
	mov	_sidetone_state_value,r5
	sjmp	00151$
00150$:
;	firmware/src/sidetone/sidetone.c:200: if (index == 0U) {
	mov	a,r6
	jnz	00148$
;	firmware/src/sidetone/sidetone.c:202: SIDETONE_PWM_CLEAR_UPDATE_FLAG();
	mov	dptr,#0xfec5
	clr	a
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:203: sidetone_state_value = SIDETONE_STATE_ZERO_WAIT;
	mov	_sidetone_state_value,#0x04
;	firmware/src/sidetone/sidetone.c:204: return;
	ret
00148$:
;	firmware/src/sidetone/sidetone.c:206: --index;
	mov	a,r6
	dec	a
;	firmware/src/sidetone/sidetone.c:207: sidetone_state_value = (index == 0U) ? SIDETONE_STATE_ZERO_WAIT : SIDETONE_STATE_FALLING;
	mov	r7,a
	jnz	00176$
	mov	r6,#0x04
	sjmp	00177$
00176$:
	mov	r6,#0x03
00177$:
	mov	_sidetone_state_value,r6
00151$:
;	firmware/src/sidetone/sidetone.c:209: sidetone_index = index;
	mov	_sidetone_index,r7
;	firmware/src/sidetone/sidetone.c:211: if (sidetone_is_table_b_active) {
	jnb	_sidetone_is_table_b_active,00153$
;	firmware/src/sidetone/sidetone.c:212: compare = sidetone_table_b[index];
	mov	a,r7
	mov	b,#0x02
	mul	ab
	add	a, #_sidetone_table_b
	mov	dpl,a
	mov	a,#(_sidetone_table_b >> 8)
	addc	a, b
	mov	dph,a
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	sjmp	00154$
00153$:
;	firmware/src/sidetone/sidetone.c:214: compare = sidetone_table_a[index];
	mov	a,r7
	mov	b,#0x02
	mul	ab
	add	a, #_sidetone_table_a
	mov	dpl,a
	mov	a,#(_sidetone_table_a >> 8)
	addc	a, b
	mov	dph,a
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
00154$:
;	firmware/src/sidetone/sidetone.c:216: if (sidetone_is_headphones_active) {
	jnb	_sidetone_is_headphones_active,00158$
;	firmware/src/sidetone/sidetone.c:217: SIDETONE_PWM_SET_COMPARE_HEADPHONES(compare);
	mov	ar4,r6
	mov	dptr,#0xfed7
	mov	a,r4
	movx	@dptr,a
	mov	ar4,r5
	inc	dptr
	mov	a,r4
	movx	@dptr,a
;	firmware/src/sidetone/sidetone.c:219: SIDETONE_PWM_SET_COMPARE_SPEAKER(compare);
	sjmp	00163$
00158$:
	mov	ar4,r6
	mov	dptr,#0xfed5
	mov	a,r4
	movx	@dptr,a
	inc	dptr
	mov	a,r5
	movx	@dptr,a
00163$:
;	firmware/src/sidetone/sidetone.c:225: if (index == 0U) {
	mov	a,r7
	jnz	00169$
;	firmware/src/sidetone/sidetone.c:226: SIDETONE_PWM_CLEAR_UPDATE_FLAG();
	mov	dptr,#0xfec5
	clr	a
	movx	@dptr,a
00169$:
;	firmware/src/sidetone/sidetone.c:228: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'keyer_service'
;------------------------------------------------------------
;tip_sample    Allocated to registers r7 
;ring_sample   Allocated to registers r6 
;levers        Allocated to registers r7 
;wanted        Allocated to registers r4 
;start         Allocated to registers r5 
;event         Allocated to registers r6 
;------------------------------------------------------------
;	firmware/inc/cw/keyer_service.h:59: static void keyer_service(void) {
;	-----------------------------------------
;	 function keyer_service
;	-----------------------------------------
_keyer_service:
;	firmware/inc/cw/keyer_service.h:70: if (!keyer_is_enabled) {
	jb	_keyer_is_enabled,00102$
;	firmware/inc/cw/keyer_service.h:71: return;
	ret
00102$:
;	firmware/inc/cw/keyer_service.h:73: tip_sample = PADDLE_PINS_IS_TIP_CLOSED() ? 1U : 0U;
	jb	_P3_2,00214$
	mov	r7,#0x01
	sjmp	00215$
00214$:
	mov	r7,#0x00
00215$:
;	firmware/inc/cw/keyer_service.h:74: ring_sample = PADDLE_PINS_IS_RING_CLOSED() ? 1U : 0U;
	jb	_P5_5,00216$
	mov	r6,#0x01
	sjmp	00217$
00216$:
	mov	r6,#0x00
00217$:
;	firmware/inc/cw/keyer_service.h:76: if (keyer_is_start_requested) {
	jnb	_keyer_is_start_requested,00108$
;	firmware/inc/cw/keyer_service.h:77: keyer_tip_history = 0x00U;
	mov	_keyer_tip_history,#0x00
;	firmware/inc/cw/keyer_service.h:78: keyer_tip_count = 0U;
	mov	_keyer_tip_count,#0x00
;	firmware/inc/cw/keyer_service.h:79: keyer_is_tip_closed = false;
;	assignBit
	clr	_keyer_is_tip_closed
;	firmware/inc/cw/keyer_service.h:80: if (tip_sample != 0U) {
	mov	a,r7
	jz	00104$
;	firmware/inc/cw/keyer_service.h:81: keyer_tip_history = 0xFFU;
	mov	_keyer_tip_history,#0xff
;	firmware/inc/cw/keyer_service.h:82: keyer_tip_count = 8U;
	mov	_keyer_tip_count,#0x08
;	firmware/inc/cw/keyer_service.h:83: keyer_is_tip_closed = true;
;	assignBit
	setb	_keyer_is_tip_closed
00104$:
;	firmware/inc/cw/keyer_service.h:85: keyer_is_tip_masked = keyer_is_tip_closed;
;	assignBit
	mov	c,_keyer_is_tip_closed
	mov	_keyer_is_tip_masked,c
;	firmware/inc/cw/keyer_service.h:86: keyer_ring_history = 0x00U;
	mov	_keyer_ring_history,#0x00
;	firmware/inc/cw/keyer_service.h:87: keyer_ring_count = 0U;
	mov	_keyer_ring_count,#0x00
;	firmware/inc/cw/keyer_service.h:88: keyer_is_ring_closed = false;
;	assignBit
	clr	_keyer_is_ring_closed
;	firmware/inc/cw/keyer_service.h:89: if (ring_sample != 0U) {
	mov	a,r6
	jz	00106$
;	firmware/inc/cw/keyer_service.h:90: keyer_ring_history = 0xFFU;
	mov	_keyer_ring_history,#0xff
;	firmware/inc/cw/keyer_service.h:91: keyer_ring_count = 8U;
	mov	_keyer_ring_count,#0x08
;	firmware/inc/cw/keyer_service.h:92: keyer_is_ring_closed = true;
;	assignBit
	setb	_keyer_is_ring_closed
00106$:
;	firmware/inc/cw/keyer_service.h:94: keyer_is_ring_masked = keyer_is_ring_closed;
;	assignBit
	mov	c,_keyer_is_ring_closed
	mov	_keyer_is_ring_masked,c
;	firmware/inc/cw/keyer_service.h:95: keyer_phase = KEYER_PHASE_IDLE;
;	firmware/inc/cw/keyer_service.h:96: keyer_ticks = 0U;
	clr	a
	mov	_keyer_phase,a
	mov	_keyer_ticks,a
	mov	(_keyer_ticks + 1),a
;	firmware/inc/cw/keyer_service.h:97: keyer_space_ticks = 0U;
	mov	_keyer_space_ticks,a
	mov	(_keyer_space_ticks + 1),a
;	firmware/inc/cw/keyer_service.h:98: keyer_has_dit_memory = false;
;	assignBit
	clr	_keyer_has_dit_memory
;	firmware/inc/cw/keyer_service.h:99: keyer_has_dah_memory = false;
;	assignBit
	clr	_keyer_has_dah_memory
;	firmware/inc/cw/keyer_service.h:100: keyer_was_squeezed = false;
;	assignBit
	clr	_keyer_was_squeezed
;	firmware/inc/cw/keyer_service.h:101: keyer_is_element_dah = false;
;	assignBit
	clr	_keyer_is_element_dah
;	firmware/inc/cw/keyer_service.h:102: keyer_is_char_gap_pending = false;
;	assignBit
	clr	_keyer_is_char_gap_pending
;	firmware/inc/cw/keyer_service.h:103: keyer_is_word_gap_pending = false;
;	assignBit
	clr	_keyer_is_word_gap_pending
;	firmware/inc/cw/keyer_service.h:104: KEYER_SIDETONE_OFF();
;	assignBit
	clr	_sidetone_is_on_requested
;	firmware/inc/cw/keyer_service.h:105: keyer_is_start_requested = false;
;	assignBit
	clr	_keyer_is_start_requested
;	firmware/inc/cw/keyer_service.h:106: return;
	ret
00108$:
;	firmware/inc/cw/keyer_service.h:110: if ((keyer_tip_history & 0x80U) != 0U) {
	mov	a,_keyer_tip_history
	jnb	acc.7,00110$
;	firmware/inc/cw/keyer_service.h:111: --keyer_tip_count;
	dec	_keyer_tip_count
00110$:
;	firmware/inc/cw/keyer_service.h:113: keyer_tip_history = (uint8_t)(keyer_tip_history << 1);
	mov	a,_keyer_tip_history
	add	a,_keyer_tip_history
	mov	_keyer_tip_history,a
;	firmware/inc/cw/keyer_service.h:114: if (tip_sample != 0U) {
	mov	a,r7
	jz	00112$
;	firmware/inc/cw/keyer_service.h:115: keyer_tip_history |= 0x01U;
	orl	_keyer_tip_history,#0x01
;	firmware/inc/cw/keyer_service.h:116: ++keyer_tip_count;
	inc	_keyer_tip_count
00112$:
;	firmware/inc/cw/keyer_service.h:118: if (keyer_tip_count >= KEYER_FILTER_CLOSED_MIN) {
	mov	a,#0x100 - 0x05
	add	a,_keyer_tip_count
	jnc	00116$
;	firmware/inc/cw/keyer_service.h:119: keyer_is_tip_closed = true;
;	assignBit
	setb	_keyer_is_tip_closed
	sjmp	00117$
00116$:
;	firmware/inc/cw/keyer_service.h:120: } else if (keyer_tip_count <= KEYER_FILTER_OPEN_MAX) {
	mov	a,_keyer_tip_count
	add	a,#0xff - 0x03
	jc	00117$
;	free result
;	firmware/inc/cw/keyer_service.h:121: keyer_is_tip_closed = false;
;	assignBit
	clr	_keyer_is_tip_closed
00117$:
;	firmware/inc/cw/keyer_service.h:123: if (!keyer_is_tip_closed) {
	jb	_keyer_is_tip_closed,00119$
;	firmware/inc/cw/keyer_service.h:124: keyer_is_tip_masked = false;
;	assignBit
	clr	_keyer_is_tip_masked
00119$:
;	firmware/inc/cw/keyer_service.h:127: if ((keyer_ring_history & 0x80U) != 0U) {
	mov	a,_keyer_ring_history
	jnb	acc.7,00121$
;	firmware/inc/cw/keyer_service.h:128: --keyer_ring_count;
	dec	_keyer_ring_count
00121$:
;	firmware/inc/cw/keyer_service.h:130: keyer_ring_history = (uint8_t)(keyer_ring_history << 1);
	mov	a,_keyer_ring_history
	add	a,_keyer_ring_history
	mov	_keyer_ring_history,a
;	firmware/inc/cw/keyer_service.h:131: if (ring_sample != 0U) {
	mov	a,r6
	jz	00123$
;	firmware/inc/cw/keyer_service.h:132: keyer_ring_history |= 0x01U;
	orl	_keyer_ring_history,#0x01
;	firmware/inc/cw/keyer_service.h:133: ++keyer_ring_count;
	inc	_keyer_ring_count
00123$:
;	firmware/inc/cw/keyer_service.h:135: if (keyer_ring_count >= KEYER_FILTER_CLOSED_MIN) {
	mov	a,#0x100 - 0x05
	add	a,_keyer_ring_count
	jnc	00127$
;	firmware/inc/cw/keyer_service.h:136: keyer_is_ring_closed = true;
;	assignBit
	setb	_keyer_is_ring_closed
	sjmp	00128$
00127$:
;	firmware/inc/cw/keyer_service.h:137: } else if (keyer_ring_count <= KEYER_FILTER_OPEN_MAX) {
	mov	a,_keyer_ring_count
	add	a,#0xff - 0x03
	jc	00128$
;	free result
;	firmware/inc/cw/keyer_service.h:138: keyer_is_ring_closed = false;
;	assignBit
	clr	_keyer_is_ring_closed
00128$:
;	firmware/inc/cw/keyer_service.h:140: if (!keyer_is_ring_closed) {
	jb	_keyer_is_ring_closed,00130$
;	firmware/inc/cw/keyer_service.h:141: keyer_is_ring_masked = false;
;	assignBit
	clr	_keyer_is_ring_masked
00130$:
;	firmware/inc/cw/keyer_service.h:144: levers = 0U;
	mov	r7,#0x00
;	firmware/inc/cw/keyer_service.h:145: if (keyer_is_tip_closed && !keyer_is_tip_masked) {
	jnb	_keyer_is_tip_closed,00132$
	jb	_keyer_is_tip_masked,00132$
;	firmware/inc/cw/keyer_service.h:146: levers = keyer_is_swapped ? KEYER_LEVER_DAH : KEYER_LEVER_DIT;
	jnb	_keyer_is_swapped,00218$
	mov	r6,#0x02
	sjmp	00219$
00218$:
	mov	r6,#0x01
00219$:
	mov	ar7,r6
00132$:
;	firmware/inc/cw/keyer_service.h:148: if (keyer_is_ring_closed && !keyer_is_ring_masked) {
	jnb	_keyer_is_ring_closed,00135$
	jb	_keyer_is_ring_masked,00135$
;	firmware/inc/cw/keyer_service.h:149: levers |= keyer_is_swapped ? KEYER_LEVER_DIT : KEYER_LEVER_DAH;
	jnb	_keyer_is_swapped,00220$
	mov	r6,#0x01
	sjmp	00221$
00220$:
	mov	r6,#0x02
00221$:
	mov	a,r6
	orl	ar7,a
00135$:
;	firmware/inc/cw/keyer_service.h:152: event = KEYER_EVENT_NONE;
;	firmware/inc/cw/keyer_service.h:153: start = 0U;
;	firmware/inc/cw/keyer_service.h:154: if (keyer_phase == KEYER_PHASE_IDLE) {
	clr	a
	mov	r6,a
	mov	r5,a
	mov	a,_keyer_phase
	jnz	00186$
;	firmware/inc/cw/keyer_service.h:155: if (levers == 0U) {
	mov	a,r7
	jnz	00702$
	ljmp	00187$
00702$:
;	firmware/inc/cw/keyer_service.h:157: } else if (keyer_is_straight) {
	jnb	_keyer_is_straight,00138$
;	firmware/inc/cw/keyer_service.h:158: keyer_phase = KEYER_PHASE_STRAIGHT_DOWN;
	mov	_keyer_phase,#0x01
;	firmware/inc/cw/keyer_service.h:159: keyer_ticks = 1U;
	mov	_keyer_ticks,#0x01
	mov	(_keyer_ticks + 1),#0x00
;	firmware/inc/cw/keyer_service.h:160: KEYER_SIDETONE_ON();
;	assignBit
	setb	_sidetone_is_on_requested
;	firmware/inc/cw/keyer_service.h:161: keyer_is_char_gap_pending = false;
;	assignBit
	clr	_keyer_is_char_gap_pending
;	firmware/inc/cw/keyer_service.h:162: keyer_is_word_gap_pending = false;
;	assignBit
	clr	_keyer_is_word_gap_pending
	ljmp	00187$
00138$:
;	firmware/inc/cw/keyer_service.h:165: start = ((levers & KEYER_LEVER_DIT) != 0U) ? KEYER_LEVER_DIT : KEYER_LEVER_DAH;
	mov	a,r7
	jnb	acc.0,00222$
	mov	r4,#0x01
	sjmp	00223$
00222$:
	mov	r4,#0x02
00223$:
	mov	ar5,r4
	ljmp	00187$
00186$:
;	firmware/inc/cw/keyer_service.h:167: } else if (keyer_phase == KEYER_PHASE_STRAIGHT_DOWN) {
	mov	a,#0x01
	cjne	a,_keyer_phase,00183$
;	firmware/inc/cw/keyer_service.h:168: if (levers != 0U) {
	mov	a,r7
	jz	00146$
;	firmware/inc/cw/keyer_service.h:169: if (keyer_ticks != 0xFFFFU) {
	mov	r3,_keyer_ticks
	mov	r4,(_keyer_ticks + 1)
	cjne	r3,#0xff,00708$
	cjne	r4,#0xff,00708$
	ljmp	00187$
00708$:
;	firmware/inc/cw/keyer_service.h:170: ++keyer_ticks;
	inc	_keyer_ticks
	clr	a
	cjne	a,_keyer_ticks,00709$
	inc	(_keyer_ticks + 1)
00709$:
	ljmp	00187$
00146$:
;	firmware/inc/cw/keyer_service.h:173: KEYER_SIDETONE_OFF();
;	assignBit
	clr	_sidetone_is_on_requested
;	firmware/inc/cw/keyer_service.h:174: event = (keyer_ticks >= keyer_two_dit_ticks) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT;
	mov	dptr,#_keyer_two_dit_ticks
	movx	a,@dptr
	mov	r3,a
	inc	dptr
	movx	a,@dptr
	mov	r4,a
	clr	c
	mov	a,_keyer_ticks
	subb	a,r3
	mov	a,(_keyer_ticks + 1)
	subb	a,r4
	jc	00224$
	mov	r4,#0x02
	sjmp	00225$
00224$:
	mov	r4,#0x01
00225$:
	mov	ar6,r4
;	firmware/inc/cw/keyer_service.h:175: keyer_phase = KEYER_PHASE_IDLE;
	mov	_keyer_phase,#0x00
	ljmp	00187$
00183$:
;	firmware/inc/cw/keyer_service.h:179: if (((levers & KEYER_LEVER_DIT) != 0U) && ((keyer_phase == KEYER_PHASE_GAP) || keyer_is_element_dah)) {
	mov	a,r7
	jnb	acc.0,00149$
	mov	a,#0x03
	cjne	a,_keyer_phase,00712$
	sjmp	00148$
00712$:
	jnb	_keyer_is_element_dah,00149$
00148$:
;	firmware/inc/cw/keyer_service.h:180: keyer_has_dit_memory = true;
;	assignBit
	setb	_keyer_has_dit_memory
00149$:
;	firmware/inc/cw/keyer_service.h:182: if (((levers & KEYER_LEVER_DAH) != 0U) && ((keyer_phase == KEYER_PHASE_GAP) || !keyer_is_element_dah)) {
	mov	a,r7
	jnb	acc.1,00153$
	mov	a,#0x03
	cjne	a,_keyer_phase,00715$
	sjmp	00152$
00715$:
	jb	_keyer_is_element_dah,00153$
00152$:
;	firmware/inc/cw/keyer_service.h:183: keyer_has_dah_memory = true;
;	assignBit
	setb	_keyer_has_dah_memory
00153$:
;	firmware/inc/cw/keyer_service.h:185: if (levers == KEYER_LEVERS_BOTH) {
	cjne	r7,#0x03,00157$
;	firmware/inc/cw/keyer_service.h:186: keyer_was_squeezed = true;
;	assignBit
	setb	_keyer_was_squeezed
00157$:
;	firmware/inc/cw/keyer_service.h:188: --keyer_ticks;
	dec	_keyer_ticks
	mov	a,#0xff
	cjne	a,_keyer_ticks,00719$
	dec	(_keyer_ticks + 1)
00719$:
;	firmware/inc/cw/keyer_service.h:189: if (keyer_phase == KEYER_PHASE_ELEMENT) {
	mov	a,#0x02
	cjne	a,_keyer_phase,00180$
;	firmware/inc/cw/keyer_service.h:190: if (keyer_ticks == 0U) {
	mov	a,_keyer_ticks
	orl	a,(_keyer_ticks + 1)
	jnz	00161$
;	firmware/inc/cw/keyer_service.h:191: event = keyer_is_element_dah ? KEYER_EVENT_DAH : KEYER_EVENT_DIT;
	jnb	_keyer_is_element_dah,00226$
	mov	r4,#0x02
	sjmp	00227$
00226$:
	mov	r4,#0x01
00227$:
	mov	ar6,r4
;	firmware/inc/cw/keyer_service.h:192: keyer_ticks = keyer_dit_ticks;
	mov	dptr,#_keyer_dit_ticks
	movx	a,@dptr
	mov	_keyer_ticks,a
	inc	dptr
	movx	a,@dptr
	mov	(_keyer_ticks + 1),a
;	firmware/inc/cw/keyer_service.h:193: keyer_phase = KEYER_PHASE_GAP;
	mov	_keyer_phase,#0x03
	sjmp	00187$
00161$:
;	firmware/inc/cw/keyer_service.h:194: } else if (keyer_ticks <= KEYER_EARLY_RELEASE_TICKS) {
	mov	r3,_keyer_ticks
	mov	r4,(_keyer_ticks + 1)
	clr	c
	mov	a,#0x20
	subb	a,r3
	clr	a
	subb	a,r4
	jc	00187$
;	free result
;	firmware/inc/cw/keyer_service.h:196: KEYER_SIDETONE_OFF();
;	assignBit
	clr	_sidetone_is_on_requested
	sjmp	00187$
00180$:
;	firmware/inc/cw/keyer_service.h:198: } else if (keyer_ticks == 0U) {
	mov	a,_keyer_ticks
	orl	a,(_keyer_ticks + 1)
	jnz	00187$
;	firmware/inc/cw/keyer_service.h:200: wanted = levers;
	mov	ar4,r7
;	firmware/inc/cw/keyer_service.h:201: if (keyer_has_dit_memory) {
	jnb	_keyer_has_dit_memory,00164$
;	firmware/inc/cw/keyer_service.h:202: wanted |= KEYER_LEVER_DIT;
	mov	a,#0x01
	orl	a,r7
	mov	r4,a
00164$:
;	firmware/inc/cw/keyer_service.h:204: if (keyer_has_dah_memory) {
	jnb	_keyer_has_dah_memory,00166$
;	firmware/inc/cw/keyer_service.h:205: wanted |= KEYER_LEVER_DAH;
	orl	ar4,#0x02
00166$:
;	firmware/inc/cw/keyer_service.h:207: keyer_phase = KEYER_PHASE_IDLE;
	mov	_keyer_phase,#0x00
;	firmware/inc/cw/keyer_service.h:208: if (keyer_is_iambic_a && keyer_was_squeezed && (levers == 0U)) {
	jnb	_keyer_is_iambic_a,00173$
	jnb	_keyer_was_squeezed,00173$
	mov	a,r7
	jz	00174$
00173$:
;	firmware/inc/cw/keyer_service.h:210: } else if ((wanted & (keyer_is_element_dah ? KEYER_LEVER_DIT : KEYER_LEVER_DAH)) != 0U) {
	jnb	_keyer_is_element_dah,00228$
	mov	r3,#0x01
	sjmp	00229$
00228$:
	mov	r3,#0x02
00229$:
	mov	ar2,r4
	mov	a,r3
	anl	a,r2
	jz	00170$
;	firmware/inc/cw/keyer_service.h:211: start = keyer_is_element_dah ? KEYER_LEVER_DIT : KEYER_LEVER_DAH;
	jnb	_keyer_is_element_dah,00230$
	mov	r3,#0x01
	sjmp	00231$
00230$:
	mov	r3,#0x02
00231$:
	mov	ar5,r3
	sjmp	00174$
00170$:
;	firmware/inc/cw/keyer_service.h:212: } else if (wanted != 0U) {
	mov	a,r4
	jz	00174$
;	firmware/inc/cw/keyer_service.h:213: start = keyer_is_element_dah ? KEYER_LEVER_DAH : KEYER_LEVER_DIT;
	jnb	_keyer_is_element_dah,00232$
	mov	r4,#0x02
	sjmp	00233$
00232$:
	mov	r4,#0x01
00233$:
	mov	ar5,r4
00174$:
;	firmware/inc/cw/keyer_service.h:215: keyer_has_dit_memory = false;
;	assignBit
	clr	_keyer_has_dit_memory
;	firmware/inc/cw/keyer_service.h:216: keyer_has_dah_memory = false;
;	assignBit
	clr	_keyer_has_dah_memory
;	firmware/inc/cw/keyer_service.h:217: keyer_was_squeezed = false;
;	assignBit
	clr	_keyer_was_squeezed
00187$:
;	firmware/inc/cw/keyer_service.h:220: if (start != 0U) {
	mov	a,r5
	jz	00196$
;	firmware/inc/cw/keyer_service.h:221: keyer_is_element_dah = (start == KEYER_LEVER_DAH);
	cjne	r5,#0x02,00737$
	setb	c
	sjmp	00738$
00737$:
	clr	c
00738$:
;	firmware/inc/cw/keyer_service.h:223: if ((levers & (keyer_is_element_dah ? KEYER_LEVER_DIT : KEYER_LEVER_DAH)) != 0U) {
	mov	_keyer_is_element_dah,c
	jnc	00234$
	mov	r5,#0x01
	sjmp	00235$
00234$:
	mov	r5,#0x02
00235$:
	mov	ar4,r7
	mov	a,r5
	anl	a,r4
	jz	00192$
;	firmware/inc/cw/keyer_service.h:224: if (keyer_is_element_dah) {
	jnb	_keyer_is_element_dah,00189$
;	firmware/inc/cw/keyer_service.h:225: keyer_has_dit_memory = true;
;	assignBit
	setb	_keyer_has_dit_memory
	sjmp	00192$
00189$:
;	firmware/inc/cw/keyer_service.h:227: keyer_has_dah_memory = true;
;	assignBit
	setb	_keyer_has_dah_memory
00192$:
;	firmware/inc/cw/keyer_service.h:230: if (levers == KEYER_LEVERS_BOTH) {
	cjne	r7,#0x03,00194$
;	firmware/inc/cw/keyer_service.h:231: keyer_was_squeezed = true;
;	assignBit
	setb	_keyer_was_squeezed
00194$:
;	firmware/inc/cw/keyer_service.h:233: keyer_phase = KEYER_PHASE_ELEMENT;
	mov	_keyer_phase,#0x02
;	firmware/inc/cw/keyer_service.h:234: keyer_ticks = keyer_is_element_dah ? keyer_dah_ticks : keyer_dit_ticks;
	jnb	_keyer_is_element_dah,00236$
	mov	dptr,#_keyer_dah_ticks
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	sjmp	00237$
00236$:
	mov	dptr,#_keyer_dit_ticks
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
00237$:
	mov	_keyer_ticks,r5
	mov	(_keyer_ticks + 1),r7
;	firmware/inc/cw/keyer_service.h:235: KEYER_SIDETONE_ON();
;	assignBit
	setb	_sidetone_is_on_requested
;	firmware/inc/cw/keyer_service.h:236: keyer_is_char_gap_pending = false;
;	assignBit
	clr	_keyer_is_char_gap_pending
;	firmware/inc/cw/keyer_service.h:237: keyer_is_word_gap_pending = false;
;	assignBit
	clr	_keyer_is_word_gap_pending
00196$:
;	firmware/inc/cw/keyer_service.h:239: if (event != KEYER_EVENT_NONE) {
	mov	a,r6
	jz	00198$
;	firmware/inc/cw/keyer_service.h:241: keyer_space_ticks = 0U;
	clr	a
	mov	_keyer_space_ticks,a
	mov	(_keyer_space_ticks + 1),a
;	firmware/inc/cw/keyer_service.h:242: keyer_is_char_gap_pending = true;
;	assignBit
	setb	_keyer_is_char_gap_pending
;	firmware/inc/cw/keyer_service.h:243: keyer_is_word_gap_pending = true;
;	assignBit
	setb	_keyer_is_word_gap_pending
00198$:
;	firmware/inc/cw/keyer_service.h:246: if (keyer_is_word_gap_pending) {
	jnb	_keyer_is_word_gap_pending,00205$
;	firmware/inc/cw/keyer_service.h:247: ++keyer_space_ticks;
	inc	_keyer_space_ticks
	clr	a
	cjne	a,_keyer_space_ticks,00747$
	inc	(_keyer_space_ticks + 1)
00747$:
;	firmware/inc/cw/keyer_service.h:248: if (keyer_is_char_gap_pending && (keyer_space_ticks == keyer_two_dit_ticks)) {
	jnb	_keyer_is_char_gap_pending,00200$
	mov	dptr,#_keyer_two_dit_ticks
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	a,r5
	cjne	a,_keyer_space_ticks,00200$
	mov	a,r7
	cjne	a,(_keyer_space_ticks + 1),00200$
;	firmware/inc/cw/keyer_service.h:249: event = KEYER_EVENT_CHAR_GAP;
	mov	r6,#0x03
;	firmware/inc/cw/keyer_service.h:250: keyer_is_char_gap_pending = false;
;	assignBit
	clr	_keyer_is_char_gap_pending
00200$:
;	firmware/inc/cw/keyer_service.h:252: if (keyer_space_ticks == keyer_word_gap_ticks) {
	mov	dptr,#_keyer_word_gap_ticks
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	a,r5
	cjne	a,_keyer_space_ticks,00205$
	mov	a,r7
	cjne	a,(_keyer_space_ticks + 1),00205$
;	firmware/inc/cw/keyer_service.h:253: event = KEYER_EVENT_WORD_GAP;
	mov	r6,#0x04
;	firmware/inc/cw/keyer_service.h:254: keyer_is_word_gap_pending = false;
;	assignBit
	clr	_keyer_is_word_gap_pending
00205$:
;	firmware/inc/cw/keyer_service.h:259: if ((event != KEYER_EVENT_NONE) && !keyer_is_overflow) {
	mov	a,r6
	jz	00212$
	jb	_keyer_is_overflow,00212$
;	firmware/inc/cw/keyer_service.h:260: if ((uint8_t)(keyer_queue_head - keyer_queue_tail) >= KEYER_QUEUE_SIZE) {
	mov	a,_keyer_queue_head
	clr	c
	subb	a,_keyer_queue_tail
	mov	r7,a
	cjne	r7,#0x08,00755$
00755$:
	jc	00207$
;	firmware/inc/cw/keyer_service.h:261: keyer_is_overflow = true;
;	assignBit
	setb	_keyer_is_overflow
	ret
00207$:
;	firmware/inc/cw/keyer_service.h:263: keyer_queue[keyer_queue_head & KEYER_QUEUE_MASK] = event;
	mov	a,#0x07
	anl	a,_keyer_queue_head
	add	a, #_keyer_queue
	mov	dpl,a
	clr	a
	addc	a,#(_keyer_queue >> 8)
	mov	dph,a
	mov	a,r6
	movx	@dptr,a
;	firmware/inc/cw/keyer_service.h:264: ++keyer_queue_head;
	inc	_keyer_queue_head
00212$:
;	firmware/inc/cw/keyer_service.h:267: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'player_service'
;------------------------------------------------------------
;	firmware/inc/cw/player_service.h:35: static void player_service(void) {
;	-----------------------------------------
;	 function player_service
;	-----------------------------------------
_player_service:
;	firmware/inc/cw/player_service.h:36: if (!player_is_enabled) {
	jb	_player_is_enabled,00102$
;	firmware/inc/cw/player_service.h:37: return;
	ret
00102$:
;	firmware/inc/cw/player_service.h:39: if (player_is_start_requested) {
;	firmware/inc/cw/player_service.h:40: player_is_start_requested = false;
;	assignBit
	jbc	_player_is_start_requested,00188$
	sjmp	00104$
00188$:
;	firmware/inc/cw/player_service.h:41: player_index = 0U;
	mov	_player_index,#0x00
;	firmware/inc/cw/player_service.h:42: player_code = player_codes[0];
	mov	dptr,#_player_codes
	movx	a,@dptr
	mov	_player_code,a
;	firmware/inc/cw/player_service.h:44: player_is_mark = false;
;	assignBit
	clr	_player_is_mark
;	firmware/inc/cw/player_service.h:45: player_ticks = 1U;
	mov	_player_ticks,#0x01
	mov	(_player_ticks + 1),#0x00
00104$:
;	firmware/inc/cw/player_service.h:47: --player_ticks;
	dec	_player_ticks
	mov	a,#0xff
	cjne	a,_player_ticks,00189$
	dec	(_player_ticks + 1)
00189$:
;	firmware/inc/cw/player_service.h:48: if (player_is_mark) {
	jnb	_player_is_mark,00118$
;	firmware/inc/cw/player_service.h:49: if (player_ticks == 0U) {
	mov	a,_player_ticks
	orl	a,(_player_ticks + 1)
	jnz	00113$
;	firmware/inc/cw/player_service.h:50: player_code >>= 1;
	mov	a,_player_code
	clr	c
	rrc	a
	mov	_player_code,a
;	firmware/inc/cw/player_service.h:51: if (player_code != PLAYER_CODE_DONE) {
	mov	a,#0x01
	cjne	a,_player_code,00192$
	sjmp	00108$
00192$:
;	firmware/inc/cw/player_service.h:52: player_is_mark = false;
;	assignBit
	clr	_player_is_mark
;	firmware/inc/cw/player_service.h:53: player_ticks = player_dit_ticks;
	mov	dptr,#_player_dit_ticks
	movx	a,@dptr
	mov	_player_ticks,a
	inc	dptr
	movx	a,@dptr
	mov	(_player_ticks + 1),a
	ret
00108$:
;	firmware/inc/cw/player_service.h:55: ++player_index;
	inc	_player_index
;	firmware/inc/cw/player_service.h:56: if (player_index == player_code_count) {
	mov	dptr,#_player_code_count
	movx	a,@dptr
	cjne	a,_player_index,00106$
;	firmware/inc/cw/player_service.h:57: player_is_enabled = false;
;	assignBit
	clr	_player_is_enabled
;	firmware/inc/cw/player_service.h:58: return;
	ret
00106$:
;	firmware/inc/cw/player_service.h:60: player_code = player_codes[player_index];
	mov	a,_player_index
	add	a, #_player_codes
	mov	dpl,a
	clr	a
	addc	a, #(_player_codes >> 8)
	mov	dph,a
	movx	a,@dptr
	mov	_player_code,a
;	firmware/inc/cw/player_service.h:61: player_is_mark = false;
;	assignBit
	clr	_player_is_mark
;	firmware/inc/cw/player_service.h:62: player_ticks = player_char_gap_ticks;
	mov	dptr,#_player_char_gap_ticks
	movx	a,@dptr
	mov	_player_ticks,a
	inc	dptr
	movx	a,@dptr
	mov	(_player_ticks + 1),a
	ret
00113$:
;	firmware/inc/cw/player_service.h:64: } else if (player_ticks <= PLAYER_EARLY_RELEASE_TICKS) {
	mov	r6,_player_ticks
	mov	r7,(_player_ticks + 1)
	clr	c
	mov	a,#0x20
	subb	a,r6
	clr	a
	subb	a,r7
	jc	00120$
;	free result
;	firmware/inc/cw/player_service.h:66: PLAYER_SIDETONE_OFF();
;	assignBit
	clr	_sidetone_is_on_requested
	ret
00118$:
;	firmware/inc/cw/player_service.h:68: } else if (player_ticks == 0U) {
	mov	a,_player_ticks
	orl	a,(_player_ticks + 1)
	jnz	00120$
;	firmware/inc/cw/player_service.h:69: player_is_mark = true;
;	assignBit
	setb	_player_is_mark
;	firmware/inc/cw/player_service.h:70: player_ticks = ((player_code & 1U) != 0U) ? player_dah_ticks : player_dit_ticks;
	mov	a,_player_code
	jnb	acc.0,00122$
	mov	dptr,#_player_dah_ticks
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	sjmp	00123$
00122$:
	mov	dptr,#_player_dit_ticks
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
00123$:
	mov	_player_ticks,r6
	mov	(_player_ticks + 1),r7
;	firmware/inc/cw/player_service.h:71: PLAYER_SIDETONE_ON();
;	assignBit
	setb	_sidetone_is_on_requested
00120$:
;	firmware/inc/cw/player_service.h:73: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'sidetone_timer0_isr'
;------------------------------------------------------------
;	firmware/src/sidetone/sidetone.c:248: void sidetone_timer0_isr(void) __interrupt(TIMER0_INTERRUPT) {
;	-----------------------------------------
;	 function sidetone_timer0_isr
;	-----------------------------------------
_sidetone_timer0_isr:
	push	bits
	push	acc
	push	b
	push	dpl
	push	dph
	push	(0+7)
	push	(0+6)
	push	(0+5)
	push	(0+4)
	push	(0+3)
	push	(0+2)
	push	(0+1)
	push	(0+0)
	push	psw
	mov	psw,#0x00
;	firmware/src/sidetone/sidetone.c:249: keyer_service();
	lcall	_keyer_service
;	firmware/src/sidetone/sidetone.c:250: player_service();
	lcall	_player_service
;	firmware/src/sidetone/sidetone.c:251: sidetone_service();
	lcall	_sidetone_service
;	firmware/src/sidetone/sidetone.c:252: }
	pop	psw
	pop	(0+0)
	pop	(0+1)
	pop	(0+2)
	pop	(0+3)
	pop	(0+4)
	pop	(0+5)
	pop	(0+6)
	pop	(0+7)
	pop	dph
	pop	dpl
	pop	b
	pop	acc
	pop	bits
	reti
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area CONST   (CODE)
_sidetone_duty_permille:
	.byte #0x00, #0x00	; 0
	.byte #0x01, #0x00	; 1
	.byte #0x05, #0x00	; 5
	.byte #0x0b, #0x00	; 11
	.byte #0x13, #0x00	; 19
	.byte #0x1e, #0x00	; 30
	.byte #0x2a, #0x00	; 42
	.byte #0x39, #0x00	; 57
	.byte #0x49, #0x00	; 73
	.byte #0x5b, #0x00	; 91
	.byte #0x6f, #0x00	; 111
	.byte #0x84, #0x00	; 132
	.byte #0x9a, #0x00	; 154
	.byte #0xb1, #0x00	; 177
	.byte #0xc9, #0x00	; 201
	.byte #0xe1, #0x00	; 225
	.byte #0xfa, #0x00	; 250
	.byte #0x13, #0x01	; 275
	.byte #0x2b, #0x01	; 299
	.byte #0x43, #0x01	; 323
	.byte #0x5a, #0x01	; 346
	.byte #0x70, #0x01	; 368
	.byte #0x85, #0x01	; 389
	.byte #0x99, #0x01	; 409
	.byte #0xab, #0x01	; 427
	.byte #0xbb, #0x01	; 443
	.byte #0xca, #0x01	; 458
	.byte #0xd6, #0x01	; 470
	.byte #0xe1, #0x01	; 481
	.byte #0xe9, #0x01	; 489
	.byte #0xef, #0x01	; 495
	.byte #0xf3, #0x01	; 499
	.byte #0xf4, #0x01	; 500
	.area CSEG    (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
