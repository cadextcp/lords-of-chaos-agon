/*
 * Key mapping by FabGL virtual key (kbuf vkey), measured in the keyboard
 * spike (issue #3, ADR 0007). Movement keys are mapped by vkey only: the
 * ascii field of key-UP events is stale (it repeats the last key down).
 */
#ifndef LOC_INPUT_H
#define LOC_INPUT_H

#include <stdint.h>

#define VK_ESC 0x7D
#define VK_UP 0x96
#define VK_DOWN 0x98
#define VK_LEFT 0x9A
#define VK_RIGHT 0x9C
#define VK_HOME 0x86
#define VK_END 0x88
#define VK_PGUP 0x93
#define VK_PGDN 0x95
/* Measured with loc --keytest (M2c): Tab/Shift+Tab share the vkey and
 * differ in kmod, Space is a vkey, not an ASCII hit. */
#define VK_TAB 0x8E
#define VK_SPACE 0x01
#define KMOD_SHIFT 0x02
/* FabGL: VK_a..VK_z = 0x16..0x2F (layout applied, e.g. German y/z swap) */
#define VK_LOWER(c) (0x16 + ((c) - 'a'))

/* ARROW_* bit for arrow keys and WASD, else 0. */
uint8_t input_arrow(uint8_t vkey);
/* Diagonal direction mask for Pos1/Bild-auf/Ende/Bild-ab (GDD 5.2), else 0. */
uint8_t input_diagonal(uint8_t vkey);

#endif
