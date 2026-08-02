//
// Handy/SDL - Settings file
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <SDL.h>

#include "handy_sdl_main.h"
#include "handy_sdl_graphics.h"
#include "handy_sdl_handling.h"
#include "handy_sdl_comlynx.h"
#include "handy_sdl_gui.h"
#include "handy_sdl_config.h"

struct CFG_ENTRY { std::string key, value; };

static std::vector<CFG_ENTRY>	entries;
static std::string				cfg_path;
static std::string				cfg_dir;

//
// A command line flag is an instruction for the run that used it, not a new
// preference. Passing -comlynx listen:9999 once should not silently become the
// stored port. So keep two snapshots: what the file said, and what startup
// settled on once the flags had been applied. At save time a value that still
// matches the startup snapshot was never touched by the user, and the stored
// value is written back instead of the flag's.
//
static int	stored_scale=-1, stored_fullscreen=-1, stored_smooth=-1, stored_integer=-1;
static int	stored_throttle=-1, stored_sound=-1, stored_fps=-1;
static int	base_scale=-1, base_fullscreen=-1, base_smooth=-1, base_integer=-1;
static int	base_throttle=-1, base_sound=-1, base_fps=-1;

static int			stored_cl_mode=HANDY_COMLYNX_OFF, stored_cl_port=8100, stored_cl_autostart=0;
static std::string	stored_cl_host="127.0.0.1";
static int			base_cl_mode=-1, base_cl_port=-1;
static std::string	base_cl_host;

static int settle(int current, int baseline, int stored)
{
	return (current==baseline) ? stored : current;
}


static void handy_sdl_config_locate(void)
{
	if(!cfg_path.empty()) return;

	const char *xdg = getenv("XDG_CONFIG_HOME");
	if(xdg && *xdg)
	{
		cfg_dir = std::string(xdg) + "/handy-sdl";
	}
	else
	{
		const char *home = getenv("HOME");
		cfg_dir = std::string(home ? home : ".") + "/.config/handy-sdl";
	}
	cfg_path = cfg_dir + "/config";
}

const char *handy_sdl_config_path(void)
{
	handy_sdl_config_locate();
	return cfg_path.c_str();
}

// Look up a single-valued key. Returns NULL when absent.
static const char *cfg_get(const char *key)
{
	for(size_t i = 0; i < entries.size(); i++)
		if(entries[i].key == key) return entries[i].value.c_str();
	return NULL;
}

static void cfg_get_int(const char *key, int *out)
{
	const char *v = cfg_get(key);
	if(v && out) *out = atoi(v);
}

void handy_sdl_config_load(void)
{
	handy_sdl_config_locate();
	entries.clear();

	FILE *fp = fopen(cfg_path.c_str(), "r");
	if(fp == NULL) return;

	char line[1200];
	while(fgets(line, sizeof(line), fp))
	{
		// Strip the newline and skip blanks and comments.
		char *nl = strpbrk(line, "\r\n");
		if(nl) *nl = '\0';
		if(line[0] == '\0' || line[0] == '#') continue;

		char *eq = strchr(line, '=');
		if(eq == NULL) continue;
		*eq = '\0';

		CFG_ENTRY e;
		e.key   = line;
		e.value = eq + 1;
		entries.push_back(e);
	}
	fclose(fp);
}

void handy_sdl_config_get_video(int *scale, int *fullscreen, int *smooth, int *integer_scale)
{
	cfg_get_int("video.scale",         scale);
	cfg_get_int("video.fullscreen",    fullscreen);
	cfg_get_int("video.smooth",        smooth);
	cfg_get_int("video.integer_scale", integer_scale);

	// Whatever the caller ends up with here is what the file had to say, or the
	// built-in default when it said nothing. Either way it is the value a flag
	// must not overwrite permanently.
	stored_scale      = scale         ? *scale         : -1;
	stored_fullscreen = fullscreen    ? *fullscreen    : -1;
	stored_smooth     = smooth        ? *smooth        : -1;
	stored_integer    = integer_scale ? *integer_scale : -1;
}

void handy_sdl_config_get_emulation(int *throttle, int *sound, int *fps)
{
	cfg_get_int("emu.throttle", throttle);
	cfg_get_int("emu.sound",    sound);
	cfg_get_int("emu.fps",      fps);

	stored_throttle = throttle ? *throttle : -1;
	stored_sound    = sound    ? *sound    : -1;
	stored_fps      = fps      ? *fps      : -1;
}

void handy_sdl_config_note_startup(int scale, int fullscreen, int smooth, int integer_scale,
                                   int throttle, int sound, int fps)
{
	base_scale      = scale;
	base_fullscreen = fullscreen;
	base_smooth     = smooth;
	base_integer    = integer_scale;
	base_throttle   = throttle;
	base_sound      = sound;
	base_fps        = fps;
}

// Bindings are keyed on a tidied-up button name: "Option 1" becomes "option1".
static std::string handy_sdl_config_button_key(int button)
{
	std::string n = handy_sdl_input_name(button);
	std::string out;
	for(size_t i = 0; i < n.size(); i++)
	{
		if(n[i] == ' ') continue;
		out += (char)tolower(n[i]);
	}
	return out;
}

void handy_sdl_config_apply_input(void)
{
	for(int b = 0; b < HANDY_BTN_COUNT; b++)
	{
		std::string base = "input." + handy_sdl_config_button_key(b);

		const char *k = cfg_get((base + ".key").c_str());
		if(k)
		{
			// SDL round-trips these names, so an unknown one means the file was
			// hand-edited badly; leave the default rather than unbinding.
			SDL_Scancode sc = SDL_GetScancodeFromName(k);
			if(sc != SDL_SCANCODE_UNKNOWN || !strcmp(k, "-"))
				handy_sdl_input_set_key(b, strcmp(k, "-") ? sc : SDL_SCANCODE_UNKNOWN);
		}

		const char *p = cfg_get((base + ".pad").c_str());
		if(p)
		{
			if(!strcmp(p, "-")) handy_sdl_input_set_pad(b, -1);
			else
			{
				SDL_GameControllerButton gb = SDL_GameControllerGetButtonFromString(p);
				if(gb != SDL_CONTROLLER_BUTTON_INVALID) handy_sdl_input_set_pad(b, (int)gb);
			}
		}
	}
}

void handy_sdl_config_apply_comlynx(void)
{
	const char *m = cfg_get("comlynx.mode");

	int mode = HANDY_COMLYNX_OFF;
	if(m && !strcmp(m, "listen"))       mode = HANDY_COMLYNX_LISTEN;
	else if(m && !strcmp(m, "connect")) mode = HANDY_COMLYNX_CONNECT;

	const char *host = cfg_get("comlynx.host");
	int port = 8100;
	cfg_get_int("comlynx.port", &port);

	int autostart = 0;
	cfg_get_int("comlynx.autostart", &autostart);

	// Remember what the file held even when the command line is about to win,
	// so a one-off -comlynx does not get written back as the new setting.
	stored_cl_mode      = mode;
	stored_cl_host      = host ? host : "127.0.0.1";
	stored_cl_port      = port;
	stored_cl_autostart = autostart;

	// -comlynx beats the stored settings outright, both for what it asks for
	// and for the decision to bring the link up at all.
	if(handy_sdl_comlynx_cli_requested() || m == NULL)
	{
		handy_sdl_config_note_startup_comlynx();
		return;
	}

	// Seed the settings either way, so the panel opens pre-filled. Only bring
	// the link up when it was running the last time settings were saved -
	// opening a socket unasked is not something to do on a whim.
	if(autostart && mode != HANDY_COMLYNX_OFF)
	{
		printf("ComLynx: restoring saved link\n");
		handy_sdl_comlynx_start(mode, host ? host : "127.0.0.1", port);
	}
	else
	{
		handy_sdl_comlynx_set_config(mode, host ? host : "127.0.0.1", port);
	}

	handy_sdl_config_note_startup_comlynx();
}

void handy_sdl_config_note_startup_comlynx(void)
{
	int mode = 0, port = 0;
	char host[256];
	handy_sdl_comlynx_get_config(&mode, host, sizeof(host), &port);
	base_cl_mode = mode;
	base_cl_host = host;
	base_cl_port = port;
}

void handy_sdl_config_apply_gui(void)
{
	const char *dir = cfg_get("rom.dir");
	if(dir) handy_sdl_gui_set_browse_dir(dir);

	// Recents are stored newest first; add them in reverse so the list ends up
	// in the same order.
	std::vector<std::string> recents;
	for(size_t i = 0; i < entries.size(); i++)
		if(entries[i].key == "rom.recent") recents.push_back(entries[i].value);

	for(size_t i = recents.size(); i > 0; i--)
		handy_sdl_gui_add_recent(recents[i-1].c_str());
}

void handy_sdl_config_save(void)
{
	handy_sdl_config_locate();

	// mkdir -p for the two levels we might need.
	{
		size_t slash = cfg_dir.find_last_of('/');
		if(slash != std::string::npos) mkdir(cfg_dir.substr(0, slash).c_str(), 0755);
		mkdir(cfg_dir.c_str(), 0755);
	}

	FILE *fp = fopen(cfg_path.c_str(), "w");
	if(fp == NULL)
	{
		printf("Could not write %s\n", cfg_path.c_str());
		return;
	}

	fprintf(fp, "# Handy/SDL settings. Command line options override these.\n\n");

	// settle() keeps a command line flag from becoming a stored preference: if
	// the value is untouched since startup it is written back as the file had
	// it, and only a change made while running is actually saved.
	fprintf(fp, "video.scale=%d\n",         settle(handy_sdl_get_window_scale(),  base_scale,      stored_scale));
	fprintf(fp, "video.fullscreen=%d\n",    settle(handy_sdl_get_fullscreen(),    base_fullscreen, stored_fullscreen));
	fprintf(fp, "video.smooth=%d\n",        settle(handy_sdl_get_smoothing(),     base_smooth,     stored_smooth));
	fprintf(fp, "video.integer_scale=%d\n", settle(handy_sdl_get_integer_scale(), base_integer,    stored_integer));
	fprintf(fp, "emu.throttle=%d\n",        settle(handy_sdl_get_throttle(),      base_throttle,   stored_throttle));
	fprintf(fp, "emu.sound=%d\n",           settle(gAudioEnabled ? 1 : 0,         base_sound,      stored_sound));
	fprintf(fp, "emu.fps=%d\n",             settle(handy_sdl_get_framecounter(),  base_fps,        stored_fps));

	fprintf(fp, "\n");
	const char *dir = handy_sdl_gui_get_browse_dir();
	if(dir && *dir) fprintf(fp, "rom.dir=%s\n", dir);
	for(int i = 0; i < handy_sdl_gui_recent_count(); i++)
		fprintf(fp, "rom.recent=%s\n", handy_sdl_gui_recent_get(i));

	fprintf(fp, "\n");
	{
		int mode = 0, port = 0, active = 0;
		char host[256];
		handy_sdl_comlynx_get_config(&mode, host, sizeof(host), &port);
		handy_sdl_comlynx_status(&active, NULL, NULL, NULL);

		// Same rule as above. Nothing changed since startup means whatever is
		// here came from -comlynx, so put the file's own settings back and
		// leave autostart as it was rather than latching a one-off link up.
		std::string hoststr = host;
		if(mode == base_cl_mode && port == base_cl_port && hoststr == base_cl_host)
		{
			mode    = stored_cl_mode;
			hoststr = stored_cl_host;
			port    = stored_cl_port;
			active  = stored_cl_autostart;
		}

		const char *modename = "off";
		if(mode == HANDY_COMLYNX_LISTEN)  modename = "listen";
		if(mode == HANDY_COMLYNX_CONNECT) modename = "connect";

		fprintf(fp, "comlynx.mode=%s\n", modename);
		fprintf(fp, "comlynx.host=%s\n", hoststr.c_str());
		fprintf(fp, "comlynx.port=%d\n", port);
		// Only ask for the link to come back if it was actually up.
		fprintf(fp, "comlynx.autostart=%d\n", active ? 1 : 0);
	}

	fprintf(fp, "\n");
	for(int b = 0; b < HANDY_BTN_COUNT; b++)
	{
		std::string base = "input." + handy_sdl_config_button_key(b);

		SDL_Scancode sc = handy_sdl_input_get_key(b);
		const char *kn = (sc == SDL_SCANCODE_UNKNOWN) ? "-" : SDL_GetScancodeName(sc);
		fprintf(fp, "%s.key=%s\n", base.c_str(), kn);

		int pb = handy_sdl_input_get_pad(b);
		const char *pn = "-";
		if(pb >= 0)
		{
			const char *s = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)pb);
			if(s) pn = s;
		}
		fprintf(fp, "%s.pad=%s\n", base.c_str(), pn);
	}

	fclose(fp);
}
