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
// handy_sdl_main.cpp                                                       //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This is the main Handy/SDL source. It manages the main functions for     //
// emulating the Atari Lynx emulator using the SDL Library.                 //
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
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cctype>
#include <SDL.h>
#include <SDL_main.h>
#include <SDL_timer.h>

#include "handy_sdl_main.h"
#include "handy_sdl_graphics.h"
#include "handy_sdl_handling.h"
#include "handy_sdl_sound.h"
#include "handy_sdl_comlynx.h"
#include "handy_sdl_gui.h"
#include "handy_sdl_config.h"
#include "handy_sdl_usage.h"
//#include "sdlemu/sdlemu_opengl.h"

/* Handy declarations */
Uint32			*mpLynxBuffer;			// Mikey renders straight into this
CSystem 		*mpLynx;
int				 mFrameSkip = 0;

/* Handy/SDL declarations */
int			 	LynxWidth;				// Lynx screen width  (102 if rotated)
int			 	LynxHeight;      		// Lynx screen height (160 if rotated)
int				LynxScale = 3;			// Initial window size multiplier
int 		 	LynxFormat;				// Lynx ROM format type
int 		 	LynxRotate;				// Lynx ROM rotation type

int		 		emulation = 0;

// Promoted out of main() so the settings file can read them back.
int				Throttle = 1;		// throttle to 60FPS
int				framecounter = 0;	// FPS counter in the title bar

/*
	Name	            : 	handy_sdl_update
	Parameters          : 	N/A
	Function			:   Update/Throttle function for Handy/SDL.

	Uses				:   N/A

	Information			:	This function is basicly the Update() function from
							Handy WIN32 with minor tweaks for SDL. It is used for
							basic throttle of the Handy core.
*/
inline	int handy_sdl_update(void)
{


		// Throttling code
		//
		if(gSystemCycleCount>gThrottleNextCycleCheckpoint)
		{
			static int limiter=0;
			static int flipflop=0;
			int overrun=gSystemCycleCount-gThrottleNextCycleCheckpoint;
			int nextstep=(((HANDY_SYSTEM_FREQ/HANDY_BASE_FPS)*gThrottleMaxPercentage)/100);

			// We've gone thru the checkpoint, so therefore the
			// we must have reached the next timer tick, if the
			// timer hasnt ticked then we've got here early. If
			// so then put the system to sleep by saying there
			// is no more idle work to be done in the idle loop

			if(gThrottleLastTimerCount==gTimerCount)
			{
				// All we know is that we got here earlier than expected as the
				// counter has not yet rolled over
				if(limiter<0) limiter=0; else limiter++;
				if(limiter>40 && mFrameSkip>0)
				{
					mFrameSkip--;
					limiter=0;
				}
				flipflop=1;
				return 0;
			}

			// Frame Skip adjustment
			if(!flipflop)
			{
				if(limiter>0) limiter=0; else limiter--;
				if(limiter<-7 && mFrameSkip<10)
				{
					mFrameSkip++;
					limiter=0;
				}
			}

			flipflop=0;

			//Set the next control point
			gThrottleNextCycleCheckpoint+=nextstep;

			// Set next timer checkpoint
			gThrottleLastTimerCount=gTimerCount;

			// Check if we've overstepped the speed limit
			if(overrun>nextstep)
			{
				// We've exceeded the next timepoint, going way too
				// fast (sprite drawing) so reschedule.
				return 0;
			}

		}

		return 1;

}

/*
	Name	            : 	handy_sdl_rom_info
	Parameters          : 	N/A
	Function			:   Game Image information function for Handy/SDL

	Uses				:   N/A

	Information			:	Basic function for getting information of the
							Atari Lynx game image and for setting up the
							Handy core concerning rotation.
*/
void handy_sdl_rom_info(void)
{

	printf("Atari Lynx ROM Information\n");

	/* Retrieving Game Image information */
	printf("Cartname      : %s\n"   , mpLynx->CartGetName()         );
	printf("ROM Size      : %d kb\n", (int)mpLynx->CartSize()      );
	printf("Manufacturer  : %s\n"   , mpLynx->CartGetManufacturer() );

	/* Retrieving Game Image Rotatation */
	printf("Lynx Rotation : ");
	switch(mpLynx->CartGetRotate())
	{
		case CART_NO_ROTATE:
			LynxRotate = MIKIE_NO_ROTATE;
			printf("NO\n");
			break;
		case CART_ROTATE_LEFT:
			LynxRotate = MIKIE_ROTATE_L;
			printf("LEFT\n");
			break;
		case CART_ROTATE_RIGHT:
			LynxRotate = MIKIE_ROTATE_R;
			printf("RIGHT\n");
			break;
		default:
			// Allright, this shouldn't be necassary. But in case the user is using a
			// bad dump, we use the default rotation as in no rotation.
			LynxRotate = MIKIE_NO_ROTATE;
			printf("NO (forced)\n");
			break;
	}

	/* Retrieving Game Image type */
	printf("ROM Type      : ");
	switch(mpLynx->mFileType)
	{
		case HANDY_FILETYPE_HOMEBREW:
			printf("Homebrew\n");
			break;
		case HANDY_FILETYPE_LNX:
			printf("Commercial and/or .LNX-format\n");
			break;
		case HANDY_FILETYPE_SNAPSHOT:
			printf("Snapshot\n");
			break;
		default:
			// Allright, this shouldn't be necessary, but just in case.
			printf("Unknown format!\n");
			exit(EXIT_FAILURE);
			break;
	}

}
/*
	Name	            : 	handy_sdl_load_rom
	Parameters          : 	path to a cartridge image
	Function			:   Swap the cartridge without restarting.

	Information			:	Builds the replacement before tearing down the old
							one, so a bad file leaves the running game alone
							rather than killing the emulator.

							Everything registered against the old CSystem has
							to be re-armed: Mikey's display callback and the
							ComLynx transmit callback both live on the object
							being replaced. Rotation is per cartridge too, so
							the texture may need rebuilding at a new size.
*/
int handy_sdl_load_rom(const char *path)
{
	CSystem *replacement = NULL;

	if(path == NULL || *path == '\0') return 0;

	try {
		replacement = new CSystem((char *)path, "lynxboot.img");
	} catch (CLynxException &err) {
		cerr << "Could not load " << path << ": "
		     << err.mMsg.str() << ": " << err.mDesc.str() << endl;
		return 0;
	}

	// Keep the audio callback away from a half-swapped machine.
	SDL_PauseAudio(1);

	delete mpLynx;
	mpLynx = replacement;

	handy_sdl_rom_info();
	handy_sdl_video_reconfigure();
	handy_sdl_attach_display();
	handy_sdl_comlynx_reattach();

	SDL_PauseAudio(0);

	printf("Loaded %s\n", path);
	return 1;
}

void handy_sdl_quit(void)
{

	// Disable audio and set emulation to pause, then quit :)
    SDL_PauseAudio(1);
	emulation   = -1;

	// Capture the current state before anything is torn down.
	handy_sdl_config_save();

	handy_sdl_input_close();
	handy_sdl_gui_close();
	handy_sdl_comlynx_close();
	handy_sdl_video_close();

	free(mpLynxBuffer);

	// Close SDL Subsystems
	SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
	SDL_Quit();
	exit(EXIT_SUCCESS);

}

int main(int argc, char *argv[])
{
	int 		i;
	int	    	frameskip  			= 0;   // Frameskip
	SDL_Event	handy_sdl_event;
	Uint32  	handy_sdl_start_time;
	Uint32  	handy_sdl_this_time;
	float   	fps_counter;
	int		 	Autoskip = 0; // Autoskip
	int		 	Skipped = 0;
	int		 	Fullscreen = 0;
	int			Smoothing  = 0;  // linear filtering when scaling up
	int			IntegerScale = 0;
	int			SoundWanted;

	gAudioEnabled = TRUE;
	SoundWanted   = 1;

	// Default output
	printf("Handy GCC/SDL Portable Atari Lynx Emulator %s\n", HANDY_SDL_VERSION);
	printf("Based upon %s by Keith Wilkins\n", HANDY_VERSION);
	printf("Written by SDLEmu Team, additions by Pierre Doucet\n");
	printf("Contact: http://sdlemu.ngemu.com | shalafi@xs4all.nl\n\n");

	// Settings first, so that anything on the command line overrides them.
	handy_sdl_config_load();
	handy_sdl_config_get_video(&LynxScale, &Fullscreen, &Smoothing, &IntegerScale);
	handy_sdl_config_get_emulation(&Throttle, &SoundWanted, &framecounter);
	gAudioEnabled = SoundWanted ? TRUE : FALSE;

	// A cartridge is optional now that there is a GUI to load one with. The
	// core boots happily with none - CCart reports "<No cart loaded>" - which
	// leaves the boot ROM on screen until something is opened.
	const char *romfile = "";
	if (argc > 1 && argv[1][0] != '-') romfile = argv[1];

    for ( i=0; (i < argc || argv[i] != NULL ); i++ )
	{
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") ||
		    !strcmp(argv[i], "--help") || !strcmp(argv[i], "-?"))
		{
			handy_sdl_usage();
			exit(EXIT_SUCCESS);
		}
		if (!strcmp(argv[i], "-throttle")) 	Throttle = 1;
		if (!strcmp(argv[i], "-nothrottle")) 	Throttle = 0;
		if (!strcmp(argv[i], "-autoskip")) 	Autoskip = 1;
		if (!strcmp(argv[i], "-noautoskip")) 	Autoskip = 0;
		if (!strcmp(argv[i], "-fps")) 			framecounter = 1;
		if (!strcmp(argv[i], "-nofps")) 		framecounter = 0;
		if (!strcmp(argv[i], "-sound")) 		gAudioEnabled = TRUE;
		if (!strcmp(argv[i], "-nosound")) 		gAudioEnabled = FALSE;
		if (!strcmp(argv[i], "-fullscreen"))	Fullscreen = 1;
		if (!strcmp(argv[i], "-nofullscreen"))	Fullscreen = 0;
		if (!strcmp(argv[i], "-smooth"))		Smoothing = 1;
		if (!strcmp(argv[i], "-nosmooth"))		Smoothing = 0;
		if (!strcmp(argv[i], "-1")) LynxScale = 1;
		if (!strcmp(argv[i], "-2")) LynxScale = 2;
		if (!strcmp(argv[i], "-3")) LynxScale = 3;
		if (!strcmp(argv[i], "-4")) LynxScale = 4;
		if (!strcmp(argv[i], "-scale"))
		{
			if (i+1 < argc) LynxScale = atoi(argv[++i]);
			if (LynxScale < 1) LynxScale = 1;
		}
		if (!strcmp(argv[i], "-frameskip"))
		{
			frameskip = atoi(argv[++i]);
			if ( frameskip > 9 )
				frameskip = 9;
		}
		if (!strcmp(argv[i], "-comlynx"))
		{
			if (i+1 >= argc || !handy_sdl_comlynx_parse(argv[++i]))
			{
				printf("ComLynx: -comlynx needs listen:PORT or connect:HOST:PORT\n");
				exit(EXIT_FAILURE);
			}
		}
		if (!strcmp(argv[i], "-comlynxtrace"))	handy_sdl_comlynx_trace(1);
	}

	// Initalising SDL for Audio and Video support
	printf("Initialising SDL...           ");
	if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "FAILED : Unable to init SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	printf("[DONE]\n");

	// Primary initalise of Handy
	printf("Initialising Handy Core...    ");
		try {
		// Ugh, hardcoded lynxboot.img. Will be fixed in future versions.
		mpLynx = new CSystem(romfile, "lynxboot.img");
	} catch (CLynxException &err) {
		cerr << err.mMsg.str() << ": " << err.mDesc.str() << endl;
		exit(EXIT_FAILURE);
	}
	printf("[DONE]\n\n");

	// Query Rom Image information
	handy_sdl_rom_info();

	// Stored ComLynx settings first, then anything -comlynx asked for.
	handy_sdl_config_apply_comlynx();
	handy_sdl_comlynx_init();

	// Initialise Handy/SDL video
	printf("Initialising Handy Display... ");
	handy_sdl_set_smoothing(Smoothing);
	handy_sdl_set_integer_scale(IntegerScale);
	if( !handy_sdl_video_setup(Fullscreen, LynxScale) )
	{
		return 0;
	}
	handy_sdl_attach_display();
	printf("[DONE]\n");

	// Bring up the GUI on the window we just created
	if(!handy_sdl_gui_init())
	{
		printf("Warning: could not start the GUI, continuing without it\n");
	}
	// Restore the browser directory and recent list, then let a cartridge named
	// on the command line take precedence over the stored directory.
	handy_sdl_config_apply_gui();
	handy_sdl_gui_set_rom_dir(romfile);

	// Nothing to play yet, so put the browser up rather than leaving the user
	// staring at the boot ROM wondering what to do.
	if(*romfile == '\0') handy_sdl_gui_open_browser();

	// Input bindings and controller support. Defaults are installed first, then
	// anything the settings file overrides.
	handy_sdl_input_init();
	handy_sdl_config_apply_input();

	// Initialise Handy/SDL audio
	printf("Initialising SDL Audio...     ");
	if(handy_sdl_audio_init())
	{
		gAudioEnabled = TRUE;
	}
	printf("[DONE]\n");


	handy_sdl_start_time = SDL_GetTicks();

	printf("Starting Lynx Emulation...\n");
	while(!emulation)
	{
		// Getting events for keyboard and/or joypad handling
		while(SDL_PollEvent(&handy_sdl_event))
		{
			// Let the GUI look first. When it takes an event - typing into a
			// text field, say - it must not also reach the Lynx.
			if(handy_sdl_gui_event(&handy_sdl_event)) continue;
			if(handy_sdl_input_event(&handy_sdl_event)) continue;

			switch(handy_sdl_event.type)
			{
				case SDL_KEYDOWN:
					// Escape is the power switch, and is deliberately not a
					// remappable Lynx button.
					if(handy_sdl_event.key.keysym.sym == SDLK_ESCAPE) handy_sdl_quit();
					break;
				case SDL_QUIT:
					handy_sdl_quit();
					break;
				default:
					break;
			}
		}

		// Rebuild the button state from what is actually held down. The GUI
		// gets first refusal on the keyboard so that typing a hostname into a
		// text field does not also play the game.
		handy_sdl_input_poll(!handy_sdl_gui_wants_keyboard());

		// Update TimerCount
		gTimerCount++;

		while( handy_sdl_update()  )
		{
			if(!gSystemHalt)
			{
				// Pump ComLynx once per batch. The UART Rx queue holds only
				// 32 bytes and drops overruns silently, so this cannot be
				// moved out to the once-per-frame loop.
				handy_sdl_comlynx_poll();

				for(ULONG loop=1024;loop;loop--)
				{
					mpLynx->Update();
				}
			}
			else
			{
#ifdef HANDY_SDL_DEBUG
					printf("gSystemHalt : %d\n", gSystemHalt);
#endif
					gTimerCount++;
			}
		}

		// Draw the frame the emulation just finished. This used to happen
		// inside Mikey's display callback; doing it here keeps the emulation
		// core out of the business of compositing the screen.
		handy_sdl_gui_frame();
		handy_sdl_present(handy_sdl_gui_draw);

		handy_sdl_this_time = SDL_GetTicks();

		fps_counter = (((float)gTimerCount/(handy_sdl_this_time-handy_sdl_start_time))*1000.0);
#ifdef HANDY_SDL_DEBUG
		printf("fps_counter : %f\n", fps_counter);
#endif

		if( (Throttle) && (fps_counter > 59.99) ) SDL_Delay( (Uint32)fps_counter );

		if(Autoskip)
		{
        	if(fps_counter > 60)
			{
                frameskip--;
       			Skipped = frameskip;
            }
			else
			{
				if(fps_counter < 60)
				{
               		Skipped++;
               		frameskip++;
               	}
            }
        }


		if ( framecounter )
		{

			if ( handy_sdl_this_time != handy_sdl_start_time )
			{
				static char buffer[256];

				snprintf (buffer, sizeof(buffer), "Handy/SDL - %0.0f FPS", fps_counter);
				SDL_SetWindowTitle( mainWindow, buffer );
			}
		}


	}

	handy_sdl_comlynx_close();

	return 0;
}

int handy_sdl_get_throttle(void)     { return Throttle; }
int handy_sdl_get_framecounter(void) { return framecounter; }
