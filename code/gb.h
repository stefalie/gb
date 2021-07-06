// Copyright (C) 2021 Stefan Lienhard

// TODO: insert cmd to compile this -c -std=c11 /c /std:c11

#include <stdbool.h>
#include <stdint.h>

#define GB_FRAMEBUFFER_WIDTH 160
#define GB_FRAMEBUFFER_HEIGHT 144

typedef struct
{
	// TODO(stefalie): this is all just tmp
	struct CPU
	{
		uint16_t registers[20];
	} cpu;

	struct Memory
	{
		uint8_t bytes[1000];
	} memory;

	struct Framebuffer
	{
		uint8_t pixels[GB_FRAMEBUFFER_WIDTH * GB_FRAMEBUFFER_HEIGHT];
	} framebuffer;
} gb_GameBoy;

void
gb_Init(gb_GameBoy* gb);

void
gb_Reset(gb_GameBoy* gb);

bool
gb_LoadRom(gb_GameBoy* gb, const uint8_t* rom);

typedef struct gb_Framebuffer
{
	uint16_t width;
	uint16_t height;
	const uint8_t* pixels;
} gb_Framebuffer;

typedef struct
{
	uint8_t r, g, b;
} gb_Pixel;

// Magnification filters for the framebuffer
typedef enum gb_MagFilter
{
	GB_MAG_FILTER_NONE,
	GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X,
	GB_MAG_FILTER_SCALE3X_ADVMAME3X_SCALEF,
	GB_MAG_FILTER_SCALE4X_ADVMAME4X,
	GB_MAG_FILTER_HQ2X,
	GB_MAG_FILTER_HQ3X,
	GB_MAG_FILTER_HQ4X,
	GB_MAG_FILTER_XBR,
	GB_MAG_FILTER_SUPERXBR,
	GB_MAG_FILTER_MAX_VALUE,
} gb_MagFilter;

// Returns the framebuffer size when using a specific magnification filter.
uint32_t
gb_MagFramebufferSizeInBytes(gb_MagFilter mag_filter);

// Returns the max framebuffer size for all magnification filters.
uint32_t
gb_MaxMagFramebufferSizeInBytes();

// Magnify 'gb's default framebuffer with a given filter and stores in user
// provided buffer. Summaries of the different filters can be found here:
// - https://en.wikipedia.org/wiki/Pixel-art_scaling_algorithms
// - https://www.scale2x.it
// - McGuire, Gagiu; 2021; MMPX Style-Preserving Pixel-Art Magnification
gb_Framebuffer
gb_MagFramebuffer(const gb_GameBoy* gb, gb_MagFilter mag_filter, uint8_t* pixels);

