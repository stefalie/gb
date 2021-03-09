// Copyright (C) 2021 Stefan Lienhard

// TODO:
// - Reset control in GUI.
// - UI timeout
// - ESC should be pause
// - make input handle function seperate.
// - make lambdas out of the OrComplainFunctions
// - deal with pause state, you might also need the previous state (if you press
// esc and don't quit, you want to be back
//   in the state you were at before pressing esc).

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>  // For opening the website
#pragma warning(push)
#pragma warning(disable : 5105)
#include <ShellScalingAPI.h>  // For DPI scaling
#pragma warning(pop)
#include <commdlg.h>  // For file open dialog

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
// See note about SDL_MAIN_HANDLED in build_win_x64.bat
// #define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <imgui/backends/imgui_impl_opengl2.h>
#include <imgui/backends/imgui_impl_sdl.h>
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

struct Input
{
	enum InputType
	{
		TYPE_KEY,
		TYPE_BUTTON,
		TYPE_AXIS,
	};

	const char* name = NULL;
	const char* nice_name = NULL;
	InputType type;
	union
	{
		SDL_Keycode key;
		SDL_GameControllerButton button;
		SDL_GameControllerAxis axis;
	} sdl;
} asdf;

static Input default_inputs[] = {
	// Keyboard
	{ "key_a", "A", Input::TYPE_KEY, SDLK_x },
	{ "key_b", "B", Input::TYPE_KEY, SDLK_z },
	{ "key_start", "Start", Input::TYPE_KEY, SDLK_RETURN },
	{ "key_select", "Select", Input::TYPE_KEY, SDLK_BACKSPACE },
	{ "key_left", "Left", Input::TYPE_KEY, SDLK_LEFT },
	{ "key_right", "Right", Input::TYPE_KEY, SDLK_RIGHT },
	{ "key_up", "Up", Input::TYPE_KEY, SDLK_UP },
	{ "key_down", "Down", Input::TYPE_KEY, SDLK_DOWN },
	// Controller
	{ "button_a", "A", Input::TYPE_BUTTON, SDL_CONTROLLER_BUTTON_B },
	{ "button_b", "B", Input::TYPE_BUTTON, SDL_CONTROLLER_BUTTON_A },
	{ "button_start", "Start", Input::TYPE_BUTTON, SDL_CONTROLLER_BUTTON_START },
	{ "button_select", "Select", Input::TYPE_BUTTON, SDL_CONTROLLER_BUTTON_BACK },
	{ "stick_horizontal", "Horizontal", Input::TYPE_AXIS, SDL_CONTROLLER_AXIS_LEFTX },
	{ "stick_vertical", "Vertical", Input::TYPE_AXIS, SDL_CONTROLLER_AXIS_LEFTY },
};
static const size_t num_inputs = sizeof(default_inputs) / sizeof(default_inputs[0]);

static const char*
InputSdlName(const Input* input)
{
	const char* sdl_name = NULL;
	switch (input->type)
	{
	case Input::TYPE_KEY:
		sdl_name = SDL_GetKeyName(input->sdl.key);
		break;
	case Input::TYPE_BUTTON:
		sdl_name = SDL_GameControllerGetStringForButton(input->sdl.button);
		break;
	case Input::TYPE_AXIS:
		sdl_name = SDL_GameControllerGetStringForAxis(input->sdl.axis);
		break;
	}
	return sdl_name;
}

struct Ini
{
	// TODO: If the opendialog initialDir actually persist between reboots, then
	// we can remove this option and set 'lpstrInitialDir' to NULL. And that then
	// raises the question if we even need the ini loading code. Meh :-(
	// char last_opened_path[1024] = {};
	int mag_filter = 0;
	int screen_size = 0;

	Input inputs[14] = {
		default_inputs[0],
		default_inputs[1],
		default_inputs[2],
		default_inputs[3],
		default_inputs[4],
		default_inputs[5],
		default_inputs[6],
		default_inputs[7],
		default_inputs[8],
		default_inputs[9],
		default_inputs[10],
		default_inputs[11],
		default_inputs[12],
		default_inputs[13],
	};
};

// TODO(stefalie): Consider trimming whitespace off key and value.
// TODO(stefalie): This doesn't deal with escaped special characters.
static Ini
IniLoadOrInit(const char* ini_path)
{
	SDL_Log("pref path: %s\n", ini_path);  // TODO
	Ini ini;

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

			/*if (!strcmp(key, "last_opened_path"))
			{
				strcpy(ini.last_opened_path, val);
			}
			else*/
			if (!strcmp(key, "mag_filter"))
			{
				// TODO: List options.
				ini.mag_filter = atoi(val);
			}
			else if (!strcmp(key, "screen_size"))
			{
				// TODO
			}
			else
			{
				bool found_key = false;

				for (int i = 0; i < num_inputs; ++i)
				{
					if (!strcmp(key, ini.inputs[i].name))
					{
						found_key = true;

						bool unkown_sdl_input = false;

						if (ini.inputs[i].type == Input::TYPE_KEY)
						{
							const SDL_Keycode sdl_key = SDL_GetKeyFromName(val);
							if (sdl_key != SDLK_UNKNOWN)
							{
								ini.inputs[i].sdl.key = sdl_key;
							}
							else
							{
								unkown_sdl_input = true;
							}
						}
						else if (ini.inputs[i].type == Input::TYPE_BUTTON)
						{
							const SDL_GameControllerButton sdl_button = SDL_GameControllerGetButtonFromString(val);
							if (sdl_button != SDL_CONTROLLER_BUTTON_INVALID)
							{
								ini.inputs[i].sdl.button = sdl_button;
							}
							else
							{
								unkown_sdl_input = true;
							}
						}
						else /* if (ini.inputs[i].type == Input::TYPE_AXIS) */
						{
							const SDL_GameControllerAxis sdl_axis = SDL_GameControllerGetAxisFromString(val);
							if (sdl_axis != SDL_CONTROLLER_AXIS_INVALID)
							{
								ini.inputs[i].sdl.axis = sdl_axis;
							}
							else
							{
								unkown_sdl_input = true;
							}
						}

						if (unkown_sdl_input)
						{
							SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
									"Warning: unknown input '%s' for '%s' in config.ini.\n", val, key);
						}
					}
				}

				if (!found_key)
				{
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Warning: unknown key '%s' in config.ini.\n", key);
				}
			}
		}
	}

	return ini;
}

static void
IniSave(const char* ini_path, const Ini* ini)
{
	FILE* file = fopen(ini_path, "w");
	assert(file);
	if (file)
	{
		fprintf(file, "[General]\n");
		// fprintf(file, "last_opened_path=%s\n", ini->last_opened_path);
		fprintf(file, "mag_filter=%i\n", ini->mag_filter);
		fprintf(file, "screen_size=%i\n", ini->screen_size);
		fprintf(file, "[Input]\n");
		for (int i = 0; i < num_inputs; ++i)
		{
			fprintf(file, "%s=%s\n", ini->inputs[i].name, InputSdlName(&ini->inputs[i]));
		}
		fclose(file);
	}
}

struct Rom
{
	// TODO
	uint8_t* data = 0;
	int size = 0;
};

enum State
{
	STATE_EMPTY,
	STATE_RUNNING,
	STATE_PAUSED,
};

struct Config
{
	char* save_path = NULL;
	Ini ini;

	Rom rom;
	State state;

	bool quit = false;

	Input* modify_input = NULL;

	struct Gui
	{
		bool show_gui = true;

		bool pressed_escape = false;

		bool show_quit_popup = false;
		bool show_input_config_popup = false;
		bool show_about_popup = false;

		bool has_active_rom = false;
		int save_slot = 1;
	} gui;

	struct Debugger
	{
		bool show = false;
		int fb_width = 1024;
		int fb_height = 768;

	} debug;

	struct Handles
	{
		SDL_Window* window = NULL;
		SDL_GLContext gl_context = NULL;
		SDL_GameController* controller = NULL;
		ImGuiContext* imgui = NULL;

		SDL_Window* debug_window = NULL;
		ImGuiContext* debug_imgui = NULL;
	} handles;
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
			if (ImGui::MenuItem("Exit", "Esc"))
			{
				config->gui.show_quit_popup = true;
			}
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
			if (ImGui::MenuItem("Configure Input"))
			{
				config->gui.show_input_config_popup = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug"))
		{
			if (ImGui::MenuItem("Open Debugger", NULL, false))
			{
				config->debug.show = true;
			}
			ImGui::MenuItem("Step", "F10");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("Website"))
			{
				// https://discourse.libsdl.org/t/does-sdl2-have-function-to-open-url-in-browser/22730/3
				ShellExecute(NULL, "open", "https://github.com/stefalie/gb", NULL, NULL, SW_SHOWNORMAL);
			}
			if (ImGui::MenuItem("About"))
			{
				config->gui.show_about_popup = true;
			}
			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();

	// The current (if any) popup will be closed if ESC is hit or the window
	// is closed. This is applies for all popups except the quit popup.
	const bool close_current_popup = config->gui.pressed_escape || config->gui.show_quit_popup;

	// Use the hit ESC key only to close the quit popup only if it wasn't used
	// to close another popup.
	const bool any_open_popups_except_quit =
			config->gui.show_about_popup || config->gui.show_input_config_popup /* || ...*/;
	const bool esc_for_quit_popup = config->gui.pressed_escape && !any_open_popups_except_quit;
	const bool close_quit_popup = config->gui.show_quit_popup && esc_for_quit_popup;
	if (esc_for_quit_popup)
	{
		// Activate quit popup if no active already.
		config->gui.show_quit_popup = true;
	}

	config->gui.pressed_escape = false;

	// TODO(stefalie): Is this really the best/only way to handle popups?

	const ImGuiWindowFlags popup_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

	// Input config popup
	if (config->gui.show_input_config_popup)
	{
		ImGui::OpenPopup("Configure Input");
	}
	if (ImGui::BeginPopupModal("Configure Input", NULL, popup_flags))
	{
		ImGui::Text("Click the buttons to change the input bindings.");

		ImGui::Columns(3);
		Ini* ini = &config->ini;
		char label[64];

		ImGui::Text("");
		ImGui::NextColumn();
		ImGui::Text("Keyboard");
		ImGui::NextColumn();
		ImGui::Text("Controller");
		ImGui::NextColumn();

		ImGui::Separator();

		const int input_modify_order[] = { 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 12, 6, 13, 7, 13 };
		for (int i = 0; i < 16; ++i)
		{
			Input* input = &ini->inputs[input_modify_order[i]];

			if (i % 2 == 0)
			{
				ImGui::Text(input->nice_name);
				ImGui::NextColumn();
			}

			snprintf(label, 64, "%s##modify_%i", InputSdlName(input), i);

			const bool is_active = config->modify_input == input;
			if (is_active)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.41f, 0.57f, 0.62f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.41f, 0.57f, 0.62f));
			}
			if (ImGui::Button(label))
			{
				config->modify_input = is_active ? NULL : input;
			}
			if (is_active)
			{
				ImGui::PopStyleColor(2);
			}
			ImGui::NextColumn();
		}

		ImGui::Columns(1);

		if (ImGui::Button("Done") || close_current_popup)
		{
			config->gui.show_input_config_popup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			memcpy(config->ini.inputs, default_inputs, sizeof(default_inputs));
		}
		ImGui::EndPopup();
	}

	// About popup
	if (config->gui.show_about_popup)
	{
		ImGui::OpenPopup("About GB");
	}
	if (ImGui::BeginPopupModal("About GB", NULL, popup_flags))
	{
		ImGui::Text("GB");
		ImGui::Separator();
		ImGui::Text(
				"GB is a simple GameBoy emulator\n"
				"and debugger for Windows created\n"
				"by Stefan Lienhard in 2021.");
		ImGui::Text("https://github.com/stefalie/gb");
		if (ImGui::Button("Cancel") || close_current_popup)
		{
			config->gui.show_about_popup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Quit popup
	if (config->gui.show_quit_popup)
	{
		ImGui::OpenPopup("Quit");
	}
	if (ImGui::BeginPopupModal("Quit", NULL, popup_flags))
	{
		ImGui::Text("Do you really want to quit GB?");
		if (ImGui::Button("Quit"))
		{
			config->quit = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || close_quit_popup)
		{
			config->gui.show_quit_popup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static void
DebuggerCreateDestroyDraw(Config* config, GB_GameBoy* gb)
{
	(void)gb;  // TODO

	// Create or delete debugger window/context on the fly as needed.
	if (!config->handles.debug_window && config->debug.show)
	{
		config->handles.debug_window = SDL_CreateWindow("GB Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
				config->debug.fb_width, config->debug.fb_height, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
		if (!config->handles.debug_window)
		{
			SDL_CheckError();
		}
		else
		{
			SDL_GL_MakeCurrent(config->handles.debug_window, config->handles.gl_context);

			// Both ImGui contexts need to share the FontAtlas. That's because the OpenGL2 backend
			// has a 'static GLuint g_FontTexture'.
			config->handles.debug_imgui = ImGui::CreateContext(ImGui::GetIO().Fonts);
			ImGui::SetCurrentContext(config->handles.debug_imgui);

			// SDL_Log("dpi scale: %f\n", dpi_scale);
			//ImGui::GetStyle().ScaleAllSizes(1.0f);  // dpi_scale * 2.0f);
			//ImGuiIO& io = ImGui::GetIO();
			//io.IniFilename = NULL;
			// io.Fonts->AddFontFromMemoryCompressedTTF(dmca_sans_serif_v0900_600_compressed_data,
			//		dmca_sans_serif_v0900_600_compressed_size, 13.0f);
			ImGui_ImplSDL2_InitForOpenGL(config->handles.debug_window, config->handles.gl_context);
			ImGui_ImplOpenGL2_Init();

			ImGui::StyleColorsClassic();
			// ImGui::GetStyle().FrameRounding = 4.0f;
		ImGui::SetCurrentContext(config->handles.imgui);

			SDL_GL_MakeCurrent(config->handles.window, config->handles.gl_context);
		}
	}
	else if (config->handles.debug_window && !config->debug.show)
	{
		SDL_GL_MakeCurrent(config->handles.debug_window, config->handles.gl_context);

		ImGui::DestroyContext(config->handles.debug_imgui);
		config->handles.debug_imgui = NULL;
		SDL_DestroyWindow(config->handles.debug_window);
		config->handles.debug_window = NULL;

		SDL_GL_MakeCurrent(config->handles.window, config->handles.gl_context);
	}

	if (config->debug.show)
	{
		SDL_GL_MakeCurrent(config->handles.debug_window, config->handles.gl_context);
		glViewport(0, 0, config->debug.fb_width, config->debug.fb_height);
		glClearColor(0.75f, 0.75f, 0.75f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui::SetCurrentContext(config->handles.debug_imgui);
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame(config->handles.debug_window);
		ImGui::NewFrame();
		static bool show_demo = true;
		if (show_demo)
		{
			ImGui::ShowDemoWindow(&show_demo);
		}
		ImGui::Render();
		// glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		ImGui::SetCurrentContext(config->handles.imgui);

		SDL_GL_SwapWindow(config->handles.debug_window);
		SDL_GL_MakeCurrent(config->handles.window, config->handles.gl_context);
	}
}

int
main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	EnableHighDpiAwareness();
	// The high DPI behavior is a bit strange, at least on Windows.
	// Both SDL_GetWindowSize and SDL_GL_GetDrawableSize always return the same
	// size which is in contradiction with their docs:
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

	Config config;
	config.save_path = SDL_GetPrefPath(NULL, "GB");

	config.handles.window = SDL_CreateWindow("GB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, fb_width, fb_height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
	if (!config.handles.window)
	{
		SDL_CheckError();
		SDL_Quit();
		exit(1);
	}
	config.handles.gl_context = SDL_GL_CreateContext(config.handles.window);
	SDL_GL_SetSwapInterval(1);  // Enable V-sync

	config.handles.controller = NULL;

	// Setup ImGui:
	config.handles.imgui = ImGui::CreateContext();
	ImGui::SetCurrentContext(config.handles.imgui);
	SDL_Log("dpi scale: %f\n", dpi_scale);
	ImGui::GetStyle().ScaleAllSizes(dpi_scale * 2.0f);
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;
	io.Fonts->AddFontFromMemoryCompressedTTF(
			dmca_sans_serif_v0900_600_compressed_data, dmca_sans_serif_v0900_600_compressed_size, 25.0f * dpi_scale);
	ImGui_ImplSDL2_InitForOpenGL(config.handles.window, config.handles.gl_context);
	ImGui_ImplOpenGL2_Init();
	ImGui::StyleColorsClassic();
	ImGui::GetStyle().FrameRounding = 4.0f;

	// TODO(stefalie): This should be true when no ROM is loaded or if the game is
	// paused or if the mouse has been moved no longer than 2-3 s ago.

	// OpenGL setup. Leave matrices as identity.
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, 1, 160, 144, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glEnable(GL_TEXTURE_2D);

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

	// Main Loop
	while (!config.quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			// SDL_Log("Event: %x\n", event.type);
			ImGui_ImplSDL2_ProcessEvent(&event);

			// TODO: Deal with enter + ctrl
			// Really?

			// Highjack (certain) inputs when the input config is modified.
			if (config.modify_input)
			{
				bool event_captured = false;

				if (event.type == SDL_KEYDOWN && config.modify_input->type == Input::TYPE_KEY &&
						(event.key.keysym.sym != SDLK_ESCAPE)  // Don't allow assigning the ESC key.
				)
				{
					config.modify_input->sdl.key = event.key.keysym.sym;
					event_captured = true;
				}
				else if (event.type == SDL_CONTROLLERBUTTONDOWN && config.modify_input->type == Input::TYPE_BUTTON)
				{
					config.modify_input->sdl.button = (SDL_GameControllerButton)event.cbutton.button;
					event_captured = true;
				}
				else if (event.type == SDL_CONTROLLERAXISMOTION && config.modify_input->type == Input::TYPE_AXIS &&
						(abs(event.caxis.value) > 20000)  // No idea if this sensitivity
														  // threshold is generally ok.
				)
				{
					config.modify_input->sdl.axis = (SDL_GameControllerAxis)event.caxis.axis;
					event_captured = true;
				}

				if (event_captured)
				{
					config.modify_input = NULL;
					continue;
				}
			}

			switch (event.type)
			{
			case SDL_QUIT:
				config.gui.show_quit_popup = true;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
						SDL_GetWindowFromID(event.window.windowID) == config.handles.window)
				{
					config.gui.show_quit_popup = true;
				}
				else if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
						SDL_GetWindowFromID(event.window.windowID) == config.handles.debug_window)
				{
					config.debug.show = false;
				}
				break;
			case SDL_CONTROLLERDEVICEADDED:
				// TODO(stefalie): This allows only for 1 controller, the user should
				// be able to chose.
				if (!config.handles.controller)
				{
					config.handles.controller = SDL_GameControllerOpen(event.cdevice.which);
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				assert(config.handles.controller);
				SDL_GameControllerClose(config.handles.controller);
				config.handles.controller = NULL;
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				for (int i = 0; i < num_inputs; ++i)
				{
					Input input = config.ini.inputs[i];
					if (input.type == Input::TYPE_BUTTON && event.cbutton.button == input.sdl.button)
					{
						SDL_Log("Pressed button '%s'.\n", input.nice_name);
					}
				}
				break;
			case SDL_CONTROLLERBUTTONUP:
				// TODO:
				break;
			case SDL_CONTROLLERAXISMOTION:
				for (int i = 0; i < num_inputs; ++i)
				{
					Input input = config.ini.inputs[i];
					if (input.type == Input::TYPE_AXIS && event.caxis.axis == input.sdl.axis)
					{
						SDL_Log("Moved '%s' by %i.\n", input.nice_name, event.caxis.value);
					}
				}
				break;
			case SDL_KEYDOWN:
				for (int i = 0; i < num_inputs; ++i)
				{
					Input input = config.ini.inputs[i];
					if (input.type == Input::TYPE_KEY && event.key.keysym.sym == input.sdl.key)
					{
						SDL_Log("Pressed key '%s'.\n", input.nice_name);
					}
				}
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					config.gui.pressed_escape = true;
				}
			case SDL_MOUSEMOTION:
				// TODO: Show menu for a few secs if game is running. If not running,
				// always show menu.
				break;
			case SDL_MOUSEBUTTONDOWN:
				break;
			}
		}


		glViewport(0, 0, fb_width, fb_height);  // TODO
		{  // Draw framebuffer
			// TODO: Don't use ticks, use perf counters as shown in the Imgui example.
			// const int tick = SDL_GetTicks();

			// NOTE: While the binary layout between SDL and OpenGL is actually
			// the same, SDL uses SDL_PIXELFORMAT_RGB888 and OpenGL GL_BGR(A).
			// glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 320, 240, GL_BGRA,
			// GL_UNSIGNED_BYTE, texture_update_buffer);
			const GB_FrameBuffer fb = GB_GetFrameBuffer(&gb);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 160, 144, GL_RED, GL_UNSIGNED_BYTE, fb.pixels);

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
		}

		// ImGui
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame(config.handles.window);
		ImGui::NewFrame();
		if (config.gui.show_gui)
		{
			GuiDraw(&config, &gb);
			static bool show_demo = true;
			if (show_demo)
			{
				ImGui::ShowDemoWindow(&show_demo);
			}
		}
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(config.handles.window);

		DebuggerCreateDestroyDraw(&config, &gb);
	}

	glDeleteTextures(1, &texture);

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(config.handles.imgui);

	// Save ini
	IniSave(ini_path, &config.ini);

	// Cleanup
	if (config.rom.data)
	{
		free(config.rom.data);
	}
	free(ini_path);
	SDL_free(config.save_path);

	if (config.handles.controller)
	{
		SDL_GameControllerClose(config.handles.controller);
	}
	SDL_GL_DeleteContext(config.handles.gl_context);
	SDL_DestroyWindow(config.handles.window);

	SDL_Quit();

	return 0;
}
