;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (Linux)
;--------------------------------------------------------
	.module lcd_spi
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
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
	.globl _AUXINTIF
	.globl _IE2
	.globl _T3L
	.globl _T3H
	.globl _T4T3M
	.globl _P2M0
	.globl _P2M1
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
	.globl _lcd_spi_write_repeat_PARM_2
	.globl _lcd_spi_write_code_PARM_2
	.globl _lcd_spi_write_xdata_PARM_2
	.globl _lcd_spi_init
	.globl _lcd_spi_set_reset
	.globl _lcd_spi_select
	.globl _lcd_spi_deselect
	.globl _lcd_spi_write_byte
	.globl _lcd_spi_write_xdata
	.globl _lcd_spi_write_code
	.globl _lcd_spi_write_repeat
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
; internal ram data
;--------------------------------------------------------
	.area DSEG    (DATA)
_lcd_spi_is_selected:
	.ds 1
_lcd_spi_write_xdata_PARM_2:
	.ds 1
_lcd_spi_write_code_PARM_2:
	.ds 1
_lcd_spi_write_repeat_PARM_2:
	.ds 1
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
	.area	OSEG    (OVR,DATA)
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
;Allocation info for local variables in function 'lcd_spi_init'
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:8: void lcd_spi_init(void) {
;	-----------------------------------------
;	 function lcd_spi_init
;	-----------------------------------------
_lcd_spi_init:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	firmware/src/lcd/lcd_spi.c:10: LCD_CS_HIGH();
;	assignBit
	mov	_P2,#0xff
	setb	_P2_0
;	firmware/src/lcd/lcd_spi.c:11: LCD_SCK_HIGH();
;	assignBit
	setb	_P2_3
;	firmware/src/lcd/lcd_spi.c:12: LCD_A0_HIGH();
;	assignBit
	setb	_P2_2
;	firmware/src/lcd/lcd_spi.c:13: LCD_SDA_WRITE(0);
;	assignBit
	clr	_P2_4
;	firmware/src/lcd/lcd_spi.c:14: LCD_RST_LOW();
;	assignBit
	clr	_P2_1
;	firmware/src/lcd/lcd_spi.c:15: LCD_PINS_CONFIGURE_OUTPUTS();
	orl	_P2M0,#0x1f
	anl	_P2M1,#0xe0
;	firmware/src/lcd/lcd_spi.c:16: lcd_spi_is_selected = false;
	mov	_lcd_spi_is_selected,#0x00
;	firmware/src/lcd/lcd_spi.c:17: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_spi_set_reset'
;------------------------------------------------------------
;is_asserted   Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:19: void lcd_spi_set_reset(bool is_asserted) {
;	-----------------------------------------
;	 function lcd_spi_set_reset
;	-----------------------------------------
_lcd_spi_set_reset:
;	firmware/src/lcd/lcd_spi.c:20: if (is_asserted) {
	mov	a,dpl
	jz	00102$
;	firmware/src/lcd/lcd_spi.c:21: LCD_RST_LOW();
;	assignBit
	clr	_P2_1
	ret
00102$:
;	firmware/src/lcd/lcd_spi.c:23: LCD_RST_HIGH();
;	assignBit
	setb	_P2_1
;	firmware/src/lcd/lcd_spi.c:25: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_spi_select'
;------------------------------------------------------------
;kind          Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:27: void lcd_spi_select(uint8_t kind) {
;	-----------------------------------------
;	 function lcd_spi_select
;	-----------------------------------------
_lcd_spi_select:
;	firmware/src/lcd/lcd_spi.c:28: if (kind == LCD_SPI_COMMAND) {
	mov	a,dpl
	mov	r7,a
	jnz	00102$
;	firmware/src/lcd/lcd_spi.c:29: LCD_A0_LOW();
;	assignBit
	clr	_P2_2
	sjmp	00103$
00102$:
;	firmware/src/lcd/lcd_spi.c:31: LCD_A0_HIGH();
;	assignBit
	setb	_P2_2
00103$:
;	firmware/src/lcd/lcd_spi.c:33: if (!lcd_spi_is_selected) {
	mov	a,_lcd_spi_is_selected
	jnz	00105$
;	firmware/src/lcd/lcd_spi.c:34: LCD_CS_LOW();
;	assignBit
	clr	_P2_0
;	firmware/src/lcd/lcd_spi.c:35: lcd_spi_is_selected = true;
	mov	_lcd_spi_is_selected,#0x01
00105$:
;	firmware/src/lcd/lcd_spi.c:38: LCD_PACE();
	nop
	nop
	nop
;	firmware/src/lcd/lcd_spi.c:39: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_spi_deselect'
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:41: void lcd_spi_deselect(void) {
;	-----------------------------------------
;	 function lcd_spi_deselect
;	-----------------------------------------
_lcd_spi_deselect:
;	firmware/src/lcd/lcd_spi.c:43: LCD_PACE();
	nop
	nop
	nop
;	firmware/src/lcd/lcd_spi.c:44: LCD_CS_HIGH();
;	assignBit
	setb	_P2_0
;	firmware/src/lcd/lcd_spi.c:45: lcd_spi_is_selected = false;
	mov	_lcd_spi_is_selected,#0x00
;	firmware/src/lcd/lcd_spi.c:46: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_spi_write_byte'
;------------------------------------------------------------
;value         Allocated to registers r7 
;bit_count     Allocated to registers r6 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:48: void lcd_spi_write_byte(uint8_t value) {
;	-----------------------------------------
;	 function lcd_spi_write_byte
;	-----------------------------------------
_lcd_spi_write_byte:
	mov	r7, dpl
;	firmware/src/lcd/lcd_spi.c:50: do {
	mov	r6,#0x08
00101$:
;	firmware/src/lcd/lcd_spi.c:52: LCD_SCK_LOW();
;	assignBit
	clr	_P2_3
;	firmware/src/lcd/lcd_spi.c:54: LCD_SDA_WRITE((bool)(value & 0x80U));
	mov	a,r7
	rlc	a
	mov	_P2_4,c
;	firmware/src/lcd/lcd_spi.c:55: LCD_PACE();
	nop
	nop
	nop
;	firmware/src/lcd/lcd_spi.c:56: LCD_SCK_HIGH();
;	assignBit
	setb	_P2_3
;	firmware/src/lcd/lcd_spi.c:57: value = (uint8_t)(value << 1);
	mov	a,r7
	add	a,r7
	mov	r7,a
;	firmware/src/lcd/lcd_spi.c:58: LCD_PACE();
	nop
	nop
	nop
;	firmware/src/lcd/lcd_spi.c:59: --bit_count;
;	firmware/src/lcd/lcd_spi.c:60: } while (bit_count != 0U);
	djnz	r6,00101$
;	firmware/src/lcd/lcd_spi.c:61: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_spi_write_xdata'
;------------------------------------------------------------
;length        Allocated with name '_lcd_spi_write_xdata_PARM_2'
;data          Allocated to registers 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:63: void lcd_spi_write_xdata(const MEM_XDATA uint8_t* data, uint8_t length) {
;	-----------------------------------------
;	 function lcd_spi_write_xdata
;	-----------------------------------------
_lcd_spi_write_xdata:
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/lcd/lcd_spi.c:64: while (length != 0U) {
	mov	r5,_lcd_spi_write_xdata_PARM_2
00101$:
	mov	a,r5
	jz	00104$
;	firmware/src/lcd/lcd_spi.c:65: lcd_spi_write_byte(*data);
	mov	dpl,r6
	mov	dph,r7
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	mov	r6,dpl
	mov	r7,dph
	mov	dpl, r4
	push	ar7
	push	ar6
	push	ar5
	lcall	_lcd_spi_write_byte
	pop	ar5
	pop	ar6
	pop	ar7
;	firmware/src/lcd/lcd_spi.c:66: ++data;
;	firmware/src/lcd/lcd_spi.c:67: --length;
	dec	r5
	sjmp	00101$
00104$:
;	firmware/src/lcd/lcd_spi.c:69: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_spi_write_code'
;------------------------------------------------------------
;length        Allocated with name '_lcd_spi_write_code_PARM_2'
;data          Allocated to registers 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:71: void lcd_spi_write_code(const MEM_CODE uint8_t* data, uint8_t length) {
;	-----------------------------------------
;	 function lcd_spi_write_code
;	-----------------------------------------
_lcd_spi_write_code:
	mov	r6, dpl
	mov	r7, dph
;	firmware/src/lcd/lcd_spi.c:72: while (length != 0U) {
	mov	r5,_lcd_spi_write_code_PARM_2
00101$:
	mov	a,r5
	jz	00104$
;	firmware/src/lcd/lcd_spi.c:73: lcd_spi_write_byte(*data);
	mov	dpl,r6
	mov	dph,r7
	clr	a
	movc	a,@a+dptr
	mov	r4,a
	inc	dptr
	mov	r6,dpl
	mov	r7,dph
	mov	dpl, r4
	push	ar7
	push	ar6
	push	ar5
	lcall	_lcd_spi_write_byte
	pop	ar5
	pop	ar6
	pop	ar7
;	firmware/src/lcd/lcd_spi.c:74: ++data;
;	firmware/src/lcd/lcd_spi.c:75: --length;
	dec	r5
	sjmp	00101$
00104$:
;	firmware/src/lcd/lcd_spi.c:77: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_spi_write_repeat'
;------------------------------------------------------------
;count         Allocated with name '_lcd_spi_write_repeat_PARM_2'
;value         Allocated to registers r7 
;------------------------------------------------------------
;	firmware/src/lcd/lcd_spi.c:79: void lcd_spi_write_repeat(uint8_t value, uint8_t count) {
;	-----------------------------------------
;	 function lcd_spi_write_repeat
;	-----------------------------------------
_lcd_spi_write_repeat:
	mov	r7, dpl
;	firmware/src/lcd/lcd_spi.c:80: while (count != 0U) {
	mov	r6,_lcd_spi_write_repeat_PARM_2
00101$:
	mov	a,r6
	jz	00104$
;	firmware/src/lcd/lcd_spi.c:81: lcd_spi_write_byte(value);
	mov	dpl, r7
	push	ar7
	push	ar6
	lcall	_lcd_spi_write_byte
	pop	ar6
	pop	ar7
;	firmware/src/lcd/lcd_spi.c:82: --count;
	dec	r6
	sjmp	00101$
00104$:
;	firmware/src/lcd/lcd_spi.c:84: }
	ret
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
