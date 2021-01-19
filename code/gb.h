// Copyright (C) 2021 Stefan Lienhard

// TODO: insert cmd to compile this -c -std=c11 /c /std:c11

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

typedef struct
{
	uint8_t r, g, b;
} GB_Pixel;
