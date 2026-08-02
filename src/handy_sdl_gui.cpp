//
// Handy/SDL - Desktop GUI
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>
#include <SDL.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_sdlrenderer2.h"

#include "handy_sdl_main.h"
#include "handy_sdl_graphics.h"
#include "handy_sdl_handling.h"
#include "handy_sdl_comlynx.h"
#include "handy_sdl_gui.h"

static int	gui_ready	= 0;
static bool	show_about	= false;
static bool	show_keys	= false;
static bool	show_browser	= false;
static bool	show_input	= false;
static bool	show_comlynx	= false;

// Edit buffers for the ComLynx panel, seeded from the live settings the first
// time it is shown so that typing does not fight with the running link.
static int	cl_mode		= HANDY_COMLYNX_LISTEN;
static char	cl_host[256]	= "127.0.0.1";
static int	cl_port		= 8100;
static bool	cl_loaded	= false;

// While non-negative, the next key or pad button pressed is bound to this
// Lynx button instead of being acted on normally.
static int	capture_button	= -1;
static bool	capture_is_pad	= false;

// ROM browser state
static std::string				browse_dir;
static std::vector<std::string>	browse_dirs;
static std::vector<std::string>	browse_files;
static int						browse_selected = -1;
static bool						browse_stale    = true;

// Most recently loaded cartridges, newest first.
static std::vector<std::string>	recent_roms;
#define RECENT_MAX	8

static const char *rom_extensions[] = { ".lnx", ".lyx", ".o", ".com", ".bin",
                                        ".zip", ".gz", NULL };


static bool handy_sdl_gui_is_rom(const std::string &name)
{
	size_t dot = name.find_last_of('.');
	if(dot == std::string::npos) return false;

	std::string ext = name.substr(dot);
	for(size_t i = 0; i < ext.size(); i++) ext[i] = tolower(ext[i]);

	for(int i = 0; rom_extensions[i]; i++)
		if(ext == rom_extensions[i]) return true;

	return false;
}

static void handy_sdl_gui_scan_dir(void)
{
	browse_dirs.clear();
	browse_files.clear();
	browse_selected = -1;
	browse_stale = false;

	DIR *d = opendir(browse_dir.c_str());
	if(d == NULL) return;

	struct dirent *e;
	while((e = readdir(d)) != NULL)
	{
		std::string name = e->d_name;
		if(name == ".") continue;
		// Skip dotfiles but keep ".." so there is always a way back up.
		if(name != ".." && name[0] == '.') continue;

		std::string full = browse_dir + "/" + name;
		struct stat st;
		if(stat(full.c_str(), &st) != 0) continue;

		if(S_ISDIR(st.st_mode))      browse_dirs.push_back(name);
		else if(handy_sdl_gui_is_rom(name)) browse_files.push_back(name);
	}
	closedir(d);

	std::sort(browse_dirs.begin(),  browse_dirs.end());
	std::sort(browse_files.begin(), browse_files.end());
}

static void handy_sdl_gui_remember(const char *path)
{
	std::string p = path;
	for(size_t i = 0; i < recent_roms.size(); i++)
	{
		if(recent_roms[i] == p) { recent_roms.erase(recent_roms.begin() + i); break; }
	}
	recent_roms.insert(recent_roms.begin(), p);
	if(recent_roms.size() > RECENT_MAX) recent_roms.resize(RECENT_MAX);
}

// Load a cartridge and, if it worked, remember it and follow it in the browser.
static void handy_sdl_gui_open(const std::string &path)
{
	if(!handy_sdl_load_rom(path.c_str())) return;

	handy_sdl_gui_remember(path.c_str());

	size_t slash = path.find_last_of('/');
	if(slash != std::string::npos && slash > 0)
	{
		browse_dir   = path.substr(0, slash);
		browse_stale = true;
	}
	show_browser = false;
}

// The menu bar only appears when the pointer is near the top of the window,
// so it stays out of the way of the game.
static int	menu_active	= 0;

#define MENU_REVEAL_ZONE	32


int handy_sdl_gui_init(void)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();
	// The emulator has no use for an imgui.ini and writing one into the
	// working directory would be a surprise.
	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	if(!ImGui_ImplSDL2_InitForSDLRenderer(mainWindow, mainRenderer)) return 0;
	if(!ImGui_ImplSDLRenderer2_Init(mainRenderer)) return 0;

	// Start the browser wherever the emulator was launched from.
	if(browse_dir.empty())
	{
		char cwd[1024];
		browse_dir = getcwd(cwd, sizeof(cwd)) ? cwd : "/";
	}

	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	gui_ready = 1;
	return 1;
}

void handy_sdl_gui_open_browser(void)
{
	show_browser = true;
	browse_stale = true;
}

void handy_sdl_gui_set_rom_dir(const char *path)
{
	if(path == NULL || *path == '\0') return;

	std::string p = path;
	size_t slash = p.find_last_of('/');
	if(slash != std::string::npos && slash > 0) browse_dir = p.substr(0, slash);

	handy_sdl_gui_remember(path);
	browse_stale = true;
}

int handy_sdl_gui_event(SDL_Event *event)
{
	if(!gui_ready) return 0;

	ImGui_ImplSDL2_ProcessEvent(event);

	ImGuiIO &io = ImGui::GetIO();

	// Rebinding: swallow the next press and turn it into a binding.
	if(capture_button >= 0)
	{
		if(!capture_is_pad && event->type == SDL_KEYDOWN)
		{
			// Escape abandons the capture rather than binding itself.
			if(event->key.keysym.scancode != SDL_SCANCODE_ESCAPE)
				handy_sdl_input_set_key(capture_button, event->key.keysym.scancode);
			capture_button = -1;
			return 1;
		}
		if(capture_is_pad && event->type == SDL_CONTROLLERBUTTONDOWN)
		{
			handy_sdl_input_set_pad(capture_button, event->cbutton.button);
			capture_button = -1;
			return 1;
		}
		// Let a click elsewhere cancel, so a capture cannot get stuck.
		if(event->type == SDL_MOUSEBUTTONDOWN) capture_button = -1;
	}

	// Dropping a cartridge on the window loads it. Handled here rather than in
	// the emulator's event switch because the path has to be freed either way.
	if(event->type == SDL_DROPFILE)
	{
		if(event->drop.file)
		{
			handy_sdl_gui_open(event->drop.file);
			SDL_free(event->drop.file);
		}
		return 1;
	}

	switch(event->type)
	{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_TEXTINPUT:
			return io.WantCaptureKeyboard;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEWHEEL:
		case SDL_MOUSEMOTION:
			return io.WantCaptureMouse;
		default:
			return 0;
	}
}

int handy_sdl_gui_wants_keyboard(void)
{
	if(!gui_ready) return 0;
	return ImGui::GetIO().WantCaptureKeyboard ? 1 : 0;
}

static void handy_sdl_gui_menubar(void)
{
	if(!ImGui::BeginMainMenuBar()) return;

	if(ImGui::BeginMenu("File"))
	{
		if(ImGui::MenuItem("Open ROM..."))
		{
			show_browser = true;
			browse_stale = true;
		}

		if(ImGui::BeginMenu("Recent", !recent_roms.empty()))
		{
			for(size_t i = 0; i < recent_roms.size(); i++)
			{
				// Show the basename; the full path is a tooltip.
				const std::string &full = recent_roms[i];
				size_t slash = full.find_last_of('/');
				std::string label = (slash == std::string::npos) ? full : full.substr(slash + 1);

				if(ImGui::MenuItem(label.c_str())) handy_sdl_gui_open(full);
				if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", full.c_str());
			}
			ImGui::EndMenu();
		}

		ImGui::Separator();
		if(ImGui::MenuItem("Reset"))       mpLynx->Reset();
		ImGui::Separator();
		if(ImGui::MenuItem("Quit", "Esc")) handy_sdl_quit();
		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("Video"))
	{
		int fs = handy_sdl_get_fullscreen();
		if(ImGui::MenuItem("Fullscreen", "", fs != 0))
			handy_sdl_set_fullscreen(!fs);

		int sm = handy_sdl_get_smoothing();
		if(ImGui::MenuItem("Smooth scaling", "", sm != 0))
			handy_sdl_set_smoothing(!sm);

		int is = handy_sdl_get_integer_scale();
		if(ImGui::MenuItem("Integer scale only", "", is != 0))
			handy_sdl_set_integer_scale(!is);

		ImGui::Separator();
		ImGui::TextDisabled("Window size");
		if(ImGui::MenuItem("1x")) handy_sdl_set_window_scale(1);
		if(ImGui::MenuItem("2x")) handy_sdl_set_window_scale(2);
		if(ImGui::MenuItem("3x")) handy_sdl_set_window_scale(3);
		if(ImGui::MenuItem("4x")) handy_sdl_set_window_scale(4);
		ImGui::Separator();
		ImGui::TextDisabled("The window can also be dragged to any size.");
		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("Input"))
	{
		if(ImGui::MenuItem("Configure controls...")) show_input = true;
		if(ImGui::MenuItem("Swap A and B"))          handy_sdl_input_swap_ab();
		if(ImGui::MenuItem("Reset to defaults"))     handy_sdl_input_defaults();
		ImGui::Separator();
		const char *padname = handy_sdl_input_pad_name();
		ImGui::TextDisabled("%s", padname ? padname : "No controller");
		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("ComLynx"))
	{
		int active = 0, peers = 0;
		handy_sdl_comlynx_status(&active, &peers, NULL, NULL);

		if(ImGui::MenuItem("Settings...")) show_comlynx = true;
		ImGui::Separator();
		if(active) ImGui::TextDisabled("Connected, %d peer%s", peers, peers==1?"":"s");
		else       ImGui::TextDisabled("Not connected");
		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("Help"))
	{
		if(ImGui::MenuItem("Keyboard controls")) show_keys  = true;
		if(ImGui::MenuItem("About"))             show_about = true;
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

static void handy_sdl_gui_browser(void)
{
	if(!show_browser) return;

	if(browse_stale) handy_sdl_gui_scan_dir();

	// Keep the browser inside the emulator window, which can be as small as
	// 160x102 scaled by one. A fixed size would put the buttons off-screen.
	ImVec2 avail = ImGui::GetMainViewport()->WorkSize;
	ImVec2 want(520.0f, 380.0f);
	if(want.x > avail.x) want.x = avail.x;
	if(want.y > avail.y) want.y = avail.y;

	ImGui::SetNextWindowSize(want, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos, ImGuiCond_Always);

	if(ImGui::Begin("Open ROM", &show_browser, ImGuiWindowFlags_NoCollapse))
	{
		// Long ROM paths are common, so wrap rather than clipping.
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", browse_dir.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Separator();

		float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
		if(ImGui::BeginChild("list", ImVec2(0, -footer), true))
		{
			int index = 0;

			for(size_t i = 0; i < browse_dirs.size(); i++, index++)
			{
				std::string label = "[" + browse_dirs[i] + "]";
				if(ImGui::Selectable(label.c_str(), browse_selected == index,
				                     ImGuiSelectableFlags_AllowDoubleClick))
				{
					browse_selected = index;
					if(ImGui::IsMouseDoubleClicked(0))
					{
						if(browse_dirs[i] == "..")
						{
							size_t slash = browse_dir.find_last_of('/');
							// Never climb past the root into an empty path.
							browse_dir = (slash == std::string::npos || slash == 0)
							             ? "/" : browse_dir.substr(0, slash);
						}
						else
						{
							if(browse_dir == "/") browse_dir += browse_dirs[i];
							else                  browse_dir += "/" + browse_dirs[i];
						}
						browse_stale = true;
					}
				}
			}

			for(size_t i = 0; i < browse_files.size(); i++, index++)
			{
				if(ImGui::Selectable(browse_files[i].c_str(), browse_selected == index,
				                     ImGuiSelectableFlags_AllowDoubleClick))
				{
					browse_selected = index;
					if(ImGui::IsMouseDoubleClicked(0))
					{
						std::string sep = (browse_dir == "/") ? "" : "/";
						handy_sdl_gui_open(browse_dir + sep + browse_files[i]);
					}
				}
			}
		}
		ImGui::EndChild();

		bool have_file = browse_selected >= (int)browse_dirs.size();

		if(!have_file) ImGui::BeginDisabled();
		if(ImGui::Button("Load"))
		{
			size_t i = (size_t)browse_selected - browse_dirs.size();
			if(i < browse_files.size())
			{
				std::string sep = (browse_dir == "/") ? "" : "/";
				handy_sdl_gui_open(browse_dir + sep + browse_files[i]);
			}
		}
		if(!have_file) ImGui::EndDisabled();

		ImGui::SameLine();
		if(ImGui::Button("Cancel")) show_browser = false;
		ImGui::SameLine();
		ImGui::TextDisabled("double-click to open, or drag a file onto the window");
	}
	ImGui::End();
}

static void handy_sdl_gui_input_window(void)
{
	if(!show_input) return;

	ImVec2 avail = ImGui::GetMainViewport()->WorkSize;
	ImVec2 want(420.0f, 300.0f);
	if(want.x > avail.x) want.x = avail.x;
	if(want.y > avail.y) want.y = avail.y;

	ImGui::SetNextWindowSize(want, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos, ImGuiCond_Always);

	if(ImGui::Begin("Controls", &show_input, ImGuiWindowFlags_NoCollapse))
	{
		const char *padname = handy_sdl_input_pad_name();
		ImGui::TextDisabled("Controller: %s", padname ? padname : "none");
		ImGui::Separator();

		if(ImGui::BeginTable("bindings", 3, ImGuiTableFlags_SizingStretchProp))
		{
			for(int i = 0; i < HANDY_BTN_COUNT; i++)
			{
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", handy_sdl_input_name(i));

				// Keyboard binding
				ImGui::TableSetColumnIndex(1);
				ImGui::PushID(i * 2);
				{
					SDL_Scancode sc = handy_sdl_input_get_key(i);
					const char *label;
					if(capture_button == i && !capture_is_pad) label = "press a key...";
					else if(sc == SDL_SCANCODE_UNKNOWN)        label = "-";
					else                                      label = SDL_GetScancodeName(sc);

					if(ImGui::Button(label, ImVec2(-1, 0)))
					{
						capture_button = i;
						capture_is_pad = false;
					}
				}
				ImGui::PopID();

				// Controller binding
				ImGui::TableSetColumnIndex(2);
				ImGui::PushID(i * 2 + 1);
				{
					int pb = handy_sdl_input_get_pad(i);
					const char *label;
					if(capture_button == i && capture_is_pad) label = "press a button...";
					else if(pb < 0)                           label = "-";
					else label = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)pb);
					if(label == NULL) label = "?";

					if(ImGui::Button(label, ImVec2(-1, 0)))
					{
						capture_button = i;
						capture_is_pad = true;
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		ImGui::Separator();
		if(ImGui::Button("Reset to defaults")) handy_sdl_input_defaults();
		ImGui::SameLine();
		if(ImGui::Button("Swap A/B"))          handy_sdl_input_swap_ab();
		ImGui::SameLine();
		if(ImGui::Button("Close"))             show_input = false;

		ImGui::TextDisabled("Escape cancels a rebind.");
	}
	ImGui::End();
}

static void handy_sdl_gui_comlynx_window(void)
{
	if(!show_comlynx) return;

	if(!cl_loaded)
	{
		handy_sdl_comlynx_get_config(&cl_mode, cl_host, sizeof(cl_host), &cl_port);
		if(cl_mode == HANDY_COMLYNX_OFF) cl_mode = HANDY_COMLYNX_LISTEN;
		cl_loaded = true;
	}

	ImVec2 avail = ImGui::GetMainViewport()->WorkSize;
	ImVec2 want(420.0f, 260.0f);
	if(want.x > avail.x) want.x = avail.x;
	if(want.y > avail.y) want.y = avail.y;

	ImGui::SetNextWindowSize(want, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos, ImGuiCond_Always);

	if(ImGui::Begin("ComLynx", &show_comlynx, ImGuiWindowFlags_NoCollapse))
	{
		int active = 0, peers = 0, rx = 0, tx = 0;
		handy_sdl_comlynx_status(&active, &peers, &rx, &tx);

		ImGui::TextWrapped("Bridges the Lynx serial port to a TCP socket as a raw "
		                   "byte pipe. Use it to link two emulators or to attach a "
		                   "bridge such as a FujiNet BoIP channel.");
		ImGui::Separator();

		ImGui::RadioButton("Listen for peers", &cl_mode, HANDY_COMLYNX_LISTEN);
		ImGui::SameLine();
		ImGui::RadioButton("Connect to peer", &cl_mode, HANDY_COMLYNX_CONNECT);

		if(cl_mode == HANDY_COMLYNX_CONNECT)
		{
			ImGui::SetNextItemWidth(200);
			ImGui::InputText("Host", cl_host, sizeof(cl_host));
		}
		ImGui::SetNextItemWidth(120);
		ImGui::InputInt("Port", &cl_port);
		if(cl_port < 1)     cl_port = 1;
		if(cl_port > 65535) cl_port = 65535;

		ImGui::Separator();

		if(ImGui::Button(active ? "Reconnect" : "Connect"))
			handy_sdl_comlynx_start(cl_mode, cl_host, cl_port);

		ImGui::SameLine();
		if(!active) ImGui::BeginDisabled();
		if(ImGui::Button("Disconnect")) handy_sdl_comlynx_stop();
		if(!active) ImGui::EndDisabled();

		ImGui::SameLine();
		bool tr = handy_sdl_comlynx_get_trace() != 0;
		if(ImGui::Checkbox("Log bytes", &tr)) handy_sdl_comlynx_trace(tr ? 1 : 0);

		ImGui::Separator();
		if(active)
		{
			ImGui::Text("Status : %d peer%s connected", peers, peers==1?"":"s");
			ImGui::Text("Traffic: %d B/s in, %d B/s out", rx, tx);
			ImGui::TextDisabled("The Lynx link runs at about 5700 B/s.");
		}
		else
		{
			ImGui::Text("Status : not connected");
		}
	}
	ImGui::End();
}

static void handy_sdl_gui_windows(void)
{
	handy_sdl_gui_browser();
	handy_sdl_gui_input_window();
	handy_sdl_gui_comlynx_window();

	if(show_keys)
	{
		ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
		if(ImGui::Begin("Keyboard controls", &show_keys))
		{
			ImGui::Text("Joypad          Arrow keys");
			ImGui::Text("A               Z");
			ImGui::Text("B               X");
			ImGui::Text("Pause           Return");
			ImGui::Text("Option 1        F1");
			ImGui::Text("Option 2        F2");
			ImGui::Text("Power off       Escape");
			ImGui::Separator();
			ImGui::TextWrapped("A is the left hand key and B the right hand one, "
			                   "which is the opposite way round to the letters on "
			                   "the console itself.");
		}
		ImGui::End();
	}

	if(show_about)
	{
		ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
		if(ImGui::Begin("About Handy/SDL", &show_about))
		{
			ImGui::Text("Handy/SDL %s", HANDY_SDL_VERSION);
			ImGui::Text("Based upon %s by Keith Wilkins", HANDY_VERSION);
			ImGui::Separator();
			ImGui::TextWrapped("Atari Lynx emulator. Originally by Keith Wilkins, "
			                   "ported to SDL by the SDLemu team.");
		}
		ImGui::End();
	}
}

void handy_sdl_gui_frame(void)
{
	if(!gui_ready) return;

	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// Reveal the menu bar when the pointer goes near the top of the window,
	// and keep it up while a menu or window is actually being used.
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	if(my < MENU_REVEAL_ZONE) menu_active = 1;
	else if(!ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
		menu_active = 0;

	if(menu_active || show_keys || show_about || show_browser || show_input || show_comlynx)
	{
		SDL_ShowCursor(SDL_ENABLE);
		handy_sdl_gui_menubar();
	}
	else if(!handy_sdl_get_fullscreen())
	{
		SDL_ShowCursor(SDL_DISABLE);
	}

	handy_sdl_gui_windows();

	ImGui::Render();
}

void handy_sdl_gui_draw(void)
{
	if(!gui_ready) return;
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), mainRenderer);
}

void handy_sdl_gui_close(void)
{
	if(!gui_ready) return;
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	gui_ready = 0;
}
