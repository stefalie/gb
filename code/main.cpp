// Copyright (C) 2021 Stefan Lienhard

// TODO:
// - UI timeout
// - pause popup
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

#include <commdlg.h>  // For file open dialog
#include <shellapi.h>  // For opening the website

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
#include <imgui/imgui_internal.h>
#include <imgui_club/imgui_memory_editor/imgui_memory_editor.h>

#include "dmca_sans_serif_v0900_600.h"
extern "C" {
#include "gb.h"
}

static inline void
_SDL_CheckError(const char* file, int line)
{
	if (strcmp(SDL_GetError(), "") != 0)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "File %s, line %i: %s.\n", file, line, SDL_GetError());
		SDL_ClearError();
	}
}

#ifndef NDEBUG
#define SDL_CheckError() _SDL_CheckError(__FILE__, __LINE__)
#else
#define SDL_CheckError() ((void)0)
#endif

void
_glCheckError(const char* file, int line)
{
	GLenum error;
	while ((error = glGetError()) != GL_NO_ERROR)
	{
		const char* error_str;
		switch (error)
		{
#define CASE(error) \
	case error: \
		error_str = #error; \
		break
			CASE(GL_NO_ERROR);
			CASE(GL_INVALID_ENUM);
			CASE(GL_INVALID_VALUE);
			CASE(GL_INVALID_OPERATION);
			CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
			CASE(GL_OUT_OF_MEMORY);
			CASE(GL_STACK_UNDERFLOW);
			CASE(GL_STACK_OVERFLOW);
#undef CASE
		default:
			error_str = "Unknown GL error number, should never happen.";
			break;
		}

		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "File %s, line %i: %s.\n", file, line, error_str);
	}
}

#ifndef NDEBUG
#define glCheckError() _glCheckError(__FILE__, __LINE__)
#else
#define glCheckError() ((void)0)
#endif

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
};

static const Input default_inputs[] = {
	// Keyboard
	{ "key_a", "A", Input::TYPE_KEY, { SDLK_x } },
	{ "key_b", "B", Input::TYPE_KEY, { SDLK_z } },
	{ "key_start", "Start", Input::TYPE_KEY, { SDLK_RETURN } },
	{ "key_select", "Select", Input::TYPE_KEY, { SDLK_BACKSPACE } },
	{ "key_left", "Left", Input::TYPE_KEY, { SDLK_LEFT } },
	{ "key_right", "Right", Input::TYPE_KEY, { SDLK_RIGHT } },
	{ "key_up", "Up", Input::TYPE_KEY, { SDLK_UP } },
	{ "key_down", "Down", Input::TYPE_KEY, { SDLK_DOWN } },
	// Controller
	{ "button_a", "A", Input::TYPE_BUTTON, { SDL_CONTROLLER_BUTTON_B } },
	{ "button_b", "B", Input::TYPE_BUTTON, { SDL_CONTROLLER_BUTTON_A } },
	{ "button_start", "Start", Input::TYPE_BUTTON, { SDL_CONTROLLER_BUTTON_START } },
	{ "button_select", "Select", Input::TYPE_BUTTON, { SDL_CONTROLLER_BUTTON_BACK } },
	{ "stick_horizontal", "Horizontal", Input::TYPE_AXIS, { SDL_CONTROLLER_AXIS_LEFTX } },
	{ "stick_vertical", "Vertical", Input::TYPE_AXIS, { SDL_CONTROLLER_AXIS_LEFTY } },
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

enum Stretch
{
	STRETCH_NORMAL,
	STRETCH_ASPECT_CORRECT,
	STRETCH_INTEGRAL_SCALE,
};
static const struct
{
	Stretch type;
	const char* name = NULL;
	const char* nice_name = NULL;
} stretch_options[] = {
	{ STRETCH_NORMAL, "normal", "Stretch" },
	{ STRETCH_ASPECT_CORRECT, "aspect_correct", "Aspect Correct Stretch" },
	{ STRETCH_INTEGRAL_SCALE, "integral_scale", "Integral Scale Stretch" },
};
static const size_t num_stretch_options = sizeof(stretch_options) / sizeof(stretch_options[0]);

static const struct
{
	gb_MagFilter type;
	const char* name = NULL;
	const char* nice_name = NULL;
} mag_options[] = {
	{ GB_MAG_FILTER_NONE, "none", "None" },
	{ GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X, "epx_scale2x_advmame2x", "EPX/Scale2x/AdvMAME2x" },
	{ GB_MAG_FILTER_SCALE3X_ADVMAME3X_SCALEF, "scale3x_advmame3x_scalef", "Scale3x/AdvMAME3x/ScaleF" },
	{ GB_MAG_FILTER_SCALE4X_ADVMAME4X, "scale4x_advmame4x", "Scale4x/AdvMAME4x" },
	{ GB_MAG_FILTER_XBR2, "xbr2", "xBR2" },
};
static const size_t num_mag_options = sizeof(mag_options) / sizeof(mag_options[0]);

float
DpiScale()
{
	float ddpi, hdpi, vdpi;
	SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi);
	(void)hdpi;
	(void)vdpi;
	const float dpi_scale = ddpi / 96.0f;
	return dpi_scale;
}

const uint32_t window_default_scale_factor = 5;

struct Ini
{
	uint32_t window_width = window_default_scale_factor * GB_FRAMEBUFFER_WIDTH;
	uint32_t window_height = window_default_scale_factor * GB_FRAMEBUFFER_HEIGHT;

	Stretch stretch = STRETCH_ASPECT_CORRECT;
	gb_MagFilter mag_filter = GB_MAG_FILTER_NONE;

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

			if (!strcmp(key, "window_width"))
			{
				ini.window_width = atoi(val);
			}
			else if (!strcmp(key, "window_height"))
			{
				ini.window_height = atoi(val);
			}
			else if (!strcmp(key, "stretch"))
			{
				bool found_val = false;
				for (size_t i = 0; i < num_stretch_options; ++i)
				{
					if (!strcmp(val, stretch_options[i].name))
					{
						found_val = true;
						ini.stretch = stretch_options[i].type;
						break;
					}
				}
				if (!found_val)
				{
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
							"Warning: invalid value '%s' for 'stretch' in config.ini.\n", val);
				}
			}
			else if (!strcmp(key, "mag_filter"))
			{
				bool found_val = false;
				for (size_t i = 0; i < num_mag_options; ++i)
				{
					if (!strcmp(val, mag_options[i].name))
					{
						found_val = true;
						ini.mag_filter = mag_options[i].type;
						break;
					}
				}
				if (!found_val)
				{
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
							"Warning: invalid value '%s' for 'mag_filter' in config.ini.\n", val);
				}
			}
			else
			{
				bool found_key = false;

				for (size_t i = 0; i < num_inputs; ++i)
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
		fprintf(file, "window_width=%i\n", ini->window_width);
		fprintf(file, "window_height=%i\n", ini->window_height);
		for (size_t i = 0; i < num_stretch_options; ++i)
		{
			if (ini->stretch == stretch_options[i].type)
			{
				fprintf(file, "stretch=%s\n", stretch_options[i].name);
				break;
			}
		}
		for (size_t i = 0; i < num_mag_options; ++i)
		{
			if (ini->mag_filter == mag_options[i].type)
			{
				fprintf(file, "mag_filter=%s\n", mag_options[i].name);
				break;
			}
		}
		fprintf(file, "[Input]\n");
		for (size_t i = 0; i < num_inputs; ++i)
		{
			fprintf(file, "%s=%s\n", ini->inputs[i].name, InputSdlName(&ini->inputs[i]));
		}
		fclose(file);
	}
}

struct Rom
{
	uint8_t* data = 0;
	int size = 0;
};

struct Config
{
	Ini ini;

	Rom rom;

	bool quit = false;

	Input* modify_input = NULL;

	struct Gui
	{
		bool show_gui = true;

		bool pressed_escape = false;

		bool show_quit_popup = false;
		bool show_input_config_popup = false;
		bool show_about_popup = false;
		bool show_rom_load_error = false;

		bool has_active_rom = false;

		int save_slot = 1;
		bool toggle_fullscreen = false;
		bool fullscreen = false;

		bool mag_filter_changed = true;

		bool single_step_mode = false;
		bool exec_next_step = false;
	} gui;

	struct Debugger
	{
		bool show = false;
		MemoryEditor memory_editor;
	} debug;

	struct Handles
	{
		SDL_Window* window = NULL;
		SDL_GLContext gl_context = NULL;
		SDL_GameController* controller = NULL;
		ImGuiContext* imgui = NULL;
		ImFont* font = NULL;

		SDL_Window* debug_window = NULL;
		ImGuiContext* debug_imgui = NULL;
		ImFont* debug_font = NULL;
	} handles;
};

static void
LoadRomFromFile(Config* config, gb_GameBoy* gb, const char* file_path)
{
	Rom rom = {};

	FILE* file = fopen(file_path, "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		rom.size = ftell(file);
		fseek(file, 0, SEEK_SET);

		rom.data = (uint8_t*)malloc(rom.size);
		fread(rom.data, rom.size, 1, file);
		fclose(file);
	}

	if (rom.data)
	{
		if (config->rom.data)
		{
			free(config->rom.data);
			config->rom = {};
		}
		config->rom = rom;
		if (gb_LoadRom(gb, config->rom.data, config->rom.size))
		{
			config->gui.show_rom_load_error = true;
			free(config->rom.data);
			config->rom = {};
		}
		else
		{
			config->gui.has_active_rom = true;
			config->gui.exec_next_step = false;
			gb_Reset(gb);

			// TODO: print cleanup
			SDL_Log("Loaded ROM: %s\n", gb->rom.name);

			SDL_Log("Bios instructions:\n");
			char str_buf[32];
			uint16_t addr = 0u;
			while (addr < 256)
			{
				addr += gb_PrintInstruction(gb, addr, str_buf, sizeof(str_buf));
				SDL_Log("%s\n", str_buf);
			}
			SDL_Log("Start execution:\n");
		}
	}
	else
	{
		config->gui.show_rom_load_error = true;
	}
}

// TODO(stefalie): Consider using only OpenGL 2.1 compatible functions, glCreateShaderProgram needs OpenGL 4.1.
#define GL_API_LIST(ENTRY) \
	ENTRY(void, glUseProgram, GLuint program) \
	ENTRY(GLuint, glCreateShaderProgramv, GLenum type, GLsizei count, const GLchar* const* strings) \
	ENTRY(void, glGetProgramiv, GLuint program, GLenum pname, GLint* params) \
	ENTRY(void, glGetProgramInfoLog, GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog) \
	ENTRY(GLint, glGetUniformLocation, GLuint program, const GLchar* name) \
	ENTRY(void, glUniform1i, GLint location, GLint v0) \
	ENTRY(void, glUniform2i, GLint location, GLint v0, GLint v1)

#define GL_API_DECL_ENTRY(return_type, name, ...) \
	typedef return_type __stdcall name##Proc(__VA_ARGS__); \
	name##Proc* name;
GL_API_LIST(GL_API_DECL_ENTRY)
#undef GL_API_DECL_ENTRY

static void
ImGuiOpenCenteredPopup(const char* name)
{
	// See https://github.com/ocornut/imgui/issues/2405
	const ImVec2 display_size_half = { ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f };
	ImGui::SetNextWindowPos(display_size_half, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(name);
}

static void
GuiDraw(Config* config, gb_GameBoy* gb)
{
	ImGui::PushFont(config->handles.font);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open ROM"))
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
					LoadRomFromFile(config, gb, ofn.lpstrFile);
				}
			}
			// TODO: Keep this menu element?
			if (ImGui::MenuItem("Eject ROM", NULL, false, config->gui.has_active_rom)) {
				config->gui.has_active_rom = false;
				free(config->rom.data);
				config->rom = {};
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "Esc"))
			{
				config->gui.show_quit_popup = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("System"))
		{
			if (ImGui::MenuItem("Reset"))
			{
				gb_Reset(gb);
			}
			ImGui::MenuItem("Pause", "Space", false, config->gui.has_active_rom);
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

				for (size_t i = 0; i < num_stretch_options; ++i)
				{
					if (ImGui::MenuItem(
								stretch_options[i].nice_name, NULL, config->ini.stretch == stretch_options[i].type))
					{
						config->ini.stretch = stretch_options[i].type;
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Fullscreen", NULL, config->gui.fullscreen))
				{
					config->gui.toggle_fullscreen = true;
				}
				if (ImGui::MenuItem("Reset Window Size", NULL, false))
				{
					const float dpi_scale = DpiScale();
					const int win_width = (int)(window_default_scale_factor * GB_FRAMEBUFFER_WIDTH * dpi_scale);
					const int win_height = (int)(window_default_scale_factor * GB_FRAMEBUFFER_HEIGHT * dpi_scale);
					SDL_SetWindowSize(config->handles.window, win_width, win_height);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Magnification Filter"))
			{
				for (size_t i = 0; i < num_mag_options; ++i)
				{
					if (ImGui::MenuItem(mag_options[i].nice_name, NULL, config->ini.mag_filter == mag_options[i].type))
					{
						config->ini.mag_filter = mag_options[i].type;
						config->gui.mag_filter_changed = true;
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Speed"))
			{
				ImGui::MenuItem("Default", NULL, true);
				ImGui::MenuItem("Faster");
				ImGui::MenuItem("Slower");
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
			if (ImGui::MenuItem("Open Debugger", NULL, config->debug.show))
			{
				config->debug.show = !config->debug.show;
			}
			if (ImGui::MenuItem("Single step mode", NULL, config->gui.single_step_mode))
			{
				config->gui.single_step_mode = !config->gui.single_step_mode;
			}
			if (ImGui::MenuItem("Step", "F10")) {
				config->gui.exec_next_step = true;
			}
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

	// The current (if any) pop-up will be closed if ESC is hit or the window
	// is closed. This is applies for all pop-ups except the quit popup.
	const bool close_current_popup = config->gui.pressed_escape || config->gui.show_quit_popup;

	// Use the hit ESC key only to close the quit popup only if it wasn't used
	// to close another pop-up.
	const bool any_open_popups_except_quit = config->gui.show_about_popup || config->gui.show_input_config_popup ||
			config->gui.show_rom_load_error /* || ...*/;
	const bool esc_for_quit_popup = config->gui.pressed_escape && !any_open_popups_except_quit;
	const bool close_quit_popup = config->gui.show_quit_popup && esc_for_quit_popup;
	if (esc_for_quit_popup)
	{
		// Activate quit popup if no active already.
		config->gui.show_quit_popup = true;
	}

	config->gui.pressed_escape = false;

	// TODO(stefalie): Is this really the best/only way to handle popups?

	const ImGuiWindowFlags popup_flags =
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

	// Input config popup
	if (config->gui.show_input_config_popup)
	{
		ImGuiOpenCenteredPopup("Configure Input");
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
				ImGui::Text("%s", input->nice_name);
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
		ImGuiOpenCenteredPopup("About GB");
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

	// Rom load error popup
	if (config->gui.show_rom_load_error)
	{
		ImGuiOpenCenteredPopup("Error");
	}
	if (ImGui::BeginPopupModal("Error", NULL, popup_flags))
	{
		ImGui::Text("Failed to open ROM file.");
		if (ImGui::Button("Ok") || close_current_popup)
		{
			config->gui.show_rom_load_error = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Quit pop-up
	if (config->gui.show_quit_popup)
	{
		ImGuiOpenCenteredPopup("Quit");
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

	ImGui::PopFont();
}

static void
DebuggerDraw(Config* config, gb_GameBoy* gb)
{
	(void)gb;  // TODO

	ImGui::SetCurrentContext(config->handles.debug_imgui);
	SDL_GL_MakeCurrent(config->handles.debug_window, config->handles.gl_context);
	ImGui_ImplSDL2_InitForOpenGL(config->handles.debug_window, config->handles.gl_context);

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame(config->handles.debug_window);
	ImGui::NewFrame();
	ImGui::PushFont(config->handles.debug_font);

	bool reset_dock = false;

	// Menu
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Reset Layout"))
			{
				reset_dock = true;
			}
			if (ImGui::MenuItem("Exit"))
			{
				config->debug.show = false;
			}
			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();

	{  // Dock
		// Unfortunately we can't use DockSpaceOverViewport(...) here because
		// it internally creates a window and defines a dock space id. But we
		// need inject the docker builder code in between these two steps.
		// The code here is replicated from DockSpaceOverViewport(...).

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.2f, 0.2f));
		ImGui::Begin("dockspace window", NULL, flags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspace_id = ImGui::GetID("dockspace");

		if (!ImGui::DockBuilderGetNode(dockspace_id) || reset_dock)
		{
			ImGui::DockBuilderRemoveNode(dockspace_id);
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

			ImGuiID dock_tmp_id = dockspace_id;
			ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_tmp_id, ImGuiDir_Left, 0.55f, NULL, &dock_tmp_id);
			ImGuiID dock_id_right_bottom =
					ImGui::DockBuilderSplitNode(dock_tmp_id, ImGuiDir_Down, 0.50f, NULL, &dock_tmp_id);
			ImGuiID dock_id_right_top = dock_tmp_id;

			ImGui::DockBuilderDockWindow("Hex View", dock_id_left);
			ImGui::DockBuilderDockWindow("Right", dock_id_right_top);
			ImGui::DockBuilderDockWindow("Right Tab 2", dock_id_right_top);
			ImGui::DockBuilderDockWindow("Sprites", dock_id_right_bottom);
			ImGui::DockBuilderFinish(dockspace_id);
		}

		ImGui::DockSpace(dockspace_id);
		ImGui::End();
	}

	{
		// TODO: brilliant, use to dispay next instr.
		config->debug.memory_editor.GotoAddrAndHighlight(0x100, 0x100+3);

		config->debug.memory_editor.DrawWindow("Hex View", config->rom.data, config->rom.size);

		ImGui::Begin("Right");
		ImGui::Text("text text text right");
		ImGui::End();
		ImGui::Begin("Right Tab 2");
		ImGui::Text("text text text left");
		ImGui::End();

		ImGui::Begin("Sprites");
		ImGui::Text("bla bla bla");
		ImGui::End();
	}

	// ImGui demo window
	// static bool show_demo = true;
	// if (show_demo)
	// {
	// 	ImGui::ShowDemoWindow(&show_demo);
	// }

	ImGui::PopFont();
	ImGui::Render();

	int fb_width, fb_height;
	SDL_GL_GetDrawableSize(config->handles.debug_window, &fb_width, &fb_height);
	glViewport(0, 0, fb_width, fb_height);
	glClearColor(0.75f, 0.75f, 0.75f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(config->handles.debug_window);

	// Switch contexts back.
	ImGui::SetCurrentContext(config->handles.imgui);
	SDL_GL_MakeCurrent(config->handles.window, config->handles.gl_context);
	ImGui_ImplSDL2_InitForOpenGL(config->handles.window, config->handles.gl_context);
}

#define STRINGIFY(str) STRINGIFY2(str)
#define STRINGIFY2(str) #str

// Pixel perfect pixel art magnification filter.
// I believe this even works for minification in so far that it actually applies low pass filtering.
static const char* fragment_shader_source =
		"#line " STRINGIFY(__LINE__) "\n"
		"\n"
		// TODO
		//"uniform int i_time;\n"
		"uniform ivec2 viewport_size;\n"
		"uniform ivec2 viewport_pos;\n"
		"uniform sampler2D gameboy_fb_tex;\n"
		"// TODO(stefalie): Pass the size fo gameboy_fb_tex as uniform or accept 'textureSize(...)\n"
		"// not available below version 130' warning (or 'gl_FragColor not available above version 130').\n"
		"\n"
		"void main()\n"
		"{\n"
		"	vec2 scale = vec2(viewport_size) / vec2(textureSize(gameboy_fb_tex, 0));\n"
		"\n"
		// TODO
		//"	float time = float(i_time) * 0.001;\n"
		//"	scale = scale + ((cos((time + 8.0) / 3.7) + 1.0) / 2.0) * 1.0 * scale;\n"
		"\n"
		"	vec2 mag_coord = (gl_FragCoord.xy - vec2(viewport_pos)) / scale;\n"
		"	mag_coord -= vec2(0.5);  // Temporarily remove the half texel offset.\n"
		"	vec2 mag_base = floor(mag_coord);\n"
		"	vec2 x = mag_coord - mag_base;\n"
		"	mag_base += vec2(0.5);  // Re-add the offset towards the center of the texel.\n"
		"\n"
		"	// Interpolate only over one screen pixel exactly in the middle between 'scale'-sized magnified pixels.\n"
		"	// Line equation (origin in the middle of a pixel in mag space): scale * x - 0.5 * (scale - 1.0)\n"
		"	vec2 mag_sample_uv = mag_base + clamp(x * scale - 0.5 * (scale - 1.0), 0.0, 1.0);\n"
		"	mag_sample_uv /= vec2(textureSize(gameboy_fb_tex, 0));\n"
		"\n"
		"	mag_sample_uv.y = 1.0 - mag_sample_uv.y;  // Image is upside down in tex memory.\n"
		"	float pixel_intensity = texture2D(gameboy_fb_tex, mag_sample_uv).r;\n"
		"\n"
		"	// TODO: Tone mapping etc.\n"
		"	gl_FragColor = vec4(vec3(pixel_intensity), 1.0);\n"
		"}\n";

int
main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
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


	Config config;
	config.debug.memory_editor.ReadOnly = true;
	char* save_path = SDL_GetPrefPath(NULL, "GB");

	// Load ini
	const char* ini_name = "config.ini";
	const size_t ini_path_len = strlen(save_path) + strlen(ini_name);
	char* ini_path = (char*)malloc(ini_path_len + 1);
	strcpy(ini_path, save_path);
	strcat(ini_path, ini_name);
	ini_path[ini_path_len] = '\0';
	config.ini = IniLoadOrInit(ini_path);

	const float dpi_scale = DpiScale();

	{  // Main window
		const int fb_width = (int)(config.ini.window_width * dpi_scale);
		const int fb_height = (int)(config.ini.window_height * dpi_scale);
		config.handles.window = SDL_CreateWindow("GB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, fb_width,
				fb_height, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
		if (!config.handles.window)
		{
			SDL_CheckError();
			SDL_Quit();
			exit(1);
		}
	}

	{  // Debugger window
		const int fb_debug_width = (int)(1024 * dpi_scale);
		const int fb_debug_height = (int)(768 * dpi_scale);
		config.handles.debug_window = SDL_CreateWindow("GB Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
				fb_debug_width, fb_debug_height, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
		if (!config.handles.debug_window)
		{
			SDL_DestroyWindow(config.handles.window);
			SDL_CheckError();
			SDL_Quit();
			exit(1);
		}
		SDL_HideWindow(config.handles.debug_window);
	}

	config.handles.gl_context = SDL_GL_CreateContext(config.handles.window);
	SDL_GL_MakeCurrent(config.handles.window, config.handles.gl_context);
	SDL_GL_SetSwapInterval(1);  // Enable V-sync

	config.handles.controller = NULL;

	// Setup ImGui for main window
	const float main_window_scale = 1.8f * dpi_scale;
	{
		config.handles.imgui = ImGui::CreateContext();
		ImGui::SetCurrentContext(config.handles.imgui);

		ImGui::GetStyle().ScaleAllSizes(main_window_scale);
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = NULL;
		ImGui_ImplSDL2_InitForOpenGL(config.handles.window, config.handles.gl_context);
		ImGui_ImplOpenGL2_Init();
		ImGui::StyleColorsClassic();
		ImGui::GetStyle().FrameRounding = 4.0f * main_window_scale;
		ImGui::GetStyle().WindowRounding = 4.0f * main_window_scale;
	}

	// Setup ImGui for debugger window
	const float debug_window_scale = 1.2f * dpi_scale;
	{
		SDL_GL_MakeCurrent(config.handles.debug_window, config.handles.gl_context);

		// Both ImGui contexts need to share the FontAtlas. That's because the OpenGL2 backend
		// has a 'static GLuint g_FontTexture'.
		config.handles.debug_imgui = ImGui::CreateContext(ImGui::GetIO().Fonts);
		ImGui::SetCurrentContext(config.handles.debug_imgui);

		ImGui::GetStyle().ScaleAllSizes(debug_window_scale);
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.IniFilename = NULL;
		ImGui_ImplSDL2_InitForOpenGL(config.handles.debug_window, config.handles.gl_context);
		ImGui_ImplOpenGL2_Init();
		ImGui::StyleColorsClassic();
		ImGui::GetStyle().FrameRounding = 4.0f * debug_window_scale;
		ImGui::GetStyle().WindowRounding = 4.0f * debug_window_scale;

		// Switch contexts back.
		ImGui::SetCurrentContext(config.handles.imgui);
		SDL_GL_MakeCurrent(config.handles.window, config.handles.gl_context);
		// This is unfortunately necessary (at least with the OpenGL2 backend) as there
		// is a 'static SDL_Window* g_Window'.
		ImGui_ImplSDL2_InitForOpenGL(config.handles.window, config.handles.gl_context);
	}

	{  // ImGui fonts
		// Doesn't matter which context, only accessing shared fonts.
		ImGuiIO& io = ImGui::GetIO();

		// First set the debug font. It will be used for newly created windows as default.
		// Docking might create new windows in NewFrame() if a new dock space is created
		// from floating elements. It's not possible to do PushFont before doing NewFrame,
		// that's why we choose the smaller debugger font as the default.
		const float debug_font_size = 13.0f * debug_window_scale;
		config.handles.debug_font = io.Fonts->AddFontFromMemoryCompressedTTF(
				dmca_sans_serif_v0900_600_compressed_data, dmca_sans_serif_v0900_600_compressed_size, debug_font_size);

		const float main_font_size = 13.0f * main_window_scale;
		config.handles.font = io.Fonts->AddFontFromMemoryCompressedTTF(
				dmca_sans_serif_v0900_600_compressed_data, dmca_sans_serif_v0900_600_compressed_size, main_font_size);
	}

	// OpenGL setup. Leave matrices as identity.
	GLuint shader_program;
	GLint viewport_size_loc;
	GLint viewport_pos_loc;
	GLint gb_fb_tex_loc;
	GLuint texture;
	{
		// Load OpenGL extensions.
#define GL_API_GETPTR_ENTRY(return_type, name, ...) \
	name = (name##Proc*)wglGetProcAddress(#name); \
	assert(name);
		GL_API_LIST(GL_API_GETPTR_ENTRY)
#undef GL_API_GETPTR_ENTRY

		shader_program = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragment_shader_source);
		GLint is_linked;
		glGetProgramiv(shader_program, GL_LINK_STATUS, &is_linked);
		char info_log[8192];
		GLsizei info_log_length;
		glGetProgramInfoLog(shader_program, sizeof(info_log), &info_log_length, info_log);
		if (!is_linked)
		{
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Shader compilation:\n%s.\n", info_log);
		}
#ifndef NDEBUG
		else if (info_log_length > 0)
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Shader compilation:\n%s.\n", info_log);
		}
#endif
		assert(is_linked);

		viewport_size_loc = glGetUniformLocation(shader_program, "viewport_size");
		viewport_pos_loc = glGetUniformLocation(shader_program, "viewport_pos");
		gb_fb_tex_loc = glGetUniformLocation(shader_program, "gameboy_fb_tex");
		assert(viewport_size_loc != -1);
		assert(viewport_pos_loc != -1);
		assert(gb_fb_tex_loc != -1);

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		// The texture object itself is lazily (re-)allocated later on when needed.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glEnable(GL_TEXTURE_2D);
		glCheckError();
	}

	gb_GameBoy gb = {};
	gb_Init(&gb);
	uint8_t* pixels = (uint8_t*)malloc(gb_MaxMagFramebufferSizeInBytes());
	if (argc > 1)
	{
		LoadRomFromFile(&config, &gb, argv[1]);
	}

	// Main Loop
	while (!config.quit)
	{
		// Make sure the right ImGui context gets the event.
		if (SDL_GetWindowFlags(config.handles.debug_window) & SDL_WINDOW_INPUT_FOCUS)
		{
			ImGui::SetCurrentContext(config.handles.debug_imgui);
		}

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			// TODO: Not needed for GB I think. Also modal poup wants input and then doesn't give us the keys anymore.
			// BAD.
			// ImGuiIO& io = ImGui::GetIO(); if ((io.WantCaptureMouse && (event.type == SDL_MOUSEWHEEL ||
			// event.type == SDL_MOUSEBUTTONDOWN)) ||
			//		(io.WantCaptureKeyboard && (event.type == SDL_KEYUP || event.type == SDL_KEYDOWN)))
			//{
			//	continue;
			//}

			// TODO(stefalie): Deal with Ctrl+? combinations to open ROM files. etc.

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
						(abs(event.caxis.value) > 20000)  // No idea if this sensitivity threshold is generally ok.
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
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
						SDL_GetWindowFromID(event.window.windowID) == config.handles.window)
				{
					// This can cause 1 pixel off rounding errors.
					config.ini.window_width = (int)(event.window.data1 / dpi_scale);
					config.ini.window_height = (int)(event.window.data2 / dpi_scale);
				}
				else if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
						SDL_GetWindowFromID(event.window.windowID) == config.handles.window)
				{
					SDL_RaiseWindow(config.handles.window);  // Make sure window gets focuses when closed from taskbar.
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
				for (size_t i = 0; i < num_inputs; ++i)
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
				for (size_t i = 0; i < num_inputs; ++i)
				{
					Input input = config.ini.inputs[i];
					if (input.type == Input::TYPE_AXIS && event.caxis.axis == input.sdl.axis)
					{
						SDL_Log("Moved '%s' by %i.\n", input.nice_name, event.caxis.value);
					}
				}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					if (SDL_GetWindowFlags(config.handles.window) & SDL_WINDOW_INPUT_FOCUS)
					{
						config.gui.pressed_escape = true;
					}
				}
				else if (event.key.keysym.sym == SDLK_RETURN && (SDL_GetModState() & KMOD_LALT))
				{
					config.gui.toggle_fullscreen = true;
				}
				else if (event.key.keysym.sym == SDLK_F10)
				{
					config.gui.exec_next_step = true;
				}
				else
				{
					for (size_t i = 0; i < num_inputs; ++i)
					{
						Input input = config.ini.inputs[i];
						if (input.type == Input::TYPE_KEY && event.key.keysym.sym == input.sdl.key)
						{
							SDL_Log("Pressed key '%s'.\n", input.nice_name);
						}
					}
				}
			case SDL_MOUSEMOTION:
				// TODO: Show menu for a few secs if game is running. If not running,
				// always show menu.
				break;
			case SDL_MOUSEBUTTONDOWN:
				break;
			}
		}
		ImGui::SetCurrentContext(config.handles.imgui);  // Reset (if changed)

		// Run emulator
		if (config.gui.has_active_rom && (!config.gui.single_step_mode || config.gui.exec_next_step))
		{
			config.gui.exec_next_step = false;

			const size_t num_elapsed_gb_cyles = gb_ExecuteNextInstruction(&gb);
			if (num_elapsed_gb_cyles == (size_t)-1)
			{
				char str_buf[32];
				gb_PrintInstruction(&gb, gb.cpu.pc - 1, str_buf, sizeof(str_buf));
				SDL_Log("%s\n", str_buf);
				assert(false);
			}
		}

		// OpenGL drawing
		int fb_width, fb_height;
		SDL_GL_GetDrawableSize(config.handles.window, &fb_width, &fb_height);
		glViewport(0, 0, fb_width, fb_height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		if (config.gui.has_active_rom)
		{
			int x = 0;
			int y = 0;
			int w = fb_width;
			int h = fb_height;

			const int default_width = 160;  // TODO get from elsewhere
			const int default_height = 144;  // TODO get from elsewhere
			if (config.ini.stretch == STRETCH_ASPECT_CORRECT)
			{
				const float default_aspect_ratio = (float)default_width / default_height;  // TODO get from elsewhere
				const float aspect_ratio = (float)fb_width / fb_height;
				const float scale_x = default_aspect_ratio / aspect_ratio;
				if (scale_x <= 1.0)
				{
					w = (int)roundf(scale_x * fb_width);
					x = (fb_width - w) / 2;
				}
				else
				{
					h = (int)roundf(fb_height / scale_x);
					y = (fb_height - h) / 2;
				}
				glViewport(x, y, w, h);
			}
			else if (config.ini.stretch == STRETCH_INTEGRAL_SCALE)
			{
				// We could filter with GL_NEAREST for this, but meh.
				const int scale_x = fb_width / default_width;
				const int scale_y = fb_height / default_height;
				const int scale = scale_x < scale_y ? scale_x : scale_y;  // No imin in C
				w = default_width * scale;
				h = default_height * scale;
				x = (fb_width - w) / 2;
				y = (fb_height - h) / 2;
				glViewport(x, y, w, h);
			}

			// TODO: Don't use ticks, use perf counters as shown in the Imgui example.
			// const int tick = SDL_GetTicks();  // TODO

			const gb_Framebuffer fb = gb_MagFramebuffer(&gb, config.ini.mag_filter, pixels);
			if (config.gui.mag_filter_changed)
			{
				// Re-alloc texture when mag filter changed as the size is most likely different.
				glTexImage2D(GL_TEXTURE_2D, 0, 1, fb.width, fb.height, 0, GL_RED, GL_UNSIGNED_BYTE, fb.pixels);
				config.gui.mag_filter_changed = false;
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb.width, fb.height, GL_RED, GL_UNSIGNED_BYTE, fb.pixels);
			}

			glUseProgram(shader_program);
			glUniform2i(viewport_size_loc, w, h);
			glUniform2i(viewport_pos_loc, x, y);
			glUniform1i(gb_fb_tex_loc, 0);  // Tex unit 0
			// glUniform1i(glGetUniformLocation(shader_program, "i_time"), tick);  // TODO
			glRects(-1, -1, 1, 1);
			glUseProgram(0);
			glCheckError();
		}

		// ImGui
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame(config.handles.window);
		ImGui::NewFrame();
		if (config.gui.show_gui)
		{
			GuiDraw(&config, &gb);
		}
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(config.handles.window);

		if (config.gui.toggle_fullscreen)
		{
			config.gui.toggle_fullscreen = false;
			config.gui.fullscreen = !config.gui.fullscreen;
			if (config.gui.fullscreen)
			{
				// TODO(stefalie): Consider using SDL_WINDOW_FULLSCREEN_DESKTOP instead.
				// I thought that proper fullscreen requires setting SDL_SetWindowDisplayMode(...),
				// and that these different modes need to be querried first. But hey, it seems to work as is.
				// See: https://discourse.libsdl.org/t/correct-way-to-swap-from-window-to-fullscreen/24270
				SDL_SetWindowFullscreen(config.handles.window, SDL_WINDOW_FULLSCREEN);
			}
			else
			{
				SDL_SetWindowFullscreen(config.handles.window, 0);
			}
		}

		if (config.debug.show)
		{
			if (SDL_GetWindowFlags(config.handles.debug_window) & SDL_WINDOW_HIDDEN)
			{
				SDL_ShowWindow(config.handles.debug_window);
			}
			DebuggerDraw(&config, &gb);
		}
		else
		{
			if (SDL_GetWindowFlags(config.handles.debug_window) & SDL_WINDOW_SHOWN)
			{
				SDL_HideWindow(config.handles.debug_window);
			}
		}
	}

	glDeleteTextures(1, &texture);

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(config.handles.imgui);
	ImGui::DestroyContext(config.handles.debug_imgui);

	IniSave(ini_path, &config.ini);

	// Cleanup
	free(pixels);
	if (config.rom.data)
	{
		free(config.rom.data);
	}
	free(ini_path);
	SDL_free(save_path);

	if (config.handles.controller)
	{
		SDL_GameControllerClose(config.handles.controller);
	}
	SDL_GL_DeleteContext(config.handles.gl_context);
	SDL_DestroyWindow(config.handles.debug_window);
	SDL_DestroyWindow(config.handles.window);

	SDL_Quit();

	return 0;
}
