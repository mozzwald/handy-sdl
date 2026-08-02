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
// handy_sdl_graphics.cpp                                                   //
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "handy_sdl_main.h"
#include "handy_sdl_graphics.h"

SDL_Window		*mainWindow   = NULL;
SDL_Renderer	*mainRenderer = NULL;
SDL_Texture		*lynxTexture  = NULL;

static int	 frame_pending = 0;
static int	 fullscreen_on = 0;
static int	 smoothing_on  = 0;
static int	 window_scale  = 3;

// Mikey renders 0x00RRGGBB, so the alpha byte is ignored: RGB888 rather than
// ARGB8888, which would come out fully transparent.
#define LYNX_TEXTURE_FORMAT	SDL_PIXELFORMAT_RGB888

/*
	Name                :   handy_sdl_attach_display
	Function            :   Hand Mikey the buffer it should render into.

	Information         :   Mikey writes straight into mpLynxBuffer in the
	                        pixel format we ask for, so with 32BPP selected the
	                        buffer can be uploaded to a texture as-is. There is
	                        no intermediate surface and no per-frame memcpy.
*/
void handy_sdl_attach_display(void)
{
	mpLynx->DisplaySetAttributes( LynxRotate,
	                              MIKIE_PIXEL_FORMAT_32BPP,
	                              (ULONG)(LynxWidth * 4),
	                              handy_sdl_display_callback,
	                              (UOBJREF)mpLynxBuffer );
}

/*
	Name                :   handy_sdl_video_reconfigure
	Function            :   (Re)build the texture for the current geometry.

	Information         :   Cartridge rotation turns 160x102 into 102x160, so
	                        this runs on every ROM load and not just at startup.
*/
// Derive the display geometry from the cartridge rotation. A rotated cart is
// displayed portrait, so width and height swap.
static void handy_sdl_update_geometry(void)
{
	switch(LynxRotate)
	{
		case MIKIE_ROTATE_L:
		case MIKIE_ROTATE_R:
			LynxWidth  = HANDY_SCREEN_HEIGHT;
			LynxHeight = HANDY_SCREEN_WIDTH;
			break;
		default:
			LynxWidth  = HANDY_SCREEN_WIDTH;
			LynxHeight = HANDY_SCREEN_HEIGHT;
			break;
	}
}

int handy_sdl_video_reconfigure(void)
{
	handy_sdl_update_geometry();

	if(lynxTexture)
	{
		SDL_DestroyTexture(lynxTexture);
		lynxTexture = NULL;
	}

	// Nearest for an exact pixel look, linear when smoothing is asked for.
	// This is a creation-time hint, hence rebuilding on change.
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smoothing_on ? "linear" : "nearest");

	lynxTexture = SDL_CreateTexture( mainRenderer,
	                                 LYNX_TEXTURE_FORMAT,
	                                 SDL_TEXTUREACCESS_STREAMING,
	                                 LynxWidth, LynxHeight );
	if(lynxTexture == NULL)
	{
		printf("Could not create the Lynx texture: %s\n", SDL_GetError());
		return 0;
	}

	// Everything the emulator draws is addressed in Lynx pixels; SDL scales
	// and letterboxes to the real window size for us.
	SDL_RenderSetLogicalSize(mainRenderer, LynxWidth, LynxHeight);

	return 1;
}

/*
	Name                :   handy_sdl_video_setup
	Parameters          :   fullscreen (0/1), scale (initial window multiplier)
	Function            :   Bring up the window and renderer.
*/
int handy_sdl_video_setup(int fullscreen, int scale)
{
	if(scale < 1) scale = 1;
	window_scale = scale;

	// Needed before the window is sized; reconfigure() recomputes it later.
	handy_sdl_update_geometry();

	// Mikey renders into this. Rotation swaps width and height but not the
	// pixel count, so one allocation covers both orientations for the life of
	// the process and survives cartridge changes.
	if(mpLynxBuffer == NULL)
	{
		mpLynxBuffer = (Uint32 *)calloc(HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT, sizeof(Uint32));
		if(mpLynxBuffer == NULL)
		{
			printf("Could not allocate the Lynx framebuffer\n");
			return 0;
		}
	}

	mainWindow = SDL_CreateWindow( "Handy/SDL",
	                               SDL_WINDOWPOS_CENTERED,
	                               SDL_WINDOWPOS_CENTERED,
	                               LynxWidth * scale,
	                               LynxHeight * scale,
	                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI );
	if(mainWindow == NULL)
	{
		printf("Could not create window: %s\n", SDL_GetError());
		return 0;
	}

	mainRenderer = SDL_CreateRenderer( mainWindow, -1, SDL_RENDERER_ACCELERATED );
	if(mainRenderer == NULL)
	{
		// A machine with no working accelerated driver should still run.
		printf("No accelerated renderer (%s), falling back to software\n", SDL_GetError());
		mainRenderer = SDL_CreateRenderer( mainWindow, -1, SDL_RENDERER_SOFTWARE );
	}
	if(mainRenderer == NULL)
	{
		printf("Could not create renderer: %s\n", SDL_GetError());
		return 0;
	}

	SDL_SetRenderDrawColor(mainRenderer, 0, 0, 0, 255);

	if(!handy_sdl_video_reconfigure()) return 0;

	if(fullscreen) handy_sdl_set_fullscreen(1);

	// The Lynx has no pointer, so keep it out of the way. The GUI turns it
	// back on when a menu is open.
	SDL_ShowCursor(SDL_DISABLE);

	return 1;
}

/*
	Name                :   handy_sdl_display_callback
	Function            :   Mikey end-of-frame hook.

	Information         :   Deliberately does no drawing. Presenting from here
	                        would mean compositing the GUI from inside the
	                        emulation core, so this only raises a flag and the
	                        main loop does the work.
*/
UBYTE *handy_sdl_display_callback(UOBJREF objref)
{
	(void)objref;
	frame_pending = 1;
	return (UBYTE *)mpLynxBuffer;
}

int handy_sdl_frame_pending(void)
{
	return frame_pending;
}

/*
	Name                :   handy_sdl_present
	Parameters          :   overlay - GUI draw callback, or NULL
	Function            :   Upload the last frame, draw it, present.
*/
void handy_sdl_present(void (*overlay)(void))
{
	if(frame_pending)
	{
		SDL_UpdateTexture(lynxTexture, NULL, mpLynxBuffer, LynxWidth * 4);
		frame_pending = 0;
	}

	SDL_RenderClear(mainRenderer);
	SDL_RenderCopy(mainRenderer, lynxTexture, NULL, NULL);

	if(overlay) overlay();

	SDL_RenderPresent(mainRenderer);
}

void handy_sdl_set_fullscreen(int on)
{
	fullscreen_on = on ? 1 : 0;
	SDL_SetWindowFullscreen(mainWindow, fullscreen_on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	SDL_ShowCursor(fullscreen_on ? SDL_DISABLE : SDL_ENABLE);
}

int handy_sdl_get_fullscreen(void)
{
	return fullscreen_on;
}

void handy_sdl_set_smoothing(int linear)
{
	if(smoothing_on == (linear ? 1 : 0)) return;
	smoothing_on = linear ? 1 : 0;
	// Scale quality is fixed when the texture is created, so rebuild it.
	handy_sdl_video_reconfigure();
}

int handy_sdl_get_smoothing(void)
{
	return smoothing_on;
}

void handy_sdl_set_window_scale(int scale)
{
	if(scale < 1) scale = 1;
	window_scale = scale;
	if(fullscreen_on) handy_sdl_set_fullscreen(0);
	SDL_SetWindowSize(mainWindow, LynxWidth * scale, LynxHeight * scale);
}

void handy_sdl_video_close(void)
{
	if(lynxTexture)  { SDL_DestroyTexture(lynxTexture);   lynxTexture  = NULL; }
	if(mainRenderer) { SDL_DestroyRenderer(mainRenderer); mainRenderer = NULL; }
	if(mainWindow)   { SDL_DestroyWindow(mainWindow);     mainWindow   = NULL; }
}
