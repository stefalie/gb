// Copyright (C) 2021 Stefan Lienhard

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 5105)
#include <ShellScalingAPI.h>
#pragma warning(pop)
#include <commdlg.h>

// See note about SDL_MAIN_HANDLED in build_win_x64.bat
// #define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <imgui/examples/imgui_impl_opengl2.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <imgui/imgui.h>
#include "dmca_sans_serif_v0900_600.h"
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

struct GuiState
{
	bool show_gui = true;
	bool has_active_rom = false;
	int save_slot = 1;
};

struct Rom
{
	uint8_t* data;
	int size;
};

static Rom
LoadRomFromFile(const char* file_path)
{
	Rom result = {};

	FILE* file;
	errno_t err = fopen_s(&file, file_path, "rb");
	if (err == 0)
	{
		fseek(file, 0, SEEK_END);
		result.size = ftell(file);
		fseek(file, 0, SEEK_SET);

		result.data = (uint8_t*)malloc(result.size);
		fread(result.data, result.size, 1, file);
		fclose(file);
	}

	return result;
}

static void
GuiDraw(GuiState* gui_state)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open ROM", "Ctrl+O"))
			{
				char file_path[MAX_PATH] = {};

				OPENFILENAME ofn = {};
				ofn.lStructSize = sizeof(ofn);
				ofn.lpstrFilter = "GameBoy (*.gb)\0*.gb\0All (*.*)\0*.*\0";
				ofn.lpstrFile = file_path;
				ofn.nMaxFile = sizeof(file_path);
				ofn.lpstrInitialDir = NULL;  // TODO keep that in your settings
				ofn.lpstrTitle = "Open GameBoy ROM";
				ofn.Flags = OFN_FILEMUSTEXIST;
				if (GetOpenFileNameA(&ofn))
				{
					SDL_Log("Selected ROM: %s\n", ofn.lpstrFile);
				}

				// TODO: load binary (could fail)
				// TODO: Reset gb
				// TODO: Make static functions Open ROM, reset, maybe stop?
			}
			ImGui::MenuItem("Close", NULL, false, gui_state->has_active_rom);
			ImGui::Separator();
			ImGui::MenuItem("Exit");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("System"))
		{
			ImGui::MenuItem("Reset", NULL, false, gui_state->has_active_rom);
			ImGui::MenuItem("Pause", "Ctrl+P", false, gui_state->has_active_rom);
			ImGui::Separator();
			ImGui::MenuItem("Save", "F5", false, gui_state->has_active_rom);
			ImGui::MenuItem("Load", "F7", false, gui_state->has_active_rom);
			if (ImGui::BeginMenu("Save Slot"))
			{
				char slot_name[7] = { 'S', 'l', 'o', 't', ' ', 'X', '\0' };
				for (int i = 1; i <= 5; ++i)
				{
					slot_name[5] = '0' + (char)i;
					if (ImGui::MenuItem(slot_name, NULL, gui_state->save_slot == i))
					{
						gui_state->save_slot = i;
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Options"))
		{
			if (ImGui::BeginMenu("Screen Size"))
			{
				ImGui::MenuItem("1x", NULL, false);
				ImGui::MenuItem("2x", NULL, false);
				ImGui::MenuItem("3x", NULL, true);
				ImGui::MenuItem("Fullscreen", NULL, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Magnification Filter"))
			{
				ImGui::MenuItem("Nearest", NULL, true);
				ImGui::MenuItem("EPX/Scale2x/AdvMAME2x", NULL, true);
				ImGui::MenuItem("Scale3x/AdvMAME3x/ScaleF", NULL, false);
				ImGui::MenuItem("hqnx", NULL, false);
				ImGui::MenuItem("Super xBR", NULL, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Speed"))
			{
				ImGui::MenuItem("Default", NULL, true);
				ImGui::MenuItem("Faster", "Ctrl++");
				ImGui::MenuItem("Slower", "Ctrl+-");
				ImGui::MenuItem("Unthrottled", NULL, false);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			ImGui::MenuItem("Website");
			// TODO: https://discourse.libsdl.org/t/does-sdl2-have-function-to-open-url-in-browser/22730/2
			// TODO: Do some overlay, check the demo
			ImGui::MenuItem("About");
			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();
}

int
main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	EnableHighDpiAwareness();
	// The high DPI behavior is a bit strange, at least on Windows.
	// Both SDL_GetWindowSize and SDL_GL_GetDrawableSize always return the same size
	// which is in contradiction with their docs:
	// https://wiki.libsdl.org/SDL_GetWindowSize
	// https://wiki.libsdl.org/SDL_GL_GetDrawableSize
	// With high DPI enabled, the window on the screen simply appears smaller.

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0)
	{
		SDL_CheckError();
		exit(1);
	}

	const char* pref_path = SDL_GetPrefPath(NULL, "gb");
	SDL_Log("pref path: %s\n", pref_path);

	float ddpi, hdpi, vdpi;
	SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi);
	(void)hdpi;
	(void)vdpi;
	const float dpi_scale = ddpi / 96.0f;
	const int scale_factor = 6;
	const int window_width = 160 * scale_factor;
	const int window_height = 144 * scale_factor;
	const int fb_width = (int)(window_width * dpi_scale);
	const int fb_height = (int)(window_height * dpi_scale);

	SDL_Window* window = SDL_CreateWindow("GB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, fb_width, fb_height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
	if (!window)
	{
		SDL_CheckError();
		SDL_Quit();
		exit(1);
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	// SDL_GL_MakeCurrent(window, gl_context);  // Not necessary.
	SDL_GL_SetSwapInterval(1);  // Enable V-sync

	// Setup ImGui:
	ImGui::CreateContext();
	SDL_Log("dpi scale: %f\n", dpi_scale);
	ImGui::GetStyle().ScaleAllSizes(dpi_scale * 2.0f);
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.Fonts->AddFontFromMemoryCompressedTTF(
			dmca_sans_serif_v0900_600_compressed_data, dmca_sans_serif_v0900_600_compressed_size, 25.0f * dpi_scale);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL2_Init();
	ImGui::StyleColorsClassic();
	ImGui::GetStyle().FrameRounding = 4.0f;
	GuiState gui_state;
	// TODO(stefalie): This should be true when no ROM is loaded or if the game is paused
	// or if the mouse has been moved no longer than 2-3 s ago.

	// OpenGL setup. Leave matrices as identity.
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 320, 240, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  // Is already default.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glEnable(GL_TEXTURE_2D);

	// TODO
	// Use pixel-perfect window 1x, 2x, 3x, 4x
	// Linear interpolate for fullscreen.
	uint32_t texture_update_buffer[320 * 240];

	GB_GameBoy gb = {};
	GB_Init(&gb);

	// Main Loop
	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
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

		// TODO: Don't use ticks, use perf counters as shown in the Imgui example.
		const int tick = SDL_GetTicks();
		for (int i = 0; i < 240; i++)
		{
			for (int j = 0; j < 320; j++)
			{
				texture_update_buffer[i * 320 + j] = i * i + j * j + tick;
			}
		}
		// NOTE: While the binary layout between SDL and OpenGL is actually
		// the same, SDL uses SDL_PIXELFORMAT_RGB888 and OpenGL GL_BGR(A).
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 320, 240, GL_BGRA, GL_UNSIGNED_BYTE, texture_update_buffer);

		// Ideally we could mix SDL_Renderer* with pure OpenGL calls, but that seems
		// to be tricker than it should be.
		glClear(GL_COLOR_BUFFER_BIT);
		glBegin(GL_TRIANGLE_STRIP);
		{
			// Tex coords are flipped upside down. SDL uses upper left, OpenGL
			// lower left as origin.
			glTexCoord2f(0.0f, 0.0f);
			glVertex2f(-1.0f, 1.0);

			glTexCoord2f(0.0f, 1.0f);
			glVertex2f(-1.0f, -1.0f);

			glTexCoord2f(1.0f, 0.0f);
			glVertex2f(1.0f, 1.0f);

			glTexCoord2f(1.0f, 1.0f);
			glVertex2f(1.0f, -1.0f);
		}
		glEnd();

		// ImGui
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();
		if (gui_state.show_gui)
		{
			GuiDraw(&gui_state);
			// ImGui::ShowDemoWindow(&show_gui);
		}
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		// glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		// glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
	}

	glDeleteTextures(1, &texture);

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
