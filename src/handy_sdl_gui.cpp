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
#include <SDL.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_sdlrenderer2.h"

#include "handy_sdl_main.h"
#include "handy_sdl_graphics.h"
#include "handy_sdl_gui.h"

static int	gui_ready	= 0;
static bool	show_about	= false;
static bool	show_keys	= false;

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

	gui_ready = 1;
	return 1;
}

int handy_sdl_gui_event(SDL_Event *event)
{
	if(!gui_ready) return 0;

	ImGui_ImplSDL2_ProcessEvent(event);

	ImGuiIO &io = ImGui::GetIO();

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

	if(ImGui::BeginMenu("Help"))
	{
		if(ImGui::MenuItem("Keyboard controls")) show_keys  = true;
		if(ImGui::MenuItem("About"))             show_about = true;
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

static void handy_sdl_gui_windows(void)
{
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

	if(menu_active || show_keys || show_about)
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
