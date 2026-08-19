//
// Handy/SDL - staged cartridge watch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef HANDY_SDL_BOOTWATCH_H
#define HANDY_SDL_BOOTWATCH_H

//
// FujiNet's Lynx config cartridge normally downloads a ROM over ComLynx and
// programs it into a flash cart. There is no flash cart here - the emulator is
// the cartridge - so the emulator-target build of that cartridge asks FujiNet
// to stage the image to a file instead, and this watches for it.
//
// Point it at FujiNet's working directory, which is where the runtime writes
// the staged image.
//

// Directory to watch. Passing NULL or an empty string disables the watch.
void handy_sdl_bootwatch_set_dir(const char *dir);

// True when -bootdir was given, so stored settings do not overwrite it.
int  handy_sdl_bootwatch_enabled(void);

// Path of the FujiNet config cartridge shipped in the runtime's data
// directory, or NULL when the watch is disabled or the file is absent. Loading
// it gives the player somewhere to browse hosts from with no cartridge of
// their own.
const char *handy_sdl_bootwatch_config_rom(void);

// Returns the path of a newly staged image, or NULL. The file is claimed by
// renaming it aside, so it is only ever reported once and a partially written
// image is never picked up.
const char *handy_sdl_bootwatch_poll(void);

#endif
