//
// Handy/SDL - Settings file
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef HANDY_SDL_CONFIG_H
#define HANDY_SDL_CONFIG_H

//
// Plain key=value settings kept in $XDG_CONFIG_HOME/handy-sdl/config, or
// ~/.config/handy-sdl/config when XDG_CONFIG_HOME is unset.
//
// Loading happens before the command line is parsed, so a flag always beats
// the stored value. Applying is split up because the settings land in modules
// that come up at different points: the video settings are needed before the
// window exists, and the input bindings would be overwritten by the defaults
// that handy_sdl_input_init() installs.
//

// Read the file into memory. Safe when there is no file yet.
void handy_sdl_config_load(void);

// Video settings, for use before the window is created. Each pointer is left
// alone when the file had nothing to say about it, so seed them with the
// built-in defaults first.
void handy_sdl_config_get_video(int *scale, int *fullscreen,
                                int *smooth, int *integer_scale);

// Emulator settings, same convention.
void handy_sdl_config_get_emulation(int *throttle, int *sound, int *fps);

// Push the stored bindings over the defaults. Call after handy_sdl_input_init().
void handy_sdl_config_apply_input(void);

// Push the stored ComLynx settings, and bring the link up if it was running
// when the settings were last written. Call before handy_sdl_comlynx_init().
void handy_sdl_config_apply_comlynx(void);

// Push the stored ROM directory and recent list. Call after the GUI is up.
void handy_sdl_config_apply_gui(void);

// Collect the current state from every module and write it out.
void handy_sdl_config_save(void);

// Where the file lives, for messages.
const char *handy_sdl_config_path(void);

#endif
