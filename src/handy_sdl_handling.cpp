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
// handy_sdl_graphics.cpp                                                   //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This is the Handy/SDL handling. It manages the handling functions        //
// of the keyboard and/or joypad for emulating the Atari Lynx emulator      //
// using the SDL Library.             										//
//                                                                          //
//    N. Wagenaar                                                           //
// December 2005                                                            //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
// Revision History:                                                        //
// -----------------                                                        //
//                                                                          //
// December 2005 :                                                          //
//  Since the 14th of April, the WIN32 of Handy (written by Keith Wilkins)  //
//  Handy has become OpenSource. Handy/SDL v0.82 R1 was based upon the old  //
//  v0.82 sources and was released closed source.                           //
//                                                                          //
//  Because of this event, the new Handy/SDL will be released as OpenSource //
//  but is rewritten from scratch because of lost sources (tm). The SDLemu  //
//  team has tried to bring Handy/SDL v0.1 with al the functions from the   //
//  closed source version.                                                  //
//////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cctype>
#include <SDL.h>
#include <SDL_main.h>
#include <SDL_timer.h>

#include "handy_sdl_main.h"
#include "handy_sdl_handling.h"

int  handy_sdl_on_key_down(SDL_KeyboardEvent key, int mask)
{
	Sint16 x_move = 0, y_move = 0;
	
/*    
    if(joy) {
    	x_move = SDL_JoystickGetAxis(joystick, 0);
		y_move = SDL_JoystickGetAxis(joystick, 1);
    }
*/  

    switch(key.keysym.sym) {
    case SDLK_LEFT: {  // Lynx LEFT
		mask|=BUTTON_LEFT;
		break;
    }
    case SDLK_RIGHT: { // Lynx RIGHT
		mask|=BUTTON_RIGHT;
		break;
    }
	
    case SDLK_UP: { // Lynx UP
		mask|=BUTTON_UP;
		break;
    }
	
    case SDLK_DOWN: { // Lynx DOWN
		mask|=BUTTON_DOWN;
		break;
    }
	
    case SDLK_RETURN: { // Lynx PAUSE
		mask|=BUTTON_PAUSE;
		break;
    }
	
    case SDLK_x: { // Lynx B
        mask|=BUTTON_B;
		break;
    }
	
    case SDLK_z: { // Lynx A
		mask|=BUTTON_A; 
		break;
    }
	
	case SDLK_F1: { // Lynx Option 1
		mask|=BUTTON_OPT1;
		break;
	}

	case SDLK_F2: { // Lynx Option 2
		mask|=BUTTON_OPT2;
		break;
	}

   

    case SDLK_ESCAPE: { // ON/OFF key (well, definately more off :-)
       handy_sdl_quit();				
    }

	default: {
	   break;
	}

	}
	
/*    
    if(joy) {
    if(x_move > 32768/2)

    	eventstate |= ( HID_EVENT_RIGHT ); // seems to work fine
    			
    if(x_move < -32768/2)
    	eventstate |= ( HID_EVENT_LEFT );
  
 	if(y_move > 32768/2)
    	eventstate |= ( HID_EVENT_DOWN );
    			
    if(y_move < -32768/2)
    	eventstate |= ( HID_EVENT_UP );
    	
    if(SDL_JoystickGetButton(joystick, 1) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_A );
    
    if(SDL_JoystickGetButton(joystick, 2) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_B );
    	
    if(SDL_JoystickGetButton(joystick, 3) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_L );
    	
    if(SDL_JoystickGetButton(joystick, 4) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_R );
    }
*/

	return mask;

}

int  handy_sdl_on_key_up(SDL_KeyboardEvent key, int mask)
{
	Sint16 x_move = 0, y_move = 0;
	
//  Uint8 *keystate = SDL_GetKeyState(NULL); // First to initialize the keystates
//	int mod = SDL_GetModState();

/*    
    if(joy) {
    	x_move = SDL_JoystickGetAxis(joystick, 0);
		y_move = SDL_JoystickGetAxis(joystick, 1);
    }
*/  

    switch(key.keysym.sym)
	{
    case SDLK_LEFT: {  // Lynx LEFT
		mask&= ~BUTTON_LEFT;
		break;
    }
    case SDLK_RIGHT: { // Lynx RIGHT
		mask&= ~BUTTON_RIGHT;
		break;
    }
	
    case SDLK_UP: { // Lynx UP
		mask&= ~BUTTON_UP;
		break;
    }
	
    case SDLK_DOWN: { // Lynx DOWN
		mask&= ~BUTTON_DOWN;
		break;
    }
	
    case SDLK_RETURN: { // Lynx PAUSE
		mask&= ~BUTTON_PAUSE;
		break;
    }
	
    case SDLK_x: { // Lynx B
        mask&= ~BUTTON_B;
		break;
    }
	
    case SDLK_z: { // Lynx A
       mask&= ~BUTTON_A; 
       break;
    }
	
	case SDLK_F1: {// Lynx Option1
		mask&= ~BUTTON_OPT1;
		break;
	}

	case SDLK_F2: {// Lynx Option2
		mask&= ~BUTTON_OPT2;
		break;
	}

   

    case SDLK_ESCAPE: {// ON/OFF key (well, definately more off :-)
       handy_sdl_quit();				
    }
	
	default: {
	   break;
	}
	
	}
/*    
    if(joy) {
    if(x_move > 32768/2)

    	eventstate |= ( HID_EVENT_RIGHT ); // seems to work fine
    			
    if(x_move < -32768/2)
    	eventstate |= ( HID_EVENT_LEFT );
  
 	if(y_move > 32768/2)
    	eventstate |= ( HID_EVENT_DOWN );
    			
    if(y_move < -32768/2)
    	eventstate |= ( HID_EVENT_UP );
    	
    if(SDL_JoystickGetButton(joystick, 1) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_A );
    
    if(SDL_JoystickGetButton(joystick, 2) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_B );
    	
    if(SDL_JoystickGetButton(joystick, 3) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_L );
    	
    if(SDL_JoystickGetButton(joystick, 4) == SDL_PRESSED)
    	eventstate |= ( HID_EVENT_R );
    }
*/

	return mask;
}
