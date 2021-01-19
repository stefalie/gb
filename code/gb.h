// Copyright (C) 2021 Stefan Lienhard

#include <stdint.h>

typedef struct GB_GameBoy
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