// Copyright (C) 2022 Stefan Lienhard

// TODO: insert cmd to compile this -c -std=c11 /c /std:c11

#include <stdbool.h>
#include <stdint.h>

#define GB_FRAMEBUFFER_WIDTH 160
#define GB_FRAMEBUFFER_HEIGHT 144

#define GB_MACHINE_FREQ 1048576
#define GB_MACHINE_CYCLES_PER_FRAME (70224 / 4)

typedef struct gb_CpuFlags
{
	uint8_t _ : 4;
	uint8_t carry : 1;
	uint8_t half_carry : 1;
	uint8_t subtract : 1;
	uint8_t zero : 1;
} gb_CpuFlags;

typedef struct gb_GameBoy
{
	// The CPU conains only the registers.
	// Note that the GameBoy uses little-endian.
	struct Cpu
	{
		union
		{
			struct
			{
				union
				{
					gb_CpuFlags flags;
					uint8_t f;
				};
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
		uint16_t pc;  // Program counter
		uint16_t sp;  // Stack pointer

		// TODO: init
		uint8_t interrupts;
		bool stop;
		bool halt;
	} cpu;

	struct GPU
	{
		//uint8_t flags;
		//uint8_t line;
		//uint8_t lineComp;
		uint8_t scx;
		uint8_t scy;
		uint8_t winx;
		uint8_t winy;

		uint8_t oam[160];  // Sprite attribute memory
	} gpu;

	struct Memory
	{
		bool bios_mapped;
		// TODO
		uint8_t wram[8192];  // 8 KiB
		uint8_t vram[8192];  // 8 KiB
		uint8_t external_ram[8192];  // 8 KiB
		uint8_t zero_page_ram[128];
	} memory;

	struct Timer
	{
		uint8_t div;
		uint8_t tima;
		uint8_t tma;
		uint8_t tac;
	} timer;

	struct Framebuffer
	{
		// One gray-scale 1-byte value per pixels. This is unlike the original GameBoy
		// with only 2 bits per pixel. Using a full byte per pixel makes it easier to
		// directly map the framebuffer onto a texture.
		uint8_t pixels[GB_FRAMEBUFFER_WIDTH * GB_FRAMEBUFFER_HEIGHT];
	} framebuffer;

	struct Rom
	{
		char name[16];
		const uint8_t* data;
		uint32_t num_bytes;
	} rom;
} gb_GameBoy;

// TODO rem, I think reset is enough
void
gb_Init(gb_GameBoy* gb);

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
gb_Reset(gb_GameBoy* gb, bool skip_bios);

// A GameBoy assembly instruction.
typedef struct gb_Instruction
{
	uint8_t opcode;
	bool is_extended;
	uint8_t num_operand_bytes;
	union
	{
		uint8_t operand_byte;
		uint16_t operand_word;
	};
} gb_Instruction;

// Fetches assembly instruction at 'addr' (which needs to point to a valid instruction).
gb_Instruction
gb_FetchInstruction(const gb_GameBoy* gb, uint16_t addr);

// Returns the total number of bytes that the instruction uses for opcode, extended
// opcode (if applicable), and operand.
uint16_t
gb_InstructionSize(gb_Instruction inst);

// Disassembles 'inst' into 'str_buf'. Appends a NUL suffix.
// A 32 byte buffer is sufficient for any instruction.
// Returns the string length of the written disassembly (not counting the NUL suffix).
size_t
gb_DisassembleInstruction(gb_Instruction inst, char str_buf[], size_t str_buf_len);

// Executes the next instruction and returns the number of GameBoy machines cycles
// it takes to execute that instruction.
size_t
gb_ExecuteNextInstruction(gb_GameBoy* gb);

// Returns a line inside one tile, 2 bits per pixel.
uint16_t
gb_GetTileLine(gb_GameBoy* gb, size_t set_index, int tile_index, size_t line_index);

// Returns a line inside one tile inside one of the two maps.
// 8 bits monochrome per pixel.
uint64_t
gb_GetMapTileLine(gb_GameBoy* gb, size_t map_index, size_t tile_x_index, size_t y_index);

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
gb_MaxMagFramebufferSizeInBytes(void);

// Magnify 'gb's default framebuffer with a given filter and stores in user
// provided buffer.
gb_Framebuffer
gb_MagFramebuffer(const gb_GameBoy* gb, gb_MagFilter mag_filter, uint8_t* pixels);

