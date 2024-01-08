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

typedef struct gb_PpuFlags
{
	uint8_t bg_enable : 1;
	uint8_t sprite_enable : 1;
	uint8_t sprite_size : 1;  // 0 -> 8x8, 1 -> 8x16
	uint8_t bg_tilemap_select : 1;  // 0 -> 0x9800-0x9BFF, 1 -> 0x9C00-0x9FFF
	uint8_t bg_tileset_select : 1;  // 0 -> 0x8800-0x97FF, 1 -> 0x8000-0x8FFF
	uint8_t win_enable : 1;
	uint8_t win_tilemap_select : 1;  // 0 -> 0x9800-0x9BFF, 1 -> 0x9C00-0x9FFF
	uint8_t lcd_enable : 1;
} gb_PpuFlags;

typedef struct gb_Palette
{
	uint8_t dot_data_00;
	uint8_t dot_data_01;
	uint8_t dot_data_10;
	uint8_t dot_data_11;
} gb_Palette;

gb_Palette
gb_DefaultPalette(void);

// Memory Bank Controller type
typedef enum gb_MbcType
{
	GB_MBC_TYPE_ROM_ONLY,
	GB_MBC_TYPE_1,
	GB_MBC_TYPE_2,
	GB_MBC_TYPE_3,
} gb_MbcType;

typedef union gb_InterruptBits
{
	struct
	{
		uint8_t vblank : 1;
		uint8_t lcd_stat : 1;
		uint8_t timer : 1;
		uint8_t serial : 1;
		uint8_t joypad : 1;
		uint8_t _ : 3;
	};
	uint8_t reg;
} gb_InterruptBits;

typedef struct gb_GameBoy
{
	// The CPU conains only the registers.
	// Note that the GameBoy uses little-endian.
	struct gb_Cpu
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
		struct gb_Interrupt
		{
			bool ime;  // Interrupt master enable flag
			bool ime_after_next_inst;
			gb_InterruptBits ie_flags;  // Interrupt enable flags
			gb_InterruptBits if_flags;  // Interrupt requested flags
		} interrupt;
		bool stop;
		bool halt;
	} cpu;

	struct gb_SerialTransfer
	{
		uint8_t sb;
		uint8_t sc;
	} serial;

	struct gb_Ppu
	{
		union
		{
			gb_PpuFlags flags;
			uint8_t lcdc;
		};
		uint8_t stat;
		uint8_t scy;  // Scroll Y
		uint8_t scx;  // Scroll X
		uint8_t ly;  // Line
		uint8_t lyc;  // Line compare
		// uint8_t dma;
		uint8_t bgp;  // Background & window palette
		uint8_t obp0;  // Object palette 0
		uint8_t obp1;  // Object palette 1
		uint8_t wy;  // Windows Y
		uint8_t wx;  // Windows X

		uint8_t oam[160];  // Sprite attribute memory
	} ppu;

	struct gb_Memory
	{
		bool bios_mapped;
		uint8_t wram[8192];  // 8 KiB
		// TODO: Consider caching the tiles.
		uint8_t vram[8192];  // 8 KiB
		uint8_t external_ram[8192 * 4];  // Up to 4 banks of 8 KiB each
		uint8_t zero_page_ram[128];

		// Memory Bank Controller
		gb_MbcType mbc_type;
		bool mbc_external_ram_enable;
		union
		{
			struct
			{
				uint8_t rom_bank : 5;
				uint8_t ram_bank : 2;  // Or high 2 bits of RAM bank.
				uint8_t bank_mode : 1;
			} mbc1;
			struct
			{
				uint8_t rom_bank : 4;
			} mbc2;
			struct
			{
				uint8_t rom_bank : 7;
				uint8_t ram_bank : 4;
				uint8_t rtc_mode_or_idx;
				// TODO: implement Real Time Clock (RTC).
				// Currenlty, loading an RTC ROM fails.
				uint8_t rtc_regs[5];
			} mbc3;
		};
	} memory;

	struct gb_Timer
	{
		uint64_t remaining_m_cycles;
		uint16_t t_clock;
		bool reset;  // Resets remaining_m_cycles upon timer activation.

		uint8_t div;
		uint8_t tima;
		uint8_t tma;
		union
		{
			struct
			{
				uint8_t clock_select : 2;
				uint8_t enable : 1;
				int8_t _ : 5;
			};
			uint8_t reg;
		} tac;
	} timer;

	struct gb_Display
	{
		// One gray-scale 1-byte value per pixels. This is unlike the original GameBoy
		// with only 2 bits per pixel. Using a full byte per pixel makes it easier to
		// directly map the framebuffer onto a texture.
		uint8_t pixels[GB_FRAMEBUFFER_WIDTH * GB_FRAMEBUFFER_HEIGHT];
	} display;

	struct gb_Rom
	{
		char name[16];
		const uint8_t *data;
		uint32_t num_bytes;
	} rom;
} gb_GameBoy;

// TODO rem, I think reset is enough
void
gb_Init(gb_GameBoy *gb);

// Returns true in error case if the ROM cannot be loaded, is broken,
// or is not for GameBoy.
// TODO(stefalie): Consider returning an error code of what went wrong.
bool
gb_LoadRom(gb_GameBoy *gb, const uint8_t *rom, uint32_t num_bytes);

// Read a byte/word from the GameBoy's memory space at 'addr'.
uint8_t
gb_MemoryReadByte(const gb_GameBoy *gb, uint16_t addr);
uint16_t
gb_MemoryReadWord(const gb_GameBoy *gb, uint16_t addr);

// Resets the GameBoy.
void
gb_Reset(gb_GameBoy *gb, bool skip_bios);

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
gb_FetchInstruction(const gb_GameBoy *gb, uint16_t addr);

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
gb_ExecuteNextInstruction(gb_GameBoy *gb);

// Advances the GameBoy by as many instruction as possible so that at most
// 'machine_cyles' many cycles elapse.
// Returns the exact number of cycles that elapsed (which is <= 'machine_cycles').
size_t
gb_Advance(gb_GameBoy *gb, size_t machine_cycles);

typedef struct gb_TileLine
{
	uint8_t pixels[8];
} gb_TileLine;

// Returns a line inside one tile, 2 bits per pixel.
// uint16_t
// gb_GetTileLine(gb_GameBoy *gb, size_t set_index, int tile_index, size_t line_index);
// Returns a line inside one tile, 8 bits per pixel.
gb_TileLine
gb_GetTileLine(gb_GameBoy *gb, size_t set_index, int tile_index, size_t line_index, gb_Palette palette);

// Returns a line inside one tile inside one of the two maps.
// 8 bits monochrome per pixel.
gb_TileLine
gb_GetMapTileLine(gb_GameBoy *gb, size_t map_index, size_t tile_x_index, size_t y_index, gb_Palette palette);

typedef struct gb_Framebuffer
{
	uint16_t width;
	uint16_t height;
	const uint8_t *pixels;
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
gb_MagFramebuffer(const gb_GameBoy *gb, gb_MagFilter mag_filter, uint8_t *pixels);
