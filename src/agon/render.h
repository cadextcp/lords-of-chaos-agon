/* VDP renderer: draws the core's dirty cells in MODE 8 using custom glyphs. */
#ifndef LOC_RENDER_H
#define LOC_RENDER_H

void render_init(void);
void render_flush(void);
void render_shutdown(void);

#endif
