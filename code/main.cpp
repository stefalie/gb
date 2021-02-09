// Copyright (C) 2021 Stefan Lienhard

// TODO:
// - Reset control in GUI.
// - UI timeout

#define _CRT_SECURE_NO_WARNINGS
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

struct Ini
{
	// TODO: If the opendialog initialDir actually persist between reboots, then we can remove
	// this option and set 'lpstrInitialDir' to NULL. And that then raises the question if we even need the ini loading
	// code. Meh :-(
	char last_opened_path[1024] = {};
	int mag_filter = 0;
	int screen_size = 0;

	struct Keys
	{
		SDL_Keycode a = SDLK_x;
		SDL_Keycode b = SDLK_z;
		SDL_Keycode start = SDLK_RETURN;
		SDL_Keycode select = SDLK_BACKSPACE;
		SDL_Keycode left = SDLK_LEFT;
		SDL_Keycode right = SDLK_RIGHT;
		SDL_Keycode up = SDLK_UP;
		SDL_Keycode down = SDLK_DOWN;
	} keys;

	struct Controller
	{
		SDL_GameControllerButton a = SDL_CONTROLLER_BUTTON_B;
		SDL_GameControllerButton b = SDL_CONTROLLER_BUTTON_A;
		SDL_GameControllerButton start = SDL_CONTROLLER_BUTTON_START;
		SDL_GameControllerButton select = SDL_CONTROLLER_BUTTON_BACK;

		// The axes are currently not customizable (and also not read/written from/into the .ini file).
		// But would you really want to map to something else than the right stick?
		// TODO(stefalie): Deal with this.
		SDL_GameControllerAxis axis_x = SDL_CONTROLLER_AXIS_LEFTX;
		SDL_GameControllerAxis axis_y = SDL_CONTROLLER_AXIS_LEFTY;
	} controller;
};

// TODO: I don't like these 2 functions, can they be merged into a key/value parser which is taken out of the ini
// parser.
static void
IniLoadKeyboardKeyOrComplain(const char* val, SDL_Keycode* key)
{
	const SDL_Keycode ini_key = SDL_GetKeyFromName(val);
	if (ini_key != SDLK_UNKNOWN)
	{
		*key = ini_key;
	}
	else
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Warning: unknown keyboard key '%s' in config.ini.\n", val);
	}
}

static void
IniLoadButtonComplain(const char* val, SDL_GameControllerButton* button)
{
	const SDL_GameControllerButton ini_button = SDL_GameControllerGetButtonFromString(val);
	if (ini_button != SDL_CONTROLLER_BUTTON_INVALID)
	{
		*button = ini_button;
	}
	else
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Warning: unknown controller button '%s' in config.ini.\n", val);
	}
}

// TODO(stefalie): Consider trimming whitespace off key and value.
// TODO(stefalie): This doesn't deal with escaped special characters.
static Ini
IniLoadOrInit(const char* ini_path)
{
	SDL_Log("pref path: %s\n", ini_path);  // TODO
	Ini result;

	FILE* file = fopen(ini_path, "r");
	if (file)
	{
		char tmp[1024];
		while (true)
		{
			char* line = fgets(tmp, 1024, file);
			if (!line)
			{
				break;
			}

			char* comment = strchr(line, '#');
			if (comment)
			{
				*comment = '\0';
			}

			char* header = strchr(line, '[');
			if (header)
			{
				continue;
			}

			char* key = strtok(line, "=");
			if (!key || (strlen(key) == 0))
			{
				SDL_LogWarn(
						SDL_LOG_CATEGORY_APPLICATION, "Warning: no/invalid key on line '%s' in config.ini.\n", line);
				continue;
			}

			char* val = strtok(NULL, "\n");
			if (!val || (strlen(val) == 0))
			{
				continue;
			}

			if (!strcmp(key, "last_opened_path"))
			{
				strcpy(result.last_opened_path, val);
			}
			else if (!strcmp(key, "mag_filter"))
			{
				// TODO: List options.
				result.mag_filter = atoi(val);
			}
			else if (!strcmp(key, "screen_size"))
			{
				// TODO
			}
			else if (!strcmp(key, "key_a"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.a);
			}
			else if (!strcmp(key, "key_b"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.b);
			}
			else if (!strcmp(key, "key_start"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.start);
			}
			else if (!strcmp(key, "key_select"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.select);
			}
			else if (!strcmp(key, "key_left"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.left);
			}
			else if (!strcmp(key, "key_right"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.right);
			}
			else if (!strcmp(key, "key_up"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.up);
			}
			else if (!strcmp(key, "key_down"))
			{
				IniLoadKeyboardKeyOrComplain(val, &result.keys.down);
			}
			else if (!strcmp(key, "controller_a"))
			{
				IniLoadButtonComplain(val, &result.controller.a);
			}
			else if (!strcmp(key, "controller_b"))
			{
				IniLoadButtonComplain(val, &result.controller.b);
			}
			else if (!strcmp(key, "controller_start"))
			{
				IniLoadButtonComplain(val, &result.controller.start);
			}
			else if (!strcmp(key, "controller_select"))
			{
				IniLoadButtonComplain(val, &result.controller.select);
			}
			else
			{
				SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Warning: unknown key '%s' in config.ini.\n", key);
			}
		}
	}

	return result;
}

static void
IniSave(const char* ini_path, const Ini* ini)
{
	FILE* file = fopen(ini_path, "w");
	assert(file);
	if (file)
	{
		fprintf(file, "[General]\n");
		fprintf(file, "last_opened_path=%s\n", ini->last_opened_path);
		fprintf(file, "mag_filter=%i\n", ini->mag_filter);
		fprintf(file, "screen_size=%i\n", ini->screen_size);
		fprintf(file, "[Input]\n");
		fprintf(file, "key_a=%s\n", SDL_GetKeyName(ini->keys.a));
		fprintf(file, "key_b=%s\n", SDL_GetKeyName(ini->keys.b));
		fprintf(file, "key_select=%s\n", SDL_GetKeyName(ini->keys.select));
		fprintf(file, "key_start=%s\n", SDL_GetKeyName(ini->keys.start));
		fprintf(file, "key_left=%s\n", SDL_GetKeyName(ini->keys.left));
		fprintf(file, "key_right=%s\n", SDL_GetKeyName(ini->keys.right));
		fprintf(file, "key_up=%s\n", SDL_GetKeyName(ini->keys.up));
		fprintf(file, "key_down=%s\n", SDL_GetKeyName(ini->keys.down));
		fprintf(file, "controller_a=%s\n", SDL_GameControllerGetStringForButton(ini->controller.a));
		fprintf(file, "controller_b=%s\n", SDL_GameControllerGetStringForButton(ini->controller.b));
		fprintf(file, "controller_select=%s\n", SDL_GameControllerGetStringForButton(ini->controller.select));
		fprintf(file, "controller_start=%s\n", SDL_GameControllerGetStringForButton(ini->controller.start));
		fclose(file);
	}
}

struct Rom
{
	// TODO
	uint8_t* data = 0;
	int size = 0;
};

struct Config
{
	char* save_path = NULL;
	Ini ini;

	Rom rom;

	struct Gui
	{
		bool show_gui = true;
		bool has_active_rom = false;
		int save_slot = 1;
	} gui;
};

static Rom
LoadRomFromFile(const char* file_path)
{
	Rom result;

	FILE* file = fopen(file_path, "rb");
	if (file)
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
GuiDraw(Config* config, GB_GameBoy* gb)
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
				ofn.lpstrInitialDir = NULL;
				ofn.lpstrTitle = "Open GameBoy ROM";
				ofn.Flags = OFN_FILEMUSTEXIST;
				if (GetOpenFileNameA(&ofn))
				{
					SDL_Log("Selected ROM: %s\n", ofn.lpstrFile);

					Rom rom = LoadRomFromFile(ofn.lpstrFile);
					if (rom.data)
					{
						if (config->rom.data)
						{
							free(config->rom.data);
						}
						config->rom = rom;
						GB_LoadRom(gb, config->rom.data);
						GB_Reset(gb);
					}
					else
					{
						// TODO: Show error overlay instead.
						SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "ERROR: Couldn't load ROM '%s'.\n", ofn.lpstrFile);
					}
				}
			}
			ImGui::MenuItem("Close", NULL, false, config->gui.has_active_rom);
			ImGui::Separator();
			ImGui::MenuItem("Exit");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("System"))
		{
			ImGui::MenuItem("Reset", NULL, false, config->gui.has_active_rom);
			ImGui::MenuItem("Pause", "Ctrl+P", false, config->gui.has_active_rom);
			ImGui::Separator();
			ImGui::MenuItem("Save", "F5", false, config->gui.has_active_rom);
			ImGui::MenuItem("Load", "F7", false, config->gui.has_active_rom);
			if (ImGui::BeginMenu("Save Slot"))
			{
				char slot_name[7] = { 'S', 'l', 'o', 't', ' ', 'X', '\0' };
				for (int i = 1; i <= 5; ++i)
				{
					slot_name[5] = '0' + (char)i;
					if (ImGui::MenuItem(slot_name, NULL, config->gui.save_slot == i))
					{
						config->gui.save_slot = i;
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
	// SDL_GL_MakeCurrent(window, gl_context);  // Not necessary. TODO
	SDL_GL_SetSwapInterval(1);  // Enable V-sync

	SDL_GameController* controller = NULL;

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

	Config config;
	config.save_path = SDL_GetPrefPath(NULL, "gb");

	// Load ini
	const char* ini_name = "config.ini";
	const size_t ini_path_len = strlen(config.save_path) + strlen(ini_name);
	char* ini_path = (char*)malloc(ini_path_len + 1);
	strcpy(ini_path, config.save_path);
	strcat(ini_path, ini_name);
	ini_path[ini_path_len] = '\0';
	config.ini = IniLoadOrInit(ini_path);

	GB_GameBoy gb = {};
	GB_Init(&gb);


	// SDL_GameController* controller = NULL;
	// for (int i = 0; i < SDL_NumJoysticks(); ++i)
	//{
	//	if (SDL_IsGameController(i))
	//	{
	//		controller = SDL_GameControllerOpen(i);
	//		if (controller)
	//		{
	//			break;
	//		}
	//		else
	//		{
	//			fprintf(stderr, "Could not open gamecontroller %i: %s\n", i, SDL_GetError());
	//		}
	//	}
	//}

	// Main Loop
	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			SDL_Log("Event: %x\n", event.type);
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.type)
			{
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_CONTROLLERDEVICEADDED:
				// TODO(stefalie): This allows only for 1 controller, the user should be able to chose.
				if (!controller)
				{
					controller = SDL_GameControllerOpen(event.cdevice.which);
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				assert(controller);
				SDL_GameControllerClose(controller);
				controller = NULL;
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				if (event.cbutton.button == config.ini.controller.a)
				{
					SDL_Log("Pressed button 'A'.\n");
				}
				else if (event.cbutton.button == config.ini.controller.b)
				{
					SDL_Log("Pressed button 'B'.\n");
				}
				else if (event.cbutton.button == config.ini.controller.start)
				{
					SDL_Log("Pressed button 'Start'.\n");
				}
				else if (event.cbutton.button == config.ini.controller.select)
				{
					SDL_Log("Pressed button 'Select'.\n");
				}
				break;
			case SDL_CONTROLLERBUTTONUP:
				// TODO:
				break;
			case SDL_CONTROLLERAXISMOTION:
				if (event.cbutton.button == config.ini.controller.axis_x)
				{
					SDL_Log("Moved 'X-axis' by %i\n", event.caxis.value);
				}
				else if (event.cbutton.button == config.ini.controller.axis_y)
				{
					SDL_Log("Moved 'Y-axis' by %i\n", event.caxis.value);
				}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					quit = true;
				}
				else if (event.key.keysym.sym == config.ini.keys.a)
				{
					SDL_Log("Pressed key 'A'.\n");
				}
				else if (event.key.keysym.sym == config.ini.keys.b)
				{
					SDL_Log("Pressed key 'B'.\n");
				}
				else if (event.key.keysym.sym == config.ini.keys.start)
				{
					SDL_Log("Pressed key 'Start'.\n");
				}
				else if (event.key.keysym.sym == config.ini.keys.select)
				{
					SDL_Log("Pressed key 'Select'.\n");
				}
				else if (event.key.keysym.sym == config.ini.keys.left)
				{
					SDL_Log("Pressed key 'Left'.\n");
				}
				else if (event.key.keysym.sym == config.ini.keys.right)
				{
					SDL_Log("Pressed key 'Right'.\n");
				}
				else if (event.key.keysym.sym == config.ini.keys.up)
				{
					SDL_Log("Pressed key 'Up'.\n");
				}
				else if (event.key.keysym.sym == config.ini.keys.down)
				{
					SDL_Log("Pressed key 'Down'.\n");
				}
			case SDL_MOUSEMOTION:
				// TODO: Show menu for a few secs if game is running. If not running, always show menu.
				break;
			case SDL_MOUSEBUTTONDOWN:
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
		if (config.gui.show_gui)
		{
			GuiDraw(&config, &gb);
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

	// Save ini
	IniSave(ini_path, &config.ini);

	// Cleanup
	if (config.rom.data)
	{
		free(config.rom.data);
	}
	free(ini_path);
	SDL_free(config.save_path);

	if (controller)
	{
		SDL_GameControllerClose(controller);
	}
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
