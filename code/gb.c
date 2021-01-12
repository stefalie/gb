// See note about SDL_MAIN_HANDLED in build_win_x64.bat
// #define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "gameboy.h"

GameBoy gameboy = { 0 };

static inline void
_SDL_CheckError(const char* file, int line)
{
	if (strcmp(SDL_GetError(), "") != 0)
	{
		fprintf(stderr, "ERROR: file %s, line %i: %s.\n", file, line, SDL_GetError());
		SDL_ClearError();
	}
}

#ifndef NDEBUG
#define SDL_CheckError() _SDL_CheckError(__FILE__, __LINE__)
#else
#define SDL_CheckError() ((void)0)
#endif

int
main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		SDL_CheckError();
		exit(1);
	}

	SDL_Window* window =
			SDL_CreateWindow("GB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 512, 512, SDL_WINDOW_SHOWN);
	if (!window)
	{
		SDL_CheckError();
		SDL_Quit();
		exit(1);
	}

	SDL_Surface* surface = SDL_GetWindowSurface(window);
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xFF, 0x00, 0x00));
	SDL_UpdateWindowSurface(window);

	// Main Loop
	SDL_Event event;
	bool quit = false;
	while (!quit)
	{
		// TODO note, it's a windows and not subsystem console app now, printf doesn't work.
		fprintf(stdout, "lala\n");
		SDL_Log("SDL_Log: Hello World\n");
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
			}
		}
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
