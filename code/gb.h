// Copyright (C) 2022 Stefan Lienhard

// TODO: insert cmd to compile this -c -std=c11 /c /std:c11

#include <stdbool.h>
#include <stdint.h>

#define GB_FRAMEBUFFER_WIDTH 160
#define GB_FRAMEBUFFER_HEIGHT 144

typedef struct
{
	// The CPU conains only the registers
	// Note thate the GameBoy uses Big Endian
	struct CPU
	{
		union
		{
			struct
			{
				uint8_t f;
				uint8_t a;
			};
			uint16_t af;
		};
		union
		{
			struct
			{
				uint8_t c;
				uint8_t b;
			};
			uint16_t bc;
		};
		union
		{
			struct
			{
				uint8_t e;
				uint8_t d;
			};
			uint16_t de;
		};
		union
		{
			struct
			{
				uint8_t l;
				uint8_t h;
			};
			uint16_t hl;
		};

		// struct
		//{
		//	uint8_t waste : 4;
		//	uint8_t carry : 1;
		//	uint8_t half_carry : 1;
		//	uint8_t operation : 1;
		//	uint8_t zero : 1;
		//} flags;
		uint16_t pc;  // Program counter
		uint16_t sp;  // Stack pointer
	} cpu;

	struct Memory
	{
		bool bios_mapped;
		// TODO
		uint8_t wram[8192];  // 8 KiB
		uint8_t vram[8192];  // 8 KiB
	} memory;

	struct Framebuffer
	{
		uint8_t pixels[GB_FRAMEBUFFER_WIDTH * GB_FRAMEBUFFER_HEIGHT];
	} framebuffer;

	struct Rom
	{
		char name[16];
		const uint8_t* data;
		uint32_t num_bytes;
	} rom;
} gb_GameBoy;

// TODO rem
void
gb_Init(gb_GameBoy* gb);

// void
// gb_Reset(gb_GameBoy* gb);

// Returns true in error case if the ROM cannot be loaded, is broken,
// or is not for GameBoy.
// TODO(stefalie): Consider returning an error code of what went wrong.
bool
gb_LoadRom(gb_GameBoy* gb, const uint8_t* rom, uint32_t num_bytes);

// Read a byte/word from the GameBoy's memory space at 'addr'.
uint8_t
gb_MemoryReadByte(const gb_GameBoy* gb, uint16_t addr);
uint16_t
gb_MemoryReadWord(const gb_GameBoy* gb, uint16_t addr);

// Resets the GameBoy.
void
gb_Reset(gb_GameBoy* gb);

// A GameBoy assembly instruction.
typedef struct
{
	uint8_t opcode;
	uint8_t num_operand_bytes;
	union
	{
		uint8_t operand_byte;
		uint16_t operand_word;
	};
	uint8_t num_machine_cycles;
} gb_Instruction;

// Fetches assembly instruction at 'addr' (which needs to point to a valid instruction).
gb_Instruction
gb_FetchInstruction(const gb_GameBoy* gb, uint16_t addr);

// Disassembles 'inst' into 'str_buf'. Appends a NUL suffix.
// A 32 byte buffer is sufficient for any instruction.
// Returns the string length of the written disassembly (not counting the NUL suffix).
size_t
gb_DisassembleInstruction(gb_Instruction inst, char str_buf[], size_t str_buf_len);

// Executes the next instruction and returns the number of GameBoy machines cycles
// it takes to execute that instruction.
size_t
gb_ExecuteNextInstruction(gb_GameBoy* gb);

typedef struct gb_Framebuffer
{
	uint16_t width;
	uint16_t height;
	const uint8_t* pixels;
} gb_Framebuffer;

// Magnification filters for the framebuffer
// For general overview, see:
// https://en.wikipedia.org/wiki/Pixel-art_scaling_algorithms
typedef enum gb_MagFilter
{
	GB_MAG_FILTER_NONE,
	GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X,
	GB_MAG_FILTER_SCALE3X_ADVMAME3X_SCALEF,
	GB_MAG_FILTER_SCALE4X_ADVMAME4X,
	GB_MAG_FILTER_XBR2,
	// TODO(stefalie): I'm too tired, but eventually I should add some more filters:
	// - HQ2x, HQ3x, HQ4x
	// - xBR at higher resolution
	// - SuperXBR (some info links, both referenced in Wikipedia):
	//   -
	//   https://drive.google.com/file/d/0B_yrhrCRtu8GYkxreElSaktxS3M/view?pref=2&pli=1&resourcekey=0-mKvLmDc8GWdBuPWpPN_wQg
	//   - https://pastebin.com/cbH8ZQQT
	// - xBRZ (https://github.com/janisozaur/xbrz/blob/master/xbrz.cpp)
	// - McGuire, Gagiu; 2021; MMPX Style-Preserving Pixel-Art Magnification
	// GB_MAG_FILTER_HQ2X,
	// GB_MAG_FILTER_HQ3X,
	// GB_MAG_FILTER_HQ4X,
	// GB_MAG_FILTER_XBR3,
	// GB_MAG_FILTER_XBR4,
	// GB_MAG_FILTER_SUPERXBR,
	// GB_MAG_FILTER_XBRZ,
	// GB_MAG_FILTER_MMPX,
	GB_MAG_FILTER_MAX_VALUE,
} gb_MagFilter;

// Returns the framebuffer size when using a specific magnification filter.
uint32_t
gb_MagFramebufferSizeInBytes(gb_MagFilter mag_filter);

// Returns the max framebuffer size for all magnification filters.
uint32_t
gb_MaxMagFramebufferSizeInBytes();

// Magnify 'gb's default framebuffer with a given filter and stores in user
// provided buffer.
gb_Framebuffer
gb_MagFramebuffer(const gb_GameBoy* gb, gb_MagFilter mag_filter, uint8_t* pixels);

