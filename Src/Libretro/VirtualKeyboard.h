/* VirtualKeyboard.h — on-screen MSX keyboard overlay for gamepad-only
 * frontends (GB350/picoarch). Open with L+R; D-pad moves, A types the
 * focused key, B closes, X = Shift latch, Y = Graph latch,
 * START = Return (hold), SELECT = Space (hold). */
#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include <stdint.h>

void vkbd_init(void);
int  vkbd_is_open(void);

/* joypad: bitfield of RETRO_DEVICE_ID_JOYPAD_* bits for port 0.
 * eventMap: the core's EC_* event map; pressed keys are set to 1 for the
 * duration of the call's frame. Call once per frame, AFTER the regular
 * keyboard/joypad event fill, so overlay keys win. */
void vkbd_input(unsigned joypad, int *eventMap);

/* Draw the overlay into the RGB565 framebuffer. visible_bottom is the last
 * framebuffer row the frontend will actually display (bottom-aligns the
 * panel). Call after the emulator rendered the frame, before video_cb. */
void vkbd_draw(uint16_t *fb, int fb_width, int fb_height, int visible_bottom);

#endif
