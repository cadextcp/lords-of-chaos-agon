; void emu_exit(uint8_t code)
; Writes `code` to I/O port 0; fab-agon-emulator then terminates with it.
; First waits until UART0 has sent everything (LSR bit 6 = TEMT) and then
; idles a little, so the last printf lines reach the (fake) VDP before the
; emulator shuts down.

	.assume ADL=1
	.text
	.global _emu_exit

UART0_LSR:	equ $C5

_emu_exit:
	ld iy, 0
	add iy, sp
	ld c, (iy+3)		; first argument (after 24-bit return address)
wait_tx:
	in0 a, (UART0_LSR)
	bit 6, a
	jr z, wait_tx
	ld de, 600000		; ~0.3 s at 18.432 MHz (CI needs the headroom)
settle:
	dec de
	ld hl, 0
	or a
	sbc hl, de		; 24-bit compare: loop until DE == 0
	jr nz, settle
	ld a, c
	out (0), a
	ret
