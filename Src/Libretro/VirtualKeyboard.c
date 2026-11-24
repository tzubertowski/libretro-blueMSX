/* VirtualKeyboard.c — on-screen MSX keyboard overlay for gamepad-only
 * frontends (GB350/picoarch).
 *
 * The core's normal input path maps the RetroPad to the MSX joystick and
 * expects the frontend to provide a RETRO_DEVICE_KEYBOARD. Minimal frontends
 * (picoarch) provide joypad only, so MSX BASIC prompts, cassette loading
 * (RUN"CAS:) and keyboard-driven games are unreachable. This overlay renders
 * a keyboard panel over the emulated frame and drives the same eventMap the
 * frontend keyboard would.
 *
 * v2 layout mirrors the real MSX keyboard (14 columns x 6 rows, RGB565):
 *   row0  F1 F2 F3 F4 F5 | STP CLS SEL INS DEL
 *   row1  1 2 3 4 5 6 7 8 9 0 - ^ \ _
 *   row2  Q W E R T Y U I O P @ [
 *   row3  CTL(2) A S D F G H J K L ; : ]
 *   row4  CAP(2) SHF(2) Z X C V B N M , . /
 *   row5  ESC TAB GRA BKS <- ^ v -> SPACE(6)
 *
 * v2 font fix: glyphs are looked up through VKBD_FONT_CHARS (strchr), not by
 * the contiguous-ASCII assumption `c - '0'` — v1 rendered every letter with
 * the wrong glyph (A showed H, S showed Z, SHIFT showed ZOPM@).
 *
 * Buttons while open: D-pad move, A = type focused key (hold = hold key),
 * B = close, X = Shift latch, Y = Graph latch, START = Return,
 * SELECT = Space, L+R = close. While open the RetroPad no longer drives the
 * emulated joystick. */

#include "libretro.h"
#include "VirtualKeyboard.h"
#include "InputEvent.h"
#include "vkbd_font.h"

#include <string.h>

#define VKBD_COLS 14
#define VKBD_ROWS 6

/* arrow pseudo-glyph codes used in labels (mapped in vk_text8) */
#define VK_CH_LEFT  "\x01"
#define VK_CH_UP    "\x02"
#define VK_CH_DOWN  "\x03"
#define VK_CH_RIGHT "\x04"

typedef struct {
	const char *label;   /* NULL = continuation of the previous cell */
	int code;            /* EC_* event code */
} VkbdKey;

/* "" = blank key; spanned cells carry NULL */
static const VkbdKey vkbd_grid[VKBD_ROWS][VKBD_COLS] = {
	{ { "F1", EC_F1 }, { "F2", EC_F2 }, { "F3", EC_F3 }, { "F4", EC_F4 },
	  { "F5", EC_F5 }, { "STP", EC_STOP }, { NULL, -1 }, { "CLS", EC_CLS },
	  { NULL, -1 }, { "SEL", EC_SELECT }, { NULL, -1 }, { "INS", EC_INS },
	  { NULL, -1 }, { "DEL", EC_DEL } },
	{ { "1", EC_1 }, { "2", EC_2 }, { "3", EC_3 }, { "4", EC_4 },
	  { "5", EC_5 }, { "6", EC_6 }, { "7", EC_7 }, { "8", EC_8 },
	  { "9", EC_9 }, { "0", EC_0 }, { "-", EC_NEG }, { "^", EC_CIRCFLX },
	  { "\\", EC_BKSLASH }, { "_", EC_UNDSCRE } },
	{ { "Q", EC_Q }, { "W", EC_W }, { "E", EC_E }, { "R", EC_R },
	  { "T", EC_T }, { "Y", EC_Y }, { "U", EC_U }, { "I", EC_I },
	  { "O", EC_O }, { "P", EC_P }, { "@", EC_AT }, { "[", EC_LBRACK },
	  { "", EC_NONE }, { "", EC_NONE } },
	{ { "CTL", EC_CTRL }, { NULL, -1 }, { "A", EC_A }, { "S", EC_S },
	  { "D", EC_D }, { "F", EC_F }, { "G", EC_G }, { "H", EC_H },
	  { "J", EC_J }, { "K", EC_K }, { "L", EC_L }, { ";", EC_SEMICOL },
	  { ":", EC_COLON }, { "]", EC_RBRACK } },
	{ { "CAP", EC_CAPS }, { NULL, -1 }, { "SHF", EC_LSHIFT }, { NULL, -1 },
	  { "Z", EC_Z }, { "X", EC_X }, { "C", EC_C }, { "V", EC_V },
	  { "B", EC_B }, { "N", EC_N }, { "M", EC_M }, { ",", EC_COMMA },
	  { ".", EC_PERIOD }, { "/", EC_DIV } },
	{ { "ESC", EC_ESC }, { "TAB", EC_TAB }, { "GRA", EC_GRAPH },
	  { "BKS", EC_BKSPACE }, { VK_CH_LEFT, EC_LEFT }, { VK_CH_UP, EC_UP },
	  { VK_CH_DOWN, EC_DOWN }, { VK_CH_RIGHT, EC_RIGHT },
	  { "SPACE", EC_SPACE }, { NULL, -1 }, { NULL, -1 }, { NULL, -1 },
	  { NULL, -1 }, { NULL, -1 } },
};

/* key the cursor rests on when the panel opens: the space bar */
#define VK_HOME_ROW 5
#define VK_HOME_COL 8

static int vk_open;
static int vk_row;
static int vk_col;
static int vk_shift_latch;
static int vk_graph_latch;
static unsigned vk_prev_buttons;

void vkbd_init(void)
{
	vk_open = 0;
	vk_row = VK_HOME_ROW;
	vk_col = VK_HOME_COL;
	vk_shift_latch = 0;
	vk_graph_latch = 0;
	vk_prev_buttons = 0;
}

int vkbd_is_open(void)
{
	return vk_open;
}

static int cell_start(int row, int col)
{
	return vkbd_grid[row][col].label != NULL;
}

void vkbd_input(unsigned joypad, int *eventMap)
{
	unsigned pressed = joypad & ~vk_prev_buttons;

	/* L+R rising edge toggles the panel */
	if ((joypad & (1 << RETRO_DEVICE_ID_JOYPAD_L)) &&
	    (joypad & (1 << RETRO_DEVICE_ID_JOYPAD_R)) &&
	    (pressed & ((1 << RETRO_DEVICE_ID_JOYPAD_L) | (1 << RETRO_DEVICE_ID_JOYPAD_R)))) {
		vk_open = !vk_open;
		vk_row = VK_HOME_ROW;
		vk_col = VK_HOME_COL;
		vk_prev_buttons = joypad;
		return;
	}

	if (!vk_open) {
		vk_prev_buttons = joypad;
		return;
	}

	/* X / Y toggle modifier latches */
	if (pressed & (1 << RETRO_DEVICE_ID_JOYPAD_X))
		vk_shift_latch = !vk_shift_latch;
	if (pressed & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
		vk_graph_latch = !vk_graph_latch;

	/* D-pad moves the cursor */
	if (pressed & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) {
		do { vk_col--; if (vk_col < 0) { vk_col = VKBD_COLS - 1; vk_row = (vk_row + VKBD_ROWS - 1) % VKBD_ROWS; } }
		while (!cell_start(vk_row, vk_col));
	}
	if (pressed & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) {
		do { vk_col++; if (vk_col >= VKBD_COLS) { vk_col = 0; vk_row = (vk_row + 1) % VKBD_ROWS; } }
		while (!cell_start(vk_row, vk_col));
	}
	if (pressed & (1 << RETRO_DEVICE_ID_JOYPAD_UP)) {
		do { vk_row = (vk_row + VKBD_ROWS - 1) % VKBD_ROWS; }
		while (!cell_start(vk_row, vk_col));
	}
	if (pressed & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)) {
		do { vk_row = (vk_row + 1) % VKBD_ROWS; }
		while (!cell_start(vk_row, vk_col));
	}

	/* B closes */
	if (pressed & (1 << RETRO_DEVICE_ID_JOYPAD_B))
		vk_open = 0;

	/* A types the focused key (hold = key held) */
	if (joypad & (1 << RETRO_DEVICE_ID_JOYPAD_A)) {
		const VkbdKey *k = &vkbd_grid[vk_row][vk_col];
		if (k->code > EC_NONE)
			eventMap[k->code] = 1;
	}

	/* START = Return, SELECT = Space (hold) */
	if (joypad & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		eventMap[EC_RETURN] = 1;
	if (joypad & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
		eventMap[EC_SPACE] = 1;

	/* latched modifier keys held every frame */
	if (vk_shift_latch)
		eventMap[EC_LSHIFT] = 1;
	if (vk_graph_latch)
		eventMap[EC_GRAPH] = 1;

	vk_prev_buttons = joypad;
}

/* ── drawing ──────────────────────────────────────────────────────────── */

#define VK_COLOR_BG     0x1082  /* dark gray */
#define VK_COLOR_KEY    0x39E7  /* mid gray  */
#define VK_COLOR_TEXT   0xFFFF  /* white     */
#define VK_COLOR_LATCH  0xFFE0  /* yellow    */

#define VK_CELL_W 19
#define VK_CELL_H 11
#define VK_PANEL_W (VKBD_COLS * VK_CELL_W)              /* 266 @ 14 cols */
#define VK_PANEL_H (VKBD_ROWS * VK_CELL_H + 9)          /* + legend row  */

static void vk_fill(uint16_t *fb, int w, int h, int x0, int y0, int x1, int y1, uint16_t c)
{
	int x, y;
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 >= w) x1 = w - 1;
	if (y1 >= h) y1 = h - 1;
	for (y = y0; y <= y1; y++)
		for (x = x0; x <= x1; x++)
			fb[y * w + x] = c;
}

static int vk_glyph_index(unsigned char c)
{
	const char *p;
	if (c >= 1 && c <= 4)                       /* arrow pseudo-glyphs */
		return VKBD_FONT_COUNT + (c - 1);
	p = strchr(VKBD_FONT_CHARS, c);
	if (!p)
		return -1;
	return (int)(p - VKBD_FONT_CHARS);
}

static void vk_text8(uint16_t *fb, int w, int h, int x, int y, const char *s, uint16_t color)
{
	for (; *s; s++, x += 8) {
		int idx = vk_glyph_index((unsigned char)*s);
		int i, j;
		if (idx < 0 || idx >= VKBD_FONT_TOTAL)
			continue;
		for (j = 0; j < 8; j++) {
			unsigned char bits = vkbd_font[idx][j];
			for (i = 0; i < 8; i++)
				if (bits & (0x80 >> i)) {
					int px = x + i, py = y + j;
					if (px >= 0 && px < w && py >= 0 && py < h)
						fb[py * w + px] = color;
				}
		}
	}
}

void vkbd_draw(uint16_t *fb, int fb_width, int fb_height, int visible_bottom)
{
	int x0 = (fb_width - VK_PANEL_W) / 2;
	int y1 = visible_bottom;
	int y0 = y1 - VK_PANEL_H + 1;
	int r, c;

	if (y0 < 1)
		y0 = 1;
	if (x0 < 1)
		x0 = 1;

	vk_fill(fb, fb_width, fb_height, x0 - 2, y0 - 2, x0 + VK_PANEL_W + 1, y1 + 1, VK_COLOR_TEXT);
	vk_fill(fb, fb_width, fb_height, x0, y0, x0 + VK_PANEL_W - 1, y1, VK_COLOR_BG);

	/* legend row */
	vk_text8(fb, fb_width, fb_height, x0 + 2, y0 + 1,
		"A=TYPE B=EXIT SEL=SPACE STA=RET", VK_COLOR_TEXT);

	for (r = 0; r < VKBD_ROWS; r++) {
		for (c = 0; c < VKBD_COLS; c++) {
			const VkbdKey *k = &vkbd_grid[r][c];
			int focused = (r == vk_row && c == vk_col && cell_start(r, c));
			uint16_t keybg = VK_COLOR_KEY;
			uint16_t textc = VK_COLOR_TEXT;
			int span = 1;

			if (k->label == NULL)
				continue;
			while (c + span < VKBD_COLS && vkbd_grid[r][c + span].label == NULL)
				span++;

			{
				int cx = x0 + c * VK_CELL_W;
				int cy = y0 + 9 + r * VK_CELL_H;

				if (focused) {
					keybg = VK_COLOR_TEXT;
					textc = VK_COLOR_BG;
				}
				if ((k->code == EC_LSHIFT && vk_shift_latch) ||
				    (k->code == EC_GRAPH && vk_graph_latch)) {
					keybg = VK_COLOR_LATCH;
					textc = VK_COLOR_BG;
				}

				vk_fill(fb, fb_width, fb_height, cx + 1, cy + 1,
					cx + span * VK_CELL_W - 2, cy + VK_CELL_H - 2, keybg);

				if (k->label[0]) {
					int len = strlen(k->label);
					int tx = cx + (span * VK_CELL_W - len * 8) / 2;
					int ty = cy + (VK_CELL_H - 8) / 2;
					vk_text8(fb, fb_width, fb_height, tx, ty, k->label, textc);
				}
			}
		}
	}
}
