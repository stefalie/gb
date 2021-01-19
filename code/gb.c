// TODO
#pragma warning(disable : 4100 4244 4267 4456 4996 5105)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <ShellScalingAPI.h>
#include <Windows.h>

// See note about SDL_MAIN_HANDLED in build_win_x64.bat
// #define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>  // TODO rem
#include <string.h>
#include <SDL2/SDL.h>
#include "gameboy.h"
#include "microui.h"
#include "renderer.h"


GameBoy gameboy = { 0 };

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

static char logbuf[64000];
static int logbuf_updated = 0;
static float bg[3] = { 90, 95, 100 };


static void
write_log(const char* text)
{
	if (logbuf[0])
	{
		strcat(logbuf, "\n");
	}
	strcat(logbuf, text);
	logbuf_updated = 1;
}


static void
test_window(mu_Context* ctx)
{
	/* do window */
	if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450)))
	{
		mu_Container* win = mu_get_current_container(ctx);
		win->rect.w = mu_max(win->rect.w, 240);
		win->rect.h = mu_max(win->rect.h, 300);

		/* window info */
		if (mu_header(ctx, "Window Info"))
		{
			mu_Container* win = mu_get_current_container(ctx);
			char buf[64];
			mu_layout_row(ctx, 2, (int[]){ 54, -1 }, 0);
			mu_label(ctx, "Position:");
			sprintf(buf, "%d, %d", win->rect.x, win->rect.y);
			mu_label(ctx, buf);
			mu_label(ctx, "Size:");
			sprintf(buf, "%d, %d", win->rect.w, win->rect.h);
			mu_label(ctx, buf);
		}

		/* labels + buttons */
		if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED))
		{
			mu_layout_row(ctx, 3, (int[]){ 86, -110, -1 }, 0);
			mu_label(ctx, "Test buttons 1:");
			if (mu_button(ctx, "Button 1"))
			{
				write_log("Pressed button 1");
			}
			if (mu_button(ctx, "Button 2"))
			{
				write_log("Pressed button 2");
			}
			mu_label(ctx, "Test buttons 2:");
			if (mu_button(ctx, "Button 3"))
			{
				write_log("Pressed button 3");
			}
			if (mu_button(ctx, "Popup"))
			{
				mu_open_popup(ctx, "Test Popup");
			}
			if (mu_begin_popup(ctx, "Test Popup"))
			{
				mu_button(ctx, "Hello");
				mu_button(ctx, "World");
				mu_end_popup(ctx);
			}
		}

		/* tree */
		if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED))
		{
			mu_layout_row(ctx, 2, (int[]){ 140, -1 }, 0);
			mu_layout_begin_column(ctx);
			if (mu_begin_treenode(ctx, "Test 1"))
			{
				if (mu_begin_treenode(ctx, "Test 1a"))
				{
					mu_label(ctx, "Hello");
					mu_label(ctx, "world");
					mu_end_treenode(ctx);
				}
				if (mu_begin_treenode(ctx, "Test 1b"))
				{
					if (mu_button(ctx, "Button 1"))
					{
						write_log("Pressed button 1");
					}
					if (mu_button(ctx, "Button 2"))
					{
						write_log("Pressed button 2");
					}
					mu_end_treenode(ctx);
				}
				mu_end_treenode(ctx);
			}
			if (mu_begin_treenode(ctx, "Test 2"))
			{
				mu_layout_row(ctx, 2, (int[]){ 54, 54 }, 0);
				if (mu_button(ctx, "Button 3"))
				{
					write_log("Pressed button 3");
				}
				if (mu_button(ctx, "Button 4"))
				{
					write_log("Pressed button 4");
				}
				if (mu_button(ctx, "Button 5"))
				{
					write_log("Pressed button 5");
				}
				if (mu_button(ctx, "Button 6"))
				{
					write_log("Pressed button 6");
				}
				mu_end_treenode(ctx);
			}
			if (mu_begin_treenode(ctx, "Test 3"))
			{
				static int checks[3] = { 1, 0, 1 };
				mu_checkbox(ctx, "Checkbox 1", &checks[0]);
				mu_checkbox(ctx, "Checkbox 2", &checks[1]);
				mu_checkbox(ctx, "Checkbox 3", &checks[2]);
				mu_end_treenode(ctx);
			}
			mu_layout_end_column(ctx);

			mu_layout_begin_column(ctx);
			mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
			mu_text(ctx,
					"Lorem ipsum dolor sit amet, consectetur adipiscing "
					"elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
					"ipsum, eu varius magna felis a nulla.");
			mu_layout_end_column(ctx);
		}

		/* background color sliders */
		if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED))
		{
			mu_layout_row(ctx, 2, (int[]){ -78, -1 }, 74);
			/* sliders */
			mu_layout_begin_column(ctx);
			mu_layout_row(ctx, 2, (int[]){ 46, -1 }, 0);
			mu_label(ctx, "Red:");
			mu_slider(ctx, &bg[0], 0, 255);
			mu_label(ctx, "Green:");
			mu_slider(ctx, &bg[1], 0, 255);
			mu_label(ctx, "Blue:");
			mu_slider(ctx, &bg[2], 0, 255);
			mu_layout_end_column(ctx);
			/* color preview */
			mu_Rect r = mu_layout_next(ctx);
			mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
			char buf[32];
			sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
			mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
		}

		mu_end_window(ctx);
	}
}


static void
log_window(mu_Context* ctx)
{
	if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200)))
	{
		/* output text panel */
		mu_layout_row(ctx, 1, (int[]){ -1 }, -25);
		mu_begin_panel(ctx, "Log Output");
		mu_Container* panel = mu_get_current_container(ctx);
		mu_layout_row(ctx, 1, (int[]){ -1 }, -1);
		mu_text(ctx, logbuf);
		mu_end_panel(ctx);
		if (logbuf_updated)
		{
			panel->scroll.y = panel->content_size.y;
			logbuf_updated = 0;
		}

		/* input textbox + submit button */
		static char buf[128];
		int submitted = 0;
		mu_layout_row(ctx, 2, (int[]){ -70, -1 }, 0);
		if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT)
		{
			mu_set_focus(ctx, ctx->last_id);
			submitted = 1;
		}
		if (mu_button(ctx, "Submit"))
		{
			submitted = 1;
		}
		if (submitted)
		{
			write_log(buf);
			buf[0] = '\0';
		}

		mu_end_window(ctx);
	}
}


static int
uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high)
{
	static float tmp;
	mu_push_id(ctx, &value, sizeof(value));
	tmp = *value;
	int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
	*value = tmp;
	mu_pop_id(ctx);
	return res;
}


static void
style_window(mu_Context* ctx)
{
	static struct
	{
		const char* label;
		int idx;
	} colors[] = { { "text:", MU_COLOR_TEXT }, { "border:", MU_COLOR_BORDER }, { "windowbg:", MU_COLOR_WINDOWBG },
		{ "titlebg:", MU_COLOR_TITLEBG }, { "titletext:", MU_COLOR_TITLETEXT }, { "panelbg:", MU_COLOR_PANELBG },
		{ "button:", MU_COLOR_BUTTON }, { "buttonhover:", MU_COLOR_BUTTONHOVER },
		{ "buttonfocus:", MU_COLOR_BUTTONFOCUS }, { "base:", MU_COLOR_BASE }, { "basehover:", MU_COLOR_BASEHOVER },
		{ "basefocus:", MU_COLOR_BASEFOCUS }, { "scrollbase:", MU_COLOR_SCROLLBASE },
		{ "scrollthumb:", MU_COLOR_SCROLLTHUMB }, { NULL } };

	if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240)))
	{
		int sw = mu_get_current_container(ctx)->body.w * 0.14;
		mu_layout_row(ctx, 6, (int[]){ 80, sw, sw, sw, sw, -1 }, 0);
		for (int i = 0; colors[i].label; i++)
		{
			mu_label(ctx, colors[i].label);
			uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
			uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
			uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
			uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
			mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
		}
		mu_end_window(ctx);
	}
}


static void
process_frame(mu_Context* ctx)
{
	mu_begin(ctx);
	style_window(ctx);
	log_window(ctx);
	test_window(ctx);
	mu_end(ctx);
}


static const char button_map[256] = {
	[SDL_BUTTON_LEFT & 0xff] = MU_MOUSE_LEFT,
	[SDL_BUTTON_RIGHT & 0xff] = MU_MOUSE_RIGHT,
	[SDL_BUTTON_MIDDLE & 0xff] = MU_MOUSE_MIDDLE,
};

static const char key_map[256] = {
	[SDLK_LSHIFT & 0xff] = MU_KEY_SHIFT,
	[SDLK_RSHIFT & 0xff] = MU_KEY_SHIFT,
	[SDLK_LCTRL & 0xff] = MU_KEY_CTRL,
	[SDLK_RCTRL & 0xff] = MU_KEY_CTRL,
	[SDLK_LALT & 0xff] = MU_KEY_ALT,
	[SDLK_RALT & 0xff] = MU_KEY_ALT,
	[SDLK_RETURN & 0xff] = MU_KEY_RETURN,
	[SDLK_BACKSPACE & 0xff] = MU_KEY_BACKSPACE,
};


static int
text_width(mu_Font font, const char* text, int len)
{
	if (len == -1)
	{
		len = strlen(text);
	}
	return r_get_text_width(text, len) * 1.5f;
}

static int
text_height(mu_Font font)
{
	return r_get_text_height() * 1.5f;
}

typedef unsigned
GDFS(void);

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

	GDFS* GetDpiForSystem = NULL;
	HMODULE dll = GetModuleHandleA("user32.dll");
	if (dll != INVALID_HANDLE_VALUE)
	{
		GetDpiForSystem = (GDFS*)GetProcAddress(dll, "GetDpiForSystem");
	}
	float dpi_scale = 1.0f;
	if (GetDpiForSystem)
	{
		dpi_scale = GetDpiForSystem() / 96.0f;
	}
	SDL_Log("dpi: %f\n", dpi_scale);
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

	r_init();
	mu_Context* ctx = malloc(sizeof(mu_Context));
	mu_init(ctx);
	ctx->text_width = text_width;
	ctx->text_height = text_height;
	ctx->style->size.x *= 1.5f;
	ctx->style->size.y *= 1.5f;
	ctx->style->padding *= 1.5f;
	ctx->style->spacing *= 1.5f;
	ctx->style->indent *= 1.5f;
	ctx->style->title_height *= 1.5f;
	ctx->style->scrollbar_size *= 1.5f;
	ctx->style->thumb_size *= 1.5f;

	SDL_Surface* surface = SDL_GetWindowSurface(window);
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0xFF, 0x00, 0x00));
	SDL_UpdateWindowSurface(window);

	// Main Loop
	SDL_Event event;
	bool quit = false;
	while (!quit)
	{
		// TODO note, it's a windows and not subsystem console app now, printf doesn't work.
		// fprintf(stdout, "lala\n");
		// SDL_Log("SDL_Log: Hello World\n");
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
				else
				{
					int c = key_map[event.key.keysym.sym & 0xff];
					if (c && event.type == SDL_KEYDOWN)
					{
						mu_input_keydown(ctx, c);
					}
					if (c && event.type == SDL_KEYUP)
					{
						mu_input_keyup(ctx, c);
					}
				}
				break;
			case SDL_MOUSEMOTION:
				mu_input_mousemove(ctx, event.motion.x, event.motion.y);
				break;
			case SDL_MOUSEWHEEL:
				mu_input_scroll(ctx, 0, event.wheel.y * -30);
				break;
			case SDL_TEXTINPUT:
				mu_input_text(ctx, event.text.text);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				int b = button_map[event.button.button & 0xff];
				if (b && event.type == SDL_MOUSEBUTTONDOWN)
				{
					mu_input_mousedown(ctx, event.button.x, event.button.y, b);
				}
				if (b && event.type == SDL_MOUSEBUTTONUP)
				{
					mu_input_mouseup(ctx, event.button.x, event.button.y, b);
				}
				break;
			}
			}
		}

		/* process frame */
		process_frame(ctx);

		/* render */
		r_clear(mu_color(bg[0], bg[1], bg[2], 255));
		mu_Command* cmd = NULL;
		while (mu_next_command(ctx, &cmd))
		{
			switch (cmd->type)
			{
			case MU_COMMAND_TEXT:
				r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
				break;
			case MU_COMMAND_RECT:
				r_draw_rect(cmd->rect.rect, cmd->rect.color);
				break;
			case MU_COMMAND_ICON:
				r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
				break;
			case MU_COMMAND_CLIP:
				r_set_clip_rect(cmd->clip.rect);
				break;
			}
		}
		r_present();

		SDL_GL_SwapWindow(window);
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
