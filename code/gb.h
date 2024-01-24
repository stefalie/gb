// Copyright (C) 2022 Stefan Lienhard

// TODO: insert cmd to compile this -c -std=c11 /c /std:c11

#include <stdbool.h>
#include <stdint.h>

#define GB_FRAMEBUFFER_WIDTH 160
#define GB_FRAMEBUFFER_HEIGHT 144

#define GB_MACHINE_FREQ 1048576
#define GB_MACHINE_CYCLES_PER_FRAME (70224 / 4)

typedef enum gb_PpuMode
{
	GB_PPU_MODE_HBLANK,
	GB_PPU_MODE_VBLANK,
	GB_PPU_MODE_OAM_SCAN,
	GB_PPU_MODE_VRAM_SCAN,
} gb_PpuMode;

// Note that this is currently rather wasteful as we only support the monochrome
// DMG. If we however decide to go for Color GameBoy support, this will make it
// easy. It also allows to map the monochrome values to whatever RGB values we
// want.
typedef union gb_Color
{
	struct
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t _;  // Unused, for alignment.
	};
	uint32_t as_u32;
} gb_Color;

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
					struct gb_CpuFlags
					{
						uint8_t _ : 4;
						uint8_t carry : 1;
						uint8_t half_carry : 1;
						uint8_t subtract : 1;
						uint8_t zero : 1;
					} flags;
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

	struct gb_Joypad
	{
		uint8_t buttons;
		uint8_t dpad;
		union
		{
			uint8_t selection_wire;
			struct
			{
				uint8_t invalid : 4;  // The lower nibble of buttons/dpad shoudl be here.
				uint8_t dpad_select : 1;
				uint8_t buttons_select : 1;
				uint8_t _ : 2;
			};
		};
	} joypad;

	struct gb_SerialTransfer
	{
		uint8_t sb;
		uint8_t sc;
	} serial;

	struct gb_Ppu
	{
		uint16_t mode_clock;

		union
		{
			struct gb_PpuLcdcFlags
			{
				uint8_t bg_and_win_enable : 1;
				uint8_t sprite_enable : 1;
				uint8_t sprite_size : 1;  // 0 -> 8x8, 1 -> 8x16
				uint8_t bg_tilemap_select : 1;  // 0 -> 0x9800-0x9BFF, 1 -> 0x9C00-0x9FFF
				uint8_t bg_and_win_tileset_select : 1;  // 0 -> 0x8800-0x97FF, 1 -> 0x8000-0x8FFF
				uint8_t win_enable : 1;
				uint8_t win_tilemap_select : 1;  // 0 -> 0x9800-0x9BFF, 1 -> 0x9C00-0x9FFF
				uint8_t lcd_enable : 1;
			} flags;
			uint8_t reg;
		} lcdc;
		union
		{
			struct gb_PpuStatFlags
			{
				uint8_t mode : 2;  // A value of 'gb_PpuMode' (but can't use it as type).
				uint8_t coincidence_flag : 1;
				uint8_t interrupt_mode_hblank : 1;
				uint8_t interrupt_mode_vblank : 1;
				uint8_t interrupt_mode_oam_scan : 1;
				uint8_t interrupt_coincidence : 1;
				uint8_t _ : 1;
			} flags;
			uint8_t reg;
		} stat;
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
		// TODO(stefalie): Consider caching the tiles.
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
				// TODO(stefalie): Real Time Clock (RTC) is currently not implemented/supported.
				// Loading a ROM with RTC will fail.
				uint8_t rtc_regs[5];
			} mbc3;
		};
	} memory;

	struct gb_Timer
	{
		uint16_t remaining_m_cycles;  // Same concept as 'mode_clock' in 'gb_Ppu'.
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
		bool updated;

		// The original DMG only has 2 bits per pixel, but This makes it easy to
		// map the framebuffer onto a texture (and there won't be anything to change
		// if we ever support the Color GameBoy).
		gb_Color pixels[GB_FRAMEBUFFER_WIDTH * GB_FRAMEBUFFER_HEIGHT];
	} display;

	struct gb_Rom
	{
		char name[16];
		const uint8_t *data;
		uint32_t num_bytes;
	} rom;

	struct gb_Sound
	{
		uint8_t nr[23];
		uint8_t w[16];
	} sound;
} gb_GameBoy;

// Returns true in error case if the ROM cannot be loaded, is broken,
// or is not for GameBoy.
// TODO(stefalie): Consider returning an error code of what went wrong.
bool
gb_LoadRom(gb_GameBoy *gb, const uint8_t *rom, uint32_t num_bytes, bool skip_bios);

// Read a byte from the GameBoy's memory space at 'addr'.
uint8_t
gb_MemoryReadByte(const gb_GameBoy *gb, uint16_t addr);

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

typedef enum gb_Input
{
	GB_INPUT_BUTTON_A,
	GB_INPUT_BUTTON_B,
	GB_INPUT_BUTTON_SELECT,
	GB_INPUT_BUTTON_START,
	GB_INPUT_ARROW_RIGHT,
	GB_INPUT_ARROW_LEFT,
	GB_INPUT_ARROW_UP,
	GB_INPUT_ARROW_DOWN,
} gb_Input;

void
gb_SetInput(gb_GameBoy *gb, gb_Input input, bool down);

typedef struct gb_Tile
{
	gb_Color pixels[8][8];
} gb_Tile;

gb_Tile
gb_GetTile(gb_GameBoy *gb, size_t set_index, int tile_index);

// Returns true if a new frame has been fully rendered.
// Use this to check if the emulator's texture should be updated.
// Calling this will reset the emulator's internal "framebuffer updated" flag.
bool
gb_FramebufferUpdated(gb_GameBoy *gb);

typedef struct gb_Framebuffer
{
	uint16_t width;
	uint16_t height;
	const gb_Color *pixels;
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
gb_MagFramebuffer(const gb_GameBoy *gb, gb_MagFilter mag_filter, gb_Color *pixels);
