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
// handy_sdl_handling.cpp                                                   //
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "handy_sdl_main.h"
#include "handy_sdl_handling.h"

// Analog sticks are mapped onto the d-pad. A third of full deflection is far
// enough to be deliberate without needing a precise centre.
#define PAD_AXIS_DEADZONE	10000

typedef struct {
	const char			*name;
	ULONG				 mask;		// Lynx button bit, from susie.h
	SDL_Scancode		 key;
	int					 pad;		// SDL_GameControllerButton, or -1
} HANDY_BINDING;

static HANDY_BINDING bindings[HANDY_BTN_COUNT];

static SDL_GameController	*pad = NULL;
static SDL_JoystickID		 pad_id = -1;


void handy_sdl_input_defaults(void)
{
	// The historical Handy/SDL layout. Note A is the left hand key and B the
	// right hand one, which is the opposite way round to the console's labels
	// but is what this emulator has always done.
	static const struct { const char *name; ULONG mask; SDL_Scancode key; int pad; }
	defaults[HANDY_BTN_COUNT] = {
		{ "Up",       BUTTON_UP,    SDL_SCANCODE_UP,     SDL_CONTROLLER_BUTTON_DPAD_UP    },
		{ "Down",     BUTTON_DOWN,  SDL_SCANCODE_DOWN,   SDL_CONTROLLER_BUTTON_DPAD_DOWN  },
		{ "Left",     BUTTON_LEFT,  SDL_SCANCODE_LEFT,   SDL_CONTROLLER_BUTTON_DPAD_LEFT  },
		{ "Right",    BUTTON_RIGHT, SDL_SCANCODE_RIGHT,  SDL_CONTROLLER_BUTTON_DPAD_RIGHT },
		{ "A",        BUTTON_A,     SDL_SCANCODE_Z,      SDL_CONTROLLER_BUTTON_A          },
		{ "B",        BUTTON_B,     SDL_SCANCODE_X,      SDL_CONTROLLER_BUTTON_B          },
		{ "Option 1", BUTTON_OPT1,  SDL_SCANCODE_F1,     SDL_CONTROLLER_BUTTON_LEFTSHOULDER  },
		{ "Option 2", BUTTON_OPT2,  SDL_SCANCODE_F2,     SDL_CONTROLLER_BUTTON_RIGHTSHOULDER },
		{ "Pause",    BUTTON_PAUSE, SDL_SCANCODE_RETURN, SDL_CONTROLLER_BUTTON_START      }
	};

	for(int i = 0; i < HANDY_BTN_COUNT; i++)
	{
		bindings[i].name = defaults[i].name;
		bindings[i].mask = defaults[i].mask;
		bindings[i].key  = defaults[i].key;
		bindings[i].pad  = defaults[i].pad;
	}
}

static void handy_sdl_pad_open(int index)
{
	if(pad != NULL) return;

	if(!SDL_IsGameController(index))
	{
		// SDL sees the device but has no mapping for it, so it cannot be used
		// as a controller. Say so rather than ignoring it silently - this is
		// the usual reason a pad "isn't detected". A mapping can be supplied
		// through the SDL_GAMECONTROLLERCONFIG environment variable.
		const char *name = SDL_JoystickNameForIndex(index);
		SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(index);
		char guidstr[64];
		SDL_JoystickGetGUIDString(guid, guidstr, sizeof(guidstr));
		printf("Input device %d (%s) has no controller mapping, ignoring.\n"
		       "  GUID: %s\n", index, name ? name : "unknown", guidstr);
		return;
	}

	pad = SDL_GameControllerOpen(index);
	if(pad == NULL)
	{
		printf("Could not open controller %d: %s\n", index, SDL_GetError());
		return;
	}

	SDL_Joystick *js = SDL_GameControllerGetJoystick(pad);
	pad_id = SDL_JoystickInstanceID(js);
	printf("Controller connected: %s\n", SDL_GameControllerName(pad));
}

static void handy_sdl_pad_close(void)
{
	if(pad == NULL) return;
	printf("Controller disconnected\n");
	SDL_GameControllerClose(pad);
	pad = NULL;
	pad_id = -1;
}

void handy_sdl_input_init(void)
{
	handy_sdl_input_defaults();

	// Sony pads over Bluetooth are driven by SDL's own HIDAPI backends rather
	// than the kernel joystick device, and those need asking for explicitly on
	// some builds. Harmless where they are already the default.
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");

	// GameController rather than raw Joystick: SDL ships a mapping database,
	// so ordinary pads work without any per-device configuration.
	if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
	{
		printf("Controller support unavailable: %s\n", SDL_GetError());
		return;
	}

	int n = SDL_NumJoysticks();
	if(n == 0) printf("No input devices found.\n");
	for(int i = 0; i < n; i++) handy_sdl_pad_open(i);
}

void handy_sdl_input_close(void)
{
	handy_sdl_pad_close();
}

int handy_sdl_input_event(SDL_Event *event)
{
	switch(event->type)
	{
		case SDL_CONTROLLERDEVICEADDED:
			handy_sdl_pad_open(event->cdevice.which);
			return 1;
		case SDL_CONTROLLERDEVICEREMOVED:
			if(event->cdevice.which == pad_id) handy_sdl_pad_close();
			return 1;
		default:
			return 0;
	}
}

/*
	Name                :   handy_sdl_input_poll
	Function            :   Rebuild the Lynx button mask and hand it over.

	Information         :   Polls current state rather than accumulating key
	                        events. The old code built the mask from KEYDOWN and
	                        KEYUP and could be left holding a button forever if
	                        an event went astray - losing window focus mid-press
	                        was enough to do it.
*/
void handy_sdl_input_poll(int allow_keyboard)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	ULONG mask = 0;

	for(int i = 0; i < HANDY_BTN_COUNT; i++)
	{
		int down = 0;

		if(allow_keyboard && bindings[i].key != SDL_SCANCODE_UNKNOWN &&
		   keys[bindings[i].key])
			down = 1;

		if(pad != NULL && bindings[i].pad >= 0 &&
		   SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)bindings[i].pad))
			down = 1;

		if(down) mask |= bindings[i].mask;
	}

	// Left stick doubles as the d-pad.
	if(pad != NULL)
	{
		int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		int y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);

		if(x < -PAD_AXIS_DEADZONE) mask |= BUTTON_LEFT;
		if(x >  PAD_AXIS_DEADZONE) mask |= BUTTON_RIGHT;
		if(y < -PAD_AXIS_DEADZONE) mask |= BUTTON_UP;
		if(y >  PAD_AXIS_DEADZONE) mask |= BUTTON_DOWN;
	}

	mpLynx->SetButtonData(mask);
}

const char *handy_sdl_input_name(int button)
{
	if(button < 0 || button >= HANDY_BTN_COUNT) return "";
	return bindings[button].name;
}

SDL_Scancode handy_sdl_input_get_key(int button)
{
	if(button < 0 || button >= HANDY_BTN_COUNT) return SDL_SCANCODE_UNKNOWN;
	return bindings[button].key;
}

void handy_sdl_input_set_key(int button, SDL_Scancode code)
{
	if(button < 0 || button >= HANDY_BTN_COUNT) return;

	// A key can only drive one Lynx button, so clear it from any other.
	for(int i = 0; i < HANDY_BTN_COUNT; i++)
		if(i != button && bindings[i].key == code) bindings[i].key = SDL_SCANCODE_UNKNOWN;

	bindings[button].key = code;
}

int handy_sdl_input_get_pad(int button)
{
	if(button < 0 || button >= HANDY_BTN_COUNT) return -1;
	return bindings[button].pad;
}

void handy_sdl_input_set_pad(int button, int pad_button)
{
	if(button < 0 || button >= HANDY_BTN_COUNT) return;

	for(int i = 0; i < HANDY_BTN_COUNT; i++)
		if(i != button && bindings[i].pad == pad_button) bindings[i].pad = -1;

	bindings[button].pad = pad_button;
}

void handy_sdl_input_swap_ab(void)
{
	SDL_Scancode k = bindings[HANDY_BTN_A].key;
	int          p = bindings[HANDY_BTN_A].pad;

	bindings[HANDY_BTN_A].key = bindings[HANDY_BTN_B].key;
	bindings[HANDY_BTN_A].pad = bindings[HANDY_BTN_B].pad;
	bindings[HANDY_BTN_B].key = k;
	bindings[HANDY_BTN_B].pad = p;
}

const char *handy_sdl_input_pad_name(void)
{
	return pad ? SDL_GameControllerName(pad) : NULL;
}
