// Copyright (C) 2021 Stefan Lienhard

// TODO:
// - Reset control in GUI.
// - UI timeout
// - ESC should be pause
// - make input handle function seperate.
// - make lambdas out of the OrComplainFunctions
// - deal with pause state, you might also need the previous state (if you press esc and don't quit, you want to be back
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

		// The axes are currently not customizable (and also not read/written from/to the .ini file).
		// But would you really want to map them to something else than the left stick?
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
IniLoadButtonOrComplain(const char* val, SDL_GameControllerButton* button)
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
				IniLoadButtonOrComplain(val, &result.controller.a);
			}
			else if (!strcmp(key, "controller_b"))
			{
				IniLoadButtonOrComplain(val, &result.controller.b);
			}
			else if (!strcmp(key, "controller_start"))
			{
				IniLoadButtonOrComplain(val, &result.controller.start);
			}
			else if (!strcmp(key, "controller_select"))
			{
				IniLoadButtonOrComplain(val, &result.controller.select);
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

	struct ModifyInputRequest
	{
		bool requested = false;
	} modify_input_request;

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
			ImGui::MenuItem("Open Debugger", NULL, false);
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

	// Reset ESC.
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
		// ImGui::Columns(2);
		// ImGui::Text("Left/Right");
		// ImGui::NextColumn();
		// ImGui::Text("Left Stick X-Axis");
		// ImGui::NextColumn();
		// ImGui::Text("Up/Down");
		// ImGui::NextColumn();
		// ImGui::Text("Left Stick Y-Axis");
		// ImGui::NextColumn();
		// ImGui::Text("A");
		// ImGui::NextColumn();
		// ImGui::Text("Dunno");
		// ImGui::NextColumn();
		// ImGui::Columns(1);


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

		ImGui::Text("Left");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##left_key", SDL_GetKeyName(ini->keys.left));
		ImGui::Button(label);
		ImGui::NextColumn();
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		// ImGui::Button("Left X-axis");
		snprintf(label, 64, "%s##left_button", SDL_GameControllerGetStringForAxis(ini->controller.axis_x));
		ImGui::Button(label);
		ImGui::PopStyleVar();
		ImGui::NextColumn();

		ImGui::Text("Right");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##right_key", SDL_GetKeyName(ini->keys.right));
		ImGui::Button(label);
		ImGui::NextColumn();
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		ImGui::Button("Left X-axis");
		ImGui::PopStyleVar();
		ImGui::NextColumn();

		ImGui::Text("Up");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##up_key", SDL_GetKeyName(ini->keys.up));
		ImGui::Button(label);
		ImGui::NextColumn();
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		ImGui::Button("Left Y-axis");
		ImGui::PopStyleVar();
		ImGui::NextColumn();

		ImGui::Text("Down");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##down_key", SDL_GetKeyName(ini->keys.down));
		ImGui::Button(label);
		ImGui::NextColumn();
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		ImGui::Button("Left Y-axis");
		ImGui::PopStyleVar();
		ImGui::NextColumn();

		ImGui::Text("Start");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##start_key", SDL_GetKeyName(ini->keys.start));
		ImGui::Button(label);
		ImGui::NextColumn();
		snprintf(label, 64, "%s##start_button", SDL_GameControllerGetStringForButton(ini->controller.start));
		ImGui::Button(label);
		ImGui::NextColumn();

		ImGui::Text("Select");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##select_key", SDL_GetKeyName(ini->keys.select));
		ImGui::Button(label);
		ImGui::NextColumn();
		snprintf(label, 64, "%s##select_button", SDL_GameControllerGetStringForButton(ini->controller.select));
		ImGui::Button(label);
		ImGui::NextColumn();

		ImGui::Text("A");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##a_key", SDL_GetKeyName(ini->keys.a));
		const bool is_active = config->modify_input_request.requested && true;
		if (is_active)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.41f, 0.57f, 0.62f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.41f, 0.57f, 0.62f));
		}
		if (ImGui::Button(label))
		{
			config->modify_input_request.requested = !config->modify_input_request.requested;
		}
		if (is_active)
		{
			ImGui::PopStyleColor(2);
		}
		ImGui::NextColumn();
		snprintf(label, 64, "%s##a_button", SDL_GameControllerGetStringForButton(ini->controller.a));
		ImGui::Button(label);
		ImGui::NextColumn();

		ImGui::Text("B");
		ImGui::NextColumn();
		snprintf(label, 64, "%s##b_key", SDL_GetKeyName(ini->keys.b));
		ImGui::Button(label);
		ImGui::NextColumn();
		snprintf(label, 64, "%s##b_button", SDL_GameControllerGetStringForButton(ini->controller.b));
		ImGui::Button(label);
		ImGui::NextColumn();

		ImGui::Columns(1);

		if (ImGui::Button("Done") || close_current_popup)
		{
			config->gui.show_input_config_popup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			config->ini.keys = Ini::Keys();
			config->ini.controller = Ini::Controller();
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
	// glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 320, 240, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_2D, 0, 1, 160, 144, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  // Is already default.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glEnable(GL_TEXTURE_2D);

	// TODO
	// Use pixel-perfect window 1x, 2x, 3x, 4x
	// Linear interpolate for fullscreen.
	uint32_t texture_update_buffer[320 * 240];
	uint8_t texture_update_buffer2[160 * 144];

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
	while (!config.quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			// SDL_Log("Event: %x\n", event.type);
			ImGui_ImplSDL2_ProcessEvent(&event);

			// Highjack (certain) inputs when the input config is modified.
			if (config.modify_input_request.requested)
			{
				bool event_captured = false;

				if (true /*config->modify_input_request.type == key*/ && (event.type == SDL_KEYDOWN) &&
						(event.key.keysym.sym != SDLK_ESCAPE)  // Don't allow assigning the ESC key.
				)
				{
					// TODO: Chose the right key
					config.ini.keys.a = event.key.keysym.sym;
					event_captured = true;
				}

				if (event_captured)
				{
					config.modify_input_request.requested = false;
					continue;
				}
			}


			switch (event.type)
			{
			case SDL_QUIT:
				config.gui.show_quit_popup = true;
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
					config.gui.pressed_escape = true;
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

		for (int i = 0; i < 144; i++)
		{
			for (int j = 0; j < 160; j++)
			{
				texture_update_buffer2[i * 160 + j] = (uint8_t)(i + j);
			}
		}
		// NOTE: While the binary layout between SDL and OpenGL is actually
		// the same, SDL uses SDL_PIXELFORMAT_RGB888 and OpenGL GL_BGR(A).
		// glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 320, 240, GL_BGRA, GL_UNSIGNED_BYTE, texture_update_buffer);
		const GB_FrameBuffer fb = GB_GetFrameBuffer(&gb);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 160, 144, GL_RED, GL_UNSIGNED_BYTE, fb.pixels);
		// glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 160, 144, GL_RED, GL_UNSIGNED_BYTE, texture_update_buffer2);


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
			static bool show_demo = true;
			if (show_demo)
			{
				ImGui::ShowDemoWindow(&show_demo);
			}
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
