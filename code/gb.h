// Copyright (C) 2021 Stefan Lienhard

// TODO: insert cmd to compile this -c -std=c11 /c /std:c11

#include <stdbool.h>
#include <stdint.h>

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
} GB_GameBoy;

void
GB_Init(GB_GameBoy* gb);

void
GB_Reset(GB_GameBoy* gb);

bool
GB_LoadRom(GB_GameBoy* gb, const uint8_t* rom);

typedef struct GB_FrameBuffer
{
	uint16_t width;
	uint16_t height;
	const uint8_t* pixels;
} GB_FrameBuffer;

GB_FrameBuffer
GB_GetFrameBuffer(const GB_GameBoy* gb /* GB_MagFilter mag_filter*/);

typedef struct
{
	uint8_t r, g, b;
} GB_Pixel;

enum gb_MagFilter
{
	GB_MAG_FILTER_WHATEVER,
};

size_t
gb_MagFramebufferSizeInBytes(enum gb_MagFilter mag_filter);

void
gb_MagFramebuffer(const GB_GameBoy* gb, enum gb_MagFilter mag_filter, uint8_t* mag_image);

