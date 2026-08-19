//
// Handy/SDL - staged cartridge watch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "SDL.h"
#include "handy_sdl_bootwatch.h"

// FujiNet writes the image under this name, in its own working directory.
#define STAGED_IMAGE_NAME  "staged-image.lnx"

// Where it is moved to once claimed. Keeping it rather than deleting it means
// the last launched image can be reloaded by hand.
#define CLAIMED_IMAGE_NAME "staged-image.loaded.lnx"

// Checking the filesystem every batch would be thousands of stats a second for
// something a player triggers by hand.
#define POLL_INTERVAL_MS 250

// FujiNet builds its config cartridge into the data directory it ships.
#define CONFIG_ROM_RELPATH "data/lynxcfg.lnx"

static char watch_dir[1024] = "";
static char staged_path[1152];
static char claimed_path[1152];
static Uint32 next_poll = 0;

void handy_sdl_bootwatch_set_dir(const char *dir)
{
	if(dir==NULL || *dir=='\0')
	{
		watch_dir[0] = '\0';
		return;
	}

	strncpy(watch_dir, dir, sizeof(watch_dir)-1);
	watch_dir[sizeof(watch_dir)-1] = '\0';

	snprintf(staged_path, sizeof(staged_path), "%s/%s", watch_dir, STAGED_IMAGE_NAME);
	snprintf(claimed_path, sizeof(claimed_path), "%s/%s", watch_dir, CLAIMED_IMAGE_NAME);

	printf("ComLynx: watching for staged cartridges in %s\n", watch_dir);
}

const char *handy_sdl_bootwatch_config_rom(void)
{
	static char config_path[1152];
	struct stat st;

	if(!handy_sdl_bootwatch_enabled()) return NULL;

	snprintf(config_path, sizeof(config_path), "%s/%s", watch_dir, CONFIG_ROM_RELPATH);
	if(stat(config_path, &st)!=0 || st.st_size<=0) return NULL;

	return config_path;
}


int handy_sdl_bootwatch_enabled(void)
{
	return watch_dir[0] != '\0';
}

const char *handy_sdl_bootwatch_poll(void)
{
	struct stat st;
	Uint32 now;

	if(!handy_sdl_bootwatch_enabled()) return NULL;

	now = SDL_GetTicks();
	if(now < next_poll) return NULL;
	next_poll = now + POLL_INTERVAL_MS;

	if(stat(staged_path, &st)!=0) return NULL;
	if(st.st_size<=0) return NULL;

	// Claim it by renaming. FujiNet writes to a temporary name and renames into
	// place, so anything visible here is already complete; moving it aside
	// stops the same image being loaded again on the next poll.
	remove(claimed_path);
	if(rename(staged_path, claimed_path)!=0)
	{
		printf("ComLynx: could not claim staged cartridge %s\n", staged_path);
		return NULL;
	}

	printf("ComLynx: staged cartridge ready (%ld bytes)\n", (long)st.st_size);
	return claimed_path;
}
