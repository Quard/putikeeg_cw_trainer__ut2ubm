;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module main
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _tick_init
	.globl _sidetone_poll
	.globl _sidetone_init
	.globl _settings_use_display_defaults
	.globl _settings_get
	.globl _settings_load
	.globl _progress_load
	.globl _menu_show_message
	.globl _menu_render
	.globl _menu_needs_redraw
	.globl _menu_poll
	.globl _menu_handle
	.globl _menu_init
	.globl _lcd_flush_step
	.globl _lcd_set_contrast
	.globl _lcd_init
	.globl _gfx_init
	.globl _eeprom_init
	.globl _keyer_init
	.globl _buttons_poll
	.globl _buttons_is_recovery_start
	.globl _buttons_init
	.globl _backlight_init
	.globl _P5_5
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
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
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
; Stack segment in internal ram
;--------------------------------------------------------
	.area SSEG
__start__stack:
	.ds	1

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
; interrupt vector
;--------------------------------------------------------
	.area HOME    (CODE)
__interrupt_vect:
	ljmp	__sdcc_gsinit_startup
	reti
	.ds	7
	ljmp	_sidetone_timer0_isr
	.ds	5
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	reti
	.ds	7
	ljmp	_tick_isr
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area HOME    (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area GSINIT  (CODE)
	.globl __sdcc_gsinit_startup
	.globl __sdcc_program_startup
	.globl __start__stack
	.globl __mcs51_genXINIT
	.globl __mcs51_genXRAMCLEAR
	.globl __mcs51_genRAMCLEAR
	.area GSFINAL (CODE)
	ljmp	__sdcc_program_startup
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area HOME    (CODE)
	.area HOME    (CODE)
__sdcc_program_startup:
	lcall	_main
__sdcc_program_exit:
	sjmp	.
;	return from main will return to caller
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area CSEG    (CODE)
;------------------------------------------------------------
;Allocation info for local variables in function 'main'
;------------------------------------------------------------
;event_count   Allocated to registers r7 
;event         Allocated to registers r5 
;------------------------------------------------------------
;	firmware/src/main.c:19: void main(void) {
;	-----------------------------------------
;	 function main
;	-----------------------------------------
_main:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	firmware/src/main.c:23: tick_init();
	lcall	_tick_init
;	firmware/src/main.c:25: EA = 1;
;	assignBit
	setb	_EA
;	firmware/src/main.c:26: eeprom_init();
	lcall	_eeprom_init
;	firmware/src/main.c:28: buttons_init();
	lcall	_buttons_init
;	firmware/src/main.c:29: (void)settings_load();
	lcall	_settings_load
;	firmware/src/main.c:30: (void)progress_load();
	lcall	_progress_load
;	firmware/src/main.c:31: if (buttons_is_recovery_start()) {
	lcall	_buttons_is_recovery_start
	mov	a, dpl
	jz	00102$
;	firmware/src/main.c:32: settings_use_display_defaults();
	lcall	_settings_use_display_defaults
00102$:
;	firmware/src/main.c:34: sidetone_init(settings_get(SETTINGS_ID_TONE_HZ));
	mov	dpl, #0x00
	lcall	_settings_get
	lcall	_sidetone_init
;	firmware/src/main.c:35: keyer_init();
	lcall	_keyer_init
;	firmware/src/main.c:36: lcd_init();
	lcall	_lcd_init
;	firmware/src/main.c:37: lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
	mov	dpl, #0x09
	lcall	_settings_get
	lcall	_lcd_set_contrast
;	firmware/src/main.c:38: backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
	mov	dpl, #0x08
	lcall	_settings_get
	lcall	_backlight_init
;	firmware/src/main.c:39: gfx_init();
	lcall	_gfx_init
;	firmware/src/main.c:40: menu_init();
	lcall	_menu_init
;	firmware/src/main.c:41: if (buttons_is_recovery_start()) {
	lcall	_buttons_is_recovery_start
	mov	a, dpl
	jz	00126$
;	firmware/src/main.c:42: menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
	mov	dpl, #0x02
	lcall	_menu_show_message
;	firmware/src/main.c:46: for (event_count = 0U; event_count < MAIN_EVENTS_PER_PASS; ++event_count) {
00126$:
	mov	r7,#0x00
00111$:
;	firmware/src/main.c:47: event = buttons_poll();
	push	ar7
	lcall	_buttons_poll
	mov	r6, dpl
	pop	ar7
;	firmware/src/main.c:48: if (event == BUTTON_EVENT_NONE) {
	mov	a,r6
	mov	r5,a
	jz	00107$
;	firmware/src/main.c:51: menu_handle(event);
	mov	dpl, r5
	push	ar7
	lcall	_menu_handle
	pop	ar7
;	firmware/src/main.c:46: for (event_count = 0U; event_count < MAIN_EVENTS_PER_PASS; ++event_count) {
	inc	r7
	cjne	r7,#0x04,00163$
00163$:
	jc	00111$
00107$:
;	firmware/src/main.c:53: menu_poll();
	lcall	_menu_poll
;	firmware/src/main.c:54: sidetone_poll();
	lcall	_sidetone_poll
;	firmware/src/main.c:55: if (menu_needs_redraw()) {
	lcall	_menu_needs_redraw
	mov	a, dpl
	jz	00109$
;	firmware/src/main.c:56: menu_render();
	lcall	_menu_render
00109$:
;	firmware/src/main.c:58: lcd_flush_step();
	lcall	_lcd_flush_step
;	firmware/src/main.c:60: }
	sjmp	00126$
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
