//
// Copyright (c) 2004 SDLemu Team
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//

//////////////////////////////////////////////////////////////////////////////
//                       Handy/SDL - An Atari Lynx Emulator                 //
//                             Copyright (c) 2005                           //
//                                SDLemu Team                               //
//////////////////////////////////////////////////////////////////////////////
// handy_sdl_handling.h                                                     //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// Input handling. The Lynx pad is a bitmask, so the button state is polled //
// once per frame and rebuilt from scratch rather than accumulated across   //
// key events - a stray event can then never leave a button stuck on.       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#ifndef __HANDY_SDL_HANDLING_H__
#define __HANDY_SDL_HANDLING_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

// The Lynx controls, in the order the GUI lists them.
enum {
	HANDY_BTN_UP = 0,
	HANDY_BTN_DOWN,
	HANDY_BTN_LEFT,
	HANDY_BTN_RIGHT,
	HANDY_BTN_A,
	HANDY_BTN_B,
	HANDY_BTN_OPT1,
	HANDY_BTN_OPT2,
	HANDY_BTN_PAUSE,
	HANDY_BTN_COUNT
};

void        handy_sdl_input_init(void);
void        handy_sdl_input_close(void);

// Handle controller hot-plug. Returns 1 if the event was an input device
// arriving or leaving.
int         handy_sdl_input_event(SDL_Event *event);

// Read the keyboard and pad, and push the resulting mask into the emulation.
// "allow_keyboard" is cleared while the GUI has keyboard focus.
void        handy_sdl_input_poll(int allow_keyboard);

// Binding accessors, for the GUI remapping window.
const char *handy_sdl_input_name(int button);
SDL_Scancode handy_sdl_input_get_key(int button);
void        handy_sdl_input_set_key(int button, SDL_Scancode code);
int         handy_sdl_input_get_pad(int button);
void        handy_sdl_input_set_pad(int button, int pad_button);
void        handy_sdl_input_defaults(void);
void        handy_sdl_input_swap_ab(void);

// Name of the attached controller, or NULL if there is none.
const char *handy_sdl_input_pad_name(void);

#endif
