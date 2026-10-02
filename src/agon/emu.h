/*
 * Emulator control. fab-agon-emulator exits with code `code` when the
 * eZ80 writes it to I/O port 0. Only call this in headless test mode.
 */
#ifndef LOC_EMU_H
#define LOC_EMU_H

#include <stdint.h>

void emu_exit(uint8_t code);

#endif
