; SDCC data operands: expressions without a protected name are ordinary RAM writes.
	.area CSEG    (CODE)
	mov	(_tick_count_ms + 1),a
	mov	(_lcd_write_PARM_3 + 1),r7
	inc	(_x + 1)
	mov	_x,#0xa0
	xch	a,(_buffer + 2)
