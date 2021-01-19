// Copyright (C) 2021 Stefan Lienhard

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#pragma warning(push)
#pragma warning(disable : 5105)
#include <ShellScalingAPI.h>
#pragma warning(pop)
#include <Windows.h>

// See note about SDL_MAIN_HANDLED in build_win_x64.bat
// #define SDL_MAIN_HANDLED
#include <assert.h>
#include <imgui.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>

extern "C" {
#include "gb.h"
}

static inline void
_SDL_CheckError(const char* file, int line)
{
	if (strcmp(SDL_GetError(), "") != 0)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "ERROR: file %s, line %i: %s.\n", file, line, SDL_GetError());
		SDL_ClearError();
	}
}

#ifndef NDEBUG
#define SDL_CheckError() _SDL_CheckError(__FILE__, __LINE__)
#else
#define SDL_CheckError() ((void)0)
#endif

// Programatically enable high-DPI mode:
// https://nlguillemot.wordpress.com/2016/12/11/high-dpi-rendering/
// https://discourse.libsdl.org/t/sdl-getdesktopdisplaymode-resolution-reported-in-windows-10-when-using-app-scaling/22389/3
typedef HRESULT(WINAPI SetProcessDpiAwarenessType)(PROCESS_DPI_AWARENESS dpi_awareness);
static void
EnableHighDpiAwareness()
{
	void* shcore_dll = SDL_LoadObject("Shcore.dll");
	assert(shcore_dll);
	if (shcore_dll)
	{
		SetProcessDpiAwarenessType* SetProcessDpiAwareness =
				(SetProcessDpiAwarenessType*)SDL_LoadFunction(shcore_dll, "SetProcessDpiAwareness");
		assert(SetProcessDpiAwareness);
		if (SetProcessDpiAwareness)
		{
			SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
		}
	}
}

int
main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	EnableHighDpiAwareness();

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		SDL_CheckError();
		exit(1);
	}
	// SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
	SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Couldn't load 'Shcore.dll'.\n");

	const char* pref_path = SDL_GetPrefPath(NULL, "gb");
	SDL_Log("pref path: %s\n", pref_path);

	SDL_Window* window = SDL_CreateWindow("GB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
			SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
	if (!window)
	{
		SDL_CheckError();
		SDL_Quit();
		exit(1);
	}

	int fb_width, fb_height;
	SDL_GetWindowSize(window, &fb_width, &fb_height);
	SDL_Log("w: %i, h: %i\n", fb_width, fb_height);
	SDL_GL_GetDrawableSize(window, &fb_width, &fb_height);
	SDL_Log("w: %i, h: %i\n", fb_width, fb_height);

	float ddpi, hdpi, vdpi;
	SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi);
	ddpi /= 96.0f;
	hdpi /= 96.0f;
	vdpi /= 96.0f;
	SDL_Log("dpi: %f %f %f\n", ddpi, hdpi, vdpi);

	SDL_Rect display_bounds;
	SDL_GetDisplayUsableBounds(0, &display_bounds);
	SDL_Log("bounds: %i %i\n", display_bounds.w, display_bounds.h);

	SDL_GL_CreateContext(window);

	SDL_Surface* surface = SDL_GetWindowSurface(window);
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xFF, 0x00, 0x00));
	SDL_UpdateWindowSurface(window);

	GB_GameBoy gb = { 0 };
	GB_Init(&gb);

	// Main Loop
	SDL_Event event;
	bool quit = false;
	while (!quit)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					quit = true;
				}
				break;
			case SDL_MOUSEMOTION:
				// TODO: Show menu for a few secs if game is running. If not running, always show menu.
				break;
			case SDL_MOUSEBUTTONDOWN:
				break;
			case SDL_MOUSEBUTTONUP:
				break;
			}
		}

		SDL_GL_SwapWindow(window);
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
