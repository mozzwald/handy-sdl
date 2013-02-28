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
#include "handy_sdl_usage.h"
//#include "sdlemu/sdlemu_opengl.h"

/* SDL declarations */
SDL_Surface		*HandyBuffer; 			// Our Handy/SDL display buffer
SDL_Surface		*mainSurface;	 		// Our Handy/SDL primary display

/* Handy declarations */
Uint32			*mpLynxBuffer;
CSystem 		*mpLynx;
int				 mFrameSkip = 0;
int				 mpBpp;				    // Lynx rendering bpp

/* Handy/SDL declarations */
int			 	LynxWidth;				// Lynx SDL screen width
int			 	LynxHeight;      		// Lynx SDL screen height
int				LynxScale = 1;			// Factor to scale the display
int				LynxLCD = 1;			// Emulate LCD Display
int 		 	LynxFormat;				// Lynx ROM format type
int 		 	LynxRotate;				// Lynx ROM rotation type
Uint32          overlay_format = SDL_YV12_OVERLAY; // YUV Overlay format

int		 		emulation = 0;
Uint8          *delta;
/*
	Handy/SDL Rendering output

	1 = SDL rendering
	2 = OpenGL rendering
	3 = YUV Overlay rendering

	Default = 1 (SDL)
*/
int				rendertype = 1;

/*
	Handy/SDL Scaling/Scanline routine

	1 = SDLEmu v1 (compatible with al SDL versions)
	2 = SDLEmu v2 (faster but might break in future SDL versions or on certain platforms)
	3 = Pierre Doucet v1 (compatible but possiby slow)

	Default = 1 (SDLEmu v1)
*/

int				stype = 1;				// Scaling/Scanline routine.


/*
	Handy/SDL Filter selection

	1 = SDLEmu v1 (compatible with al SDL versions)
	2 = SDLEmu v2 (faster but might break in future SDL versions or on certain platforms)
	3 = Pierre Doucet v1 (compatible but possiby slow)

	Default = 1 (SDLEmu v1)
*/

int				filter = 0;				// Scaling/Scanline routine.


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
void handy_sdl_quit(void)
{

	// Disable audio and set emulation to pause, then quit :)
    SDL_PauseAudio(1);
	emulation   = -1;

    //Remove YUV Overlay
    if ( rendertype == 3 )
        handy_sdl_video_close();

	//Let is give some free memory
    free(mpLynxBuffer);

	// Destroy SDL Surface's
	SDL_FreeSurface(HandyBuffer);
	SDL_FreeSurface(mainSurface);

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
	int	 	    Throttle  = 1;  // Throttle to 60FPS
	int		 	framecounter = 0; // FPS Counter
	int		 	Autoskip = 0; // Autoskip
	int		 	Skipped = 0;
	int		 	Fullscreen = 0;
	int			bpp = 0; // BPP -> 8,16 or 32. 0 = autodetect (default)
	int		    fsaa = 0;   // OpenGL FSAA (default off)
	int			accel = 1;  // OpenGL Hardware accel (default on)
	int         sync  = 0;  // OpenGL VSYNC (default off)
	int			overlay = 1; // YUV Overlay format
	char        overlaytype[4];   // Overlay Format

	gAudioEnabled = TRUE;

	// Default output
	printf("Handy GCC/SDL Portable Atari Lynx Emulator %s\n", HANDY_SDL_VERSION);
	printf("Based upon %s by Keith Wilkins\n", HANDY_VERSION);
	printf("Written by SDLEmu Team, additions by Pierre Doucet\n");
	printf("Contact: http://sdlemu.ngemu.com | shalafi@xs4all.nl\n\n");

	if (argc < 2) {
		handy_sdl_usage();
		exit(EXIT_FAILURE);
	}

    for ( i=0; (i < argc || argv[i] != NULL ); i++ )
	{
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
		if (!strcmp(argv[i], "-fsaa"))			fsaa = 1;
		if (!strcmp(argv[i], "-nofsaa"))		fsaa = 0;
		if (!strcmp(argv[i], "-accel"))		accel = 1;
		if (!strcmp(argv[i], "-noaccel"))		accel = 0;
		if (!strcmp(argv[i], "-sync"))			sync = 1;
		if (!strcmp(argv[i], "-nosync"))		sync = 0;
		if (!strcmp(argv[i], "-2")) LynxScale = 2;
		if (!strcmp(argv[i], "-3")) LynxScale = 3;
		if (!strcmp(argv[i], "-4")) LynxScale = 4;
		if (!strcmp(argv[i], "-lcd")) LynxLCD = 1;
		if (!strcmp(argv[i], "-nolcd")) LynxLCD = 0;
		if (!strcmp(argv[i], "-frameskip"))
		{
			frameskip = atoi(argv[++i]);
			if ( frameskip > 9 )
				frameskip = 9;
		}
		if (!strcmp(argv[i], "-bpp"))
		{
			bpp = atoi(argv[++i]);
			if ( (bpp != 0) && (bpp != 8) && (bpp != 15) && (bpp != 16) && (bpp != 24) && (bpp != 32) )
			{
				bpp = 0;
			}
		}
		if (!strcmp(argv[i], "-rtype"))
		{
			rendertype = atoi(argv[++i]);
			if ( (rendertype != 1) && (rendertype != 2) && (rendertype != 3))
			{
				rendertype = 1;
			}
		}

		if (!strcmp(argv[i], "-stype"))
		{
			stype = atoi(argv[++i]);
			if ( (stype != 1) && (stype != 2) && (stype != 3))
			{
				stype = 1;
			}
		}

		if (!strcmp(argv[i], "-filter"))
		{
			filter = atoi(argv[++i]);
			// Check if the filter number is larger then 1 and not more then 10.
			if ( (filter <= 10) && (filter >= 1) )
			{
				rendertype =  1;  // Filter type only works with SDL rendering
				LynxScale  =  2;  // Maximum size is 2 times
				bpp        = 16;  // Maximum BPP is 16.
			}
			// Otherwise disable the filter
			else
			{
			    filter = 0;
			}
		}

		if (!strcmp(argv[i], "-format"))
		{
		    overlay = atoi(argv[++i]);
			if ( ( overlay <= 5 ) && (overlay >= 1) )
			{
				switch(overlay) {
					case 1: 
						overlay_format = SDL_YV12_OVERLAY;
						strcpy( overlaytype, "YV12" );
						break;
					case 2:
						overlay_format = SDL_IYUV_OVERLAY;
						strcpy( overlaytype, "IYUV" );
						break;
					case 3:
						overlay_format = SDL_YUY2_OVERLAY;
						strcpy( overlaytype, "YUY2" );
						break;
					case 4:
						overlay_format = SDL_UYVY_OVERLAY;
						strcpy( overlaytype, "UYVY" );
						break;
					case 5:
						overlay_format = SDL_YVYU_OVERLAY;
						strcpy( overlaytype, "YVYU" );
						break;
				}
			}	
			else
			{
			    overlay_format = SDL_YV12_OVERLAY;
				strcpy( overlaytype, "YV12" );
				
			}
			printf("Using YUV Overlay format: %s\n",overlaytype);
		}
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
		mpLynx = new CSystem(argv[1], "lynxboot.img");
	} catch (CLynxException &err) {
		cerr << err.mMsg.str() << ": " << err.mDesc.str() << endl;
		exit(EXIT_FAILURE);
	}
	printf("[DONE]\n\n");

	// Query Rom Image information
	handy_sdl_rom_info();

	// Initialise Handy/SDL video
	if( !handy_sdl_video_setup(rendertype,fsaa,Fullscreen, bpp, LynxScale, accel, sync) )
	{
		return 0;
	}



	// Initialise Handy/SDL audio
	printf("\nInitialising SDL Audio...     ");
	if(handy_sdl_audio_init())
	{
		gAudioEnabled = TRUE;
	}
	printf("[DONE]\n");


	// Setup of Handy Core video
	handy_sdl_video_init(mpBpp);


	handy_sdl_start_time = SDL_GetTicks();

	printf("Starting Lynx Emulation...\n");
	while(!emulation)
	{
		// Initialise Handy button events
		int OldKeyMask, KeyMask = mpLynx->GetButtonData();
		OldKeyMask = KeyMask;

		// Getting events for keyboard and/or joypad handling
		while(SDL_PollEvent(&handy_sdl_event))
		{
			switch(handy_sdl_event.type)
			{
				case SDL_KEYUP:
					KeyMask = handy_sdl_on_key_up(handy_sdl_event.key, KeyMask);
					break;
				case SDL_KEYDOWN:
					KeyMask = handy_sdl_on_key_down(handy_sdl_event.key, KeyMask);
					break;
				default:
					KeyMask = 0;
					break;
			}
		}

		// Checking if we had SDL handling events and then we'll update the Handy button events.
		if (OldKeyMask != KeyMask)
			mpLynx->SetButtonData(KeyMask);

		// Update TimerCount
		gTimerCount++;

		while( handy_sdl_update()  )
		{
			if(!gSystemHalt)
			{
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

				sprintf (buffer, "Handy %0.0f", fps_counter);
				strcat( buffer, "FPS");
				SDL_WM_SetCaption( buffer , "HANDY" );
			}
		}


	}

	return 0;
}
