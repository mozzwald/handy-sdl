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
//                                                                          //
//                          Based upon Handy v0.90 WIN32                    //
//                            Copyright (c) 1996,1997                       //
//                                  K. Wilkins                              //
//////////////////////////////////////////////////////////////////////////////
// handy_sdl_graphics.h                                                     //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// Video output for Handy/SDL, built on the SDL2 render API.                //
//                                                                          //
// The Lynx framebuffer is a single streaming texture at native resolution. //
// SDL_RenderSetLogicalSize() scales it to whatever size the window happens //
// to be, on the GPU and with the aspect ratio preserved, so resizing needs //
// no handling of its own.                                                  //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#ifndef __HANDY_SDL_GRAPHICS_H__
#define __HANDY_SDL_GRAPHICS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

extern SDL_Window	*mainWindow;
extern SDL_Renderer	*mainRenderer;
extern SDL_Texture	*lynxTexture;

// Create the window, renderer and texture. "scale" only picks the initial
// window size; the user is free to drag it to anything afterwards.
int   handy_sdl_video_setup(int fullscreen, int scale);

// Point Mikey at our pixel buffer. Must be called again after the emulation
// object is replaced, and whenever cartridge rotation changes the geometry.
void  handy_sdl_attach_display(void);

// Rebuild the texture and logical size from the current LynxWidth/LynxHeight.
// Cartridge rotation swaps 160x102 for 102x160, so this runs on every load.
int   handy_sdl_video_reconfigure(void);

// Mikey's end-of-frame callback. This only flags the frame as ready; drawing
// happens in the main loop so that the GUI can compose on top of it.
UBYTE *handy_sdl_display_callback(UOBJREF objref);

// Upload the last frame and present it. Pass a callback to draw the GUI over
// the emulation, or NULL for none.
void  handy_sdl_present(void (*overlay)(void));

int   handy_sdl_frame_pending(void);

void  handy_sdl_set_fullscreen(int on);
int   handy_sdl_get_fullscreen(void);
void  handy_sdl_set_smoothing(int linear);
int   handy_sdl_get_smoothing(void);
void  handy_sdl_set_window_scale(int scale);

void  handy_sdl_video_close(void);

#endif
