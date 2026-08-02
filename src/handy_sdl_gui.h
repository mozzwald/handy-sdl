//
// Handy/SDL - Desktop GUI
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef HANDY_SDL_GUI_H
#define HANDY_SDL_GUI_H

#include <SDL.h>

// Bring up Dear ImGui on the existing window and renderer. Call after
// handy_sdl_video_setup(). Returns 1 on success.
int  handy_sdl_gui_init(void);

// Feed one SDL event to the GUI. Returns 1 if the GUI consumed it, in which
// case the emulator should ignore it.
int  handy_sdl_gui_event(SDL_Event *event);

// Build this frame's widgets. Call once per frame before handy_sdl_present().
void handy_sdl_gui_frame(void);

// Draw callback to hand to handy_sdl_present().
void handy_sdl_gui_draw(void);

// True when the GUI wants the keyboard, i.e. a text field has focus. The
// emulator must not read input while this holds or typing into a field would
// also drive the game.
int  handy_sdl_gui_wants_keyboard(void);

void handy_sdl_gui_close(void);

#endif
