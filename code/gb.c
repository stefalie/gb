// Copyright (C) 2022 Stefan Lienhard

#include "gb.h"
#include <assert.h>
#include <math.h>  // TODO(stefalie): Only used for fabs, consider removing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(v, min, max) MIN(MAX(v, min), max)

#define BLARGG_TEST_ENABLE 0

// Palette from bgb
static const gb_Color gb__DefaultPalette[4] = {
	{ .r = 0xE8, .g = 0xFC, .b = 0xCC },
	{ .r = 0xAC, .g = 0xD4, .b = 0x90 },
	{ .r = 0x54, .g = 0x8C, .b = 0x70 },
	{ .r = 0x14, .g = 0x2C, .b = 0x38 },
};
// TODO(stefalie): This is currently unused.
// Color scheme from https://gbdev.io/pandocs/Tile_Data.html
static const gb_Color gb__GbDevIoPalette[4] = {
	{ .r = 0xE0, .g = 0xF8, .b = 0xD0 },
	{ .r = 0x88, .g = 0xC0, .b = 0x70 },
	{ .r = 0x34, .g = 0x68, .b = 0x56 },
	{ .r = 0x08, .g = 0x18, .b = 0x20 },
};

#define ROM_HEADER_START_ADDRESS 0x0100
typedef struct gb__RomHeader
{
	uint8_t begin_code_execution_point[4];
	uint8_t nintendo_logo[48];
	char rom_name[15];
	uint8_t gbc;  // 0x80 is GameBoy Color
	uint8_t licensee_hi;
	uint8_t licensee_lo;
	uint8_t sgb;  // 0x00 is GameBoy, 0x03 is Super GameBoy
	uint8_t cartridge_type;
	uint8_t rom_size;
	uint8_t ram_size;
	uint8_t dst_country_cod;  // 0x00 is Japan, 0x01 is non-Japanese
	uint8_t licensee_old;
	uint8_t mask_rom_version_number;
	uint8_t complement_check;
	uint8_t checksum_hi;
	uint8_t checksum_lo;
} gb__RomHeader;

static const uint8_t gb__bios[256] = { 0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21,
	0x26, 0xFF, 0x0E, 0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
	0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B, 0xFE, 0x34, 0x20,
	0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9, 0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21,
	0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20, 0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57,
	0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04, 0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20,
	0xF7, 0x1D, 0x20, 0xF2, 0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20,
	0x06, 0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xE2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20, 0x4F, 0x16,
	0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17, 0x05, 0x20, 0xF5, 0x22, 0x23,
	0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67,
	0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5,
	0x42, 0x3C, 0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20, 0xF5,
	0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50 };

static inline uint8_t
gb__Hi(uint16_t val)
{
	return (val >> 8u) & 0xFF;
}

static inline uint8_t
gb__Lo(uint16_t val)
{
	return (uint8_t)val;
}

static inline const gb__RomHeader *
gb__GetHeader(const gb_GameBoy *gb)
{
	return (gb__RomHeader *)&(gb->rom.data[ROM_HEADER_START_ADDRESS]);
}

// TODO(stefalie): Does stat blocking happen even if the LCD was previously disabled?
static inline bool
gb__LcdStatInt48Line(gb_GameBoy *gb)
{
	union gb_PpuStat *stat = &gb->ppu.stat;

	bool line_high = gb->ppu.lcdc.lcd_enable &&
			((stat->interrupt_coincidence && stat->coincidence_flag) ||
					(stat->interrupt_mode_hblank && stat->mode == GB_PPU_MODE_HBLANK) ||
					(stat->interrupt_mode_vblank && stat->mode == GB_PPU_MODE_VBLANK) ||
					(stat->interrupt_mode_oam_scan && stat->mode == GB_PPU_MODE_OAM_SCAN));
	return line_high;
}

static inline void
gb__CompareLyToLyc(gb_GameBoy *gb, bool prev_int48_signal)
{
	struct gb_Ppu *ppu = &gb->ppu;

	ppu->stat.coincidence_flag = ppu->ly == ppu->lyc;

	if (ppu->lcdc.lcd_enable)
	{
		if (ppu->stat.coincidence_flag && ppu->stat.interrupt_coincidence)
		{
			if (!prev_int48_signal)
			{
				gb->cpu.interrupt.if_flags.lcd_stat = 1;
			}
		}
	}
}

bool
gb_LoadRom(gb_GameBoy *gb, const uint8_t *rom, uint32_t num_bytes, bool skip_bios)
{
	gb->rom.data = rom;
	gb->rom.num_bytes = num_bytes;

	const gb__RomHeader *header = gb__GetHeader(gb);

	const uint8_t nintendo_logo[] = { 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00,
		0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9,
		0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E };

	if (memcmp(nintendo_logo, header->nintendo_logo, sizeof(header->nintendo_logo)))
	{
		return true;
	}

	// 0x80 is Color GameBoy but backwards compatible
	// 0xC0 is Color GameBoy only
	if (header->gbc == 0xC0)
	{
		return true;
	}

	gb->rom.name[15] = '\0';
	memcpy(gb->rom.name, header->rom_name, sizeof(header->rom_name));

	switch (header->cartridge_type)
	{
	case 0x00:
	case 0x08:
	case 0x09:
		gb->memory.mbc_type = GB_MBC_TYPE_ROM_ONLY;
		break;
	case 0x01:
	case 0x02:
	case 0x03:
		gb->memory.mbc_type = GB_MBC_TYPE_1;
		break;
	case 0x05:
	case 0x06:
		gb->memory.mbc_type = GB_MBC_TYPE_2;
		break;
	case 0x0F:
	case 0x10:
		// MBC3 but RTC not supported.
		return true;
	case 0x11:
	case 0x12:
	case 0x13:
		gb->memory.mbc_type = GB_MBC_TYPE_3;
		break;
	default:
		// TODO(stefalie): Filter out other types of cartridge flags such as RAM and BATTERY?
		// That would allow us to assert that the RAM flag is set when RAM is accessed.
		// Or what's easier is to simply that nobody ever uses RAM when the corresponding
		// flag is not set.
		// NOTE: External RAM only needs to be written to a save state if the BATTERY flag
		// is set.
		// TODO(stefalie): Add support for MBC5? I think that implies that we should also support.
		// I'm not sure there are any games with MBC5 for DMG.
		return true;
	};

	// Checksum test
#if !BLARGG_TEST_ENABLE
	uint16_t checksum = 0;
	for (size_t i = 0; i < num_bytes; ++i)
	{
		checksum += gb->rom.data[i];
	}
	checksum -= header->checksum_lo + header->checksum_hi;
	if (gb__Lo(checksum) != header->checksum_lo || gb__Hi(checksum) != header->checksum_hi)
	{
		return true;
	}

	// Another checksum over part of the header (see page 17 of the GameBoy CPU Manual)
	checksum = 0;
	for (size_t i = 0x134; i <= 0x14D; ++i)
	{
		checksum += gb->rom.data[i];
	}
	checksum += 25;
	if (gb__Lo(checksum) != 0)
	{
		return true;
	}
#endif

	gb_Reset(gb, skip_bios);
	return false;
}

uint8_t
gb_MemoryReadByte(const gb_GameBoy *gb, uint16_t addr)
{
	const struct gb_Memory *mem = &gb->memory;

	// The data bus value for undefined addresses seems to be 0xFF.
	// - For MBC1 with disabled external RAM, it's stated here:
	//   https://gbdev.io/pandocs/MBC1.html
	// - More clues here:
	//   https://www.reddit.com/r/EmuDev/comments/5nixai/gb_tetris_writing_to_unused_memory/
	const uint8_t undefined_value = 0xFF;

	switch (addr & 0xF000)
	{
	// ROM bank 0 or BIOS
	case 0x0000:
	case 0x1000:
	case 0x2000:
	case 0x3000:
		if (mem->bios_mapped && addr < 0x100)
		{
			return gb__bios[addr];
		}
		else
		{
			size_t offset = 0;
			if (mem->mbc_type == GB_MBC_TYPE_1 && mem->mbc1.bank_mode == 1)
			{
				offset = mem->mbc1.ram_bank << 19u;
			}
			return gb->rom.data[addr + offset];
		}
	// Switchable ROM bank
	case 0x4000:
	case 0x5000:
	case 0x6000:
	case 0x7000: {
		const uint8_t rom_size = MIN(gb__GetHeader(gb)->rom_size, 6);
		const uint8_t num_rom_banks = 2u << rom_size;
		assert(num_rom_banks < 128);

		uint8_t bank = 0;
		if (mem->mbc_type == GB_MBC_TYPE_ROM_ONLY)
		{
			bank = 1;
		}
		else if (mem->mbc_type == GB_MBC_TYPE_1)
		{
			// 0 -> 1 transition
			uint8_t lower_bits = mem->mbc1.rom_bank;
			if (lower_bits == 0)
			{
				lower_bits = 1;
			}
			bank = lower_bits + (mem->mbc1.ram_bank << 5u);

			// Mask away unused bits.
			const uint8_t mask = num_rom_banks - 1;
			bank &= mask;
		}
		else if (mem->mbc_type == GB_MBC_TYPE_2)
		{
			bank = MAX(mem->mbc2.rom_bank, 1);
			assert(bank < 16);
			assert(rom_size <= 3);
		}
		else if (mem->mbc_type == GB_MBC_TYPE_3)
		{
			bank = MAX(mem->mbc3.rom_bank, 1);
		}
		assert(bank < num_rom_banks);

		const uint16_t bank_size = 0x4000;
		return gb->rom.data[bank * bank_size + (addr - bank_size)];
	}
	// VRAM
	// TODO(stefalie): VRAM/OAM is inaccessible during certain PPU modes.
	// See: https://gbdev.io/pandocs/Rendering.html
	case 0x8000:
	case 0x9000:
		// TODO(stefalie): Can't assert if the debugger is open and shows the memory view.
		// assert(gb->ppu.stat.mode != GB_PPU_MODE_VRAM_SCAN || gb->ppu.lcdc.lcd_enable == 0);
		return mem->vram[addr & 0x1FFF];
	// Switchable RAM bank
	case 0xA000:
	case 0xB000:
		// TODO(stefalie): What happens if one reads from a non-existing area in external
		// RAM in MBC1/3? Undefined behavior I assume? Then the asserts can be removed
		// and the code is OK. Nicer is probably to do nothing in that case (meaning you
		// have to replace the asserts if/else).
		if (mem->mbc_type == GB_MBC_TYPE_ROM_ONLY)
		{
			return mem->external_ram[addr & 0x1FFF];
		}
		else if (mem->mbc_type == GB_MBC_TYPE_1)
		{
			if (mem->mbc_external_ram_enable)
			{
				const uint8_t ram_size = gb__GetHeader(gb)->ram_size;
				(void)ram_size;
				assert(mem->mbc1.ram_bank == 0 && ram_size == 2 || ram_size == 3);
				return mem->external_ram[addr & 0x1FFF + (mem->mbc1.ram_bank << 13u)];
			}
		}
		else if (mem->mbc_type == GB_MBC_TYPE_2)
		{
			if (mem->mbc_external_ram_enable)
			{
				// Higher nibble undefined.
				// Therefore the masking by 0x0F is not really needed (espeically also because
				// it is also masked when writing). Better be safe though.
				const uint8_t ram_size = gb__GetHeader(gb)->ram_size;
				(void)ram_size;
				assert(ram_size == 0 && (addr & 0x1FFF) < 0x0200);
				return mem->external_ram[addr & 0x01FF] & 0x0F;
			}
		}
		else if (mem->mbc_type == GB_MBC_TYPE_3)
		{
			if (mem->mbc_external_ram_enable)
			{
				if (mem->mbc3.rtc_mode_or_idx == 0)
				{
					const uint8_t ram_size = gb__GetHeader(gb)->ram_size;
					(void)ram_size;
					assert(mem->mbc1.ram_bank == 0 && ram_size == 2 || ram_size == 3);
					return mem->external_ram[addr & 0x1FFF + (mem->mbc3.ram_bank << 13u)];
				}
				else
				{
					return mem->mbc3.rtc_regs[mem->mbc3.rtc_mode_or_idx - 0x08];
				}
			}
		}
		return undefined_value;
	// (Internal) working RAM
	case 0xC000:
	case 0xD000:
	case 0xE000:  // Echo of (Internal) working RAM, [0xE000, 0xFE00)
		return mem->wram[addr & 0x1FFF];
	// Echo of (Internal) working RAM, I/O, zero page
	case 0xF000:
		switch (addr & 0x0F00)
		{
		default:  // Echo of (Internal) working RAM, [0xE000, 0xFE00)
			assert((addr - 0xF000) < 0x0E00);
			return mem->wram[addr & 0x1FFF];
		case 0x0E00:
			// TODO(stefalie): VRAM/OAM is inaccessible during certain PPU modes.
			// See: https://gbdev.io/pandocs/Rendering.html
			if (addr < 0xFEA0)  // Sprite Attrib Memory (OAM)
			{
				// TODO(stefalie): Can't assert if the debugger is open and shows the memory view.
				// assert(gb->ppu.stat.mode == GB_PPU_MODE_HBLANK || gb->ppu.stat.mode == GB_PPU_MODE_VBLANK ||
				//		gb->ppu.lcdc.lcd_enable == 0);
				return gb->memory.oam.bytes[addr & 0xFF];
			}
			else  // Empty
			{
				return undefined_value;
			}
		case 0x0F00:
			// Joypad
			if (addr == 0xFF00)
			{
				// The documentaton is unclear about what happens when both selection
				// wires are active, i.e., == 0. I assume it's just ANDed together.
				uint8_t p1 = 0x0F;
				if (gb->joypad.buttons_select == 0)
				{
					p1 &= gb->joypad.buttons;
				}
				if (gb->joypad.dpad_select == 0)
				{
					p1 &= gb->joypad.dpad;
				}
				assert((gb->joypad.selection_wire & 0x0F) == 0);
				assert(gb->joypad._ == 0x3);
				p1 |= gb->joypad.selection_wire;
				assert((p1 & ~0x3F) == 0xC0);
				return p1;
			}
			// Timer
			else if (addr == 0xFF04)
			{
				return gb->timer.div;
			}
			else if (addr == 0xFF05)
			{
				return gb->timer.tima;
			}
			else if (addr == 0xFF06)
			{
				return gb->timer.tma;
			}
			else if (addr == 0xFF07)
			{
				return gb->timer.tac.reg;
			}
			// Interrupt request flags
			else if (addr == 0xFF0F)
			{
				return gb->cpu.interrupt.if_flags.reg;
			}
			// Sound channels
			else if ((addr >= 0xFF10 && addr <= 0xFF14) || (addr >= 0xFF16 && addr <= 0xFF1E) ||
					(addr >= 0xFF20 && addr <= 0xFF26))
			{
				// TODO SND
				return gb->sound.nr[addr - 0xFF10];
			}
			// Wave pattern
			else if (addr >= 0xFF30 && addr <= 0xFF3F)
			{
				// TODO SND
				return gb->sound.w[addr - 0xFF30];
			}
			// Display
			else if (addr == 0xFF40)
			{
				return gb->ppu.lcdc.reg;
			}
			else if (addr == 0xFF41)
			{
				return gb->ppu.stat.reg;
			}
			else if (addr == 0xFF42)
			{
				return gb->ppu.scy;
			}
			else if (addr == 0xFF43)
			{
				return gb->ppu.scx;
			}
			else if (addr == 0xFF44)
			{
				return gb->ppu.ly;
			}
			else if (addr == 0xFF45)
			{
				return gb->ppu.lyc;
			}
			else if (addr == 0xFF46)
			{
				// DMA is write-only
				return undefined_value;
			}
			else if (addr == 0xFF47)
			{
				return gb->ppu.bgp;
			}
			else if (addr == 0xFF48)
			{
				return gb->ppu.obp0;
			}
			else if (addr == 0xFF49)
			{
				return gb->ppu.obp1;
			}
			else if (addr == 0xFF4A)
			{
				return gb->ppu.wy;
			}
			else if (addr == 0xFF4B)
			{
				return gb->ppu.wx;
			}
			// Zero page RAM
			else if (addr >= 0xFF80 && addr < 0xFFFF)
			{
				assert((addr - 0xFF80) < 0x80);
				return mem->zero_page_ram[addr & 0x7F];
			}
			// Interrupt enable
			else if (addr == 0xFFFF)
			{
				return gb->cpu.interrupt.ie_flags.reg;
			}
			else
			{
				return undefined_value;
			}
			break;
		}
	default:
		assert(false);
		return 0;
	}
}

uint16_t
gb_MemoryReadWord(const gb_GameBoy *gb, uint16_t addr)
{
	return gb_MemoryReadByte(gb, addr) + (gb_MemoryReadByte(gb, addr + 1) << 8u);
}

static void
gb__MemoryWriteByte(gb_GameBoy *gb, uint16_t addr, uint8_t value)
{
	struct gb_Memory *mem = &gb->memory;

	switch (addr & 0xF000)
	{
	// ROM bank selection
	case 0x0000:
	case 0x1000:
	case 0x2000:
	case 0x3000:
		assert(!mem->bios_mapped);
		// NOTE: Tetris writes to 0x2000 even though it's of MBC1 type, can't assert.
		// See: https://www.reddit.com/r/EmuDev/comments/zddum6/gameboy_tetris_issues_with_getting_main_menu_to/
		if (mem->mbc_type == GB_MBC_TYPE_1)
		{
			if (addr < 0x2000)
			{
				mem->mbc_external_ram_enable = (value & 0x0F) == 0xA;
			}
			else
			{
				mem->mbc1.rom_bank = value;
			}
		}
		else if (mem->mbc_type == GB_MBC_TYPE_2)
		{
			if (addr & 0x0100)
			{
				mem->mbc2.rom_bank = value;
			}
			else
			{
				mem->mbc_external_ram_enable = (value & 0x0F) == 0xA;
			}
		}
		if (mem->mbc_type == GB_MBC_TYPE_3)
		{
			if (addr < 0x2000)
			{
				mem->mbc_external_ram_enable = (value & 0x0F) == 0xA;
			}
			else
			{
				mem->mbc3.rom_bank = value;
			}
		}
		break;
	// RAM bank selection
	case 0x4000:
	case 0x5000:
		if (mem->mbc_type == GB_MBC_TYPE_1)
		{
			mem->mbc1.ram_bank = value;
		}
		else if (mem->mbc_type == GB_MBC_TYPE_3)
		{
			if (value <= 0x03)
			{
				mem->mbc3.ram_bank = value;
				mem->mbc3.rtc_mode_or_idx = 0;
			}
			else if (value >= 0x08 && value <= 0x0C)
			{
				mem->mbc3.rtc_mode_or_idx = value;
			}
			else
			{
				assert(false);
			}
		}
		break;
	// ROM banking mode selection
	case 0x6000:
	case 0x7000:
		if (mem->mbc_type == GB_MBC_TYPE_1)
		{
			mem->mbc1.bank_mode = value;
		}
		else if (mem->mbc_type == GB_MBC_TYPE_3)
		{
			// Latch currenty time into RTC.
			// TODO(stefalie): RTC not suppored,
			assert(false);
		}
		break;
	// VRAM
	// TODO(stefalie): VRAM/OAM is inaccessible during certain PPU modes.
	// See: https://gbdev.io/pandocs/Rendering.html
	case 0x8000:
	case 0x9000:
		// TODO(stefalie): Can't assert. Wario Land does such an access when first
		// entering the world map. I'm not sure if my timing is wrong or a game bug.
		// Not sure if it's better to ignore the write or let it write the value
		// anyway (current status).
		//
		// assert(gb->ppu.stat.mode != GB_PPU_MODE_VRAM_SCAN || gb->ppu.lcdc.lcd_enable == 0);
		mem->vram[addr & 0x1FFF] = value;
		break;
	// Switchable RAM bank
	case 0xA000:
	case 0xB000:
		// TODO(stefalie): See comment for the same memory range in gb_MemoryReadByte.
		if (mem->mbc_type == GB_MBC_TYPE_ROM_ONLY)
		{
			mem->external_ram[addr & 0x1FFF] = value;
		}
		else if (mem->mbc_type == GB_MBC_TYPE_1)
		{
			if (mem->mbc_external_ram_enable)
			{
#if BLARGG_TEST_ENABLE == 0
				const uint8_t ram_size = gb__GetHeader(gb)->ram_size;
				(void)ram_size;
				assert(mem->mbc1.ram_bank == 0 && ram_size == 2 || ram_size == 3);
#endif
				mem->external_ram[addr & 0x1FFF + (mem->mbc1.ram_bank << 13u)] = value;
			}
		}
		else if (mem->mbc_type == GB_MBC_TYPE_2)
		{
			if (mem->mbc_external_ram_enable)
			{
				const uint8_t ram_size = gb__GetHeader(gb)->ram_size;
				(void)ram_size;
				assert(ram_size == 0 && (addr & 0x1FFF) < 0x0200);
				mem->external_ram[addr & 0x01FF] = value & 0x0F;
			}
		}
		else if (mem->mbc_type == GB_MBC_TYPE_3)
		{
			if (mem->mbc_external_ram_enable)
			{
				if (mem->mbc3.rtc_mode_or_idx == 0)
				{
					const uint8_t ram_size = gb__GetHeader(gb)->ram_size;
					(void)ram_size;
					assert(mem->mbc1.ram_bank == 0 && ram_size == 2 || ram_size == 3);
					mem->external_ram[addr & 0x1FFF + (mem->mbc3.ram_bank << 13u)] = value;
				}
				else
				{
					mem->mbc3.rtc_regs[mem->mbc3.rtc_mode_or_idx - 0x08] = value;
				}
			}
		}
		break;
	// (Internal) working RAM
	case 0xC000:
	case 0xD000:
	case 0xE000:  // Echo of (Internal) working RAM, [0xE000, 0xFE00)
		mem->wram[addr & 0x1FFF] = value;
		break;
	// Echo of (Internal) working RAM, I/O, zero page
	case 0xF000:
		switch (addr & 0x0F00)
		{
		default:  // Echo of (Internal) working RAM, [0xE000, 0xFE00)
			assert((addr - 0xF000) < 0x0E00);
			mem->wram[addr & 0x1FFF] = value;
			break;
		case 0x0E00:
			// TODO(stefalie): VRAM/OAM is inaccessible during certain PPU modes.
			// See: https://gbdev.io/pandocs/Rendering.html
			if (addr < 0xFEA0)  // Sprite Attrib Memory (OAM)
			{
				assert(gb->ppu.stat.mode == GB_PPU_MODE_HBLANK || gb->ppu.stat.mode == GB_PPU_MODE_VBLANK ||
						gb->ppu.lcdc.lcd_enable == 0);
				gb->memory.oam.bytes[addr & 0xFF] = value;
			}
			else
			{
				// The empty area is ignored

				// NOTE(stefalie): Tetris writes to 0xFEFF, therefore we can't assert.
				// See: https://www.reddit.com/r/EmuDev/comments/5nixai/gb_tetris_writing_to_unused_memory/
			}
			break;
		case 0x0F00:
			// Joypad
			if (addr == 0xFF00)
			{
				gb->joypad.selection_wire = value & 0x30;
				gb->joypad._ = 0x3;
			}
			// Serial port
			else if (addr == 0xFF01)
			{
				// TODO(stefalie): Serial transfers are note implemented.
				// Currently this is only used by Blargg tests.
				gb->serial.sb = value;
			}
			else if (addr == 0xFF02)
			{
				// TODO(stefalie): Serial transfers are not implemented.
				// Currently this is only used by Blargg tests.
				gb->serial.sc = value & 0x81;
				if (gb->serial.sc == 0x81)
				{
					gb->serial.enable_interrupt_timer = true;
				}
			}
			// Timer
			else if (addr == 0xFF04)
			{
				// Writing any value resets this.
				gb->timer.div = 0x00;
			}
			else if (addr == 0xFF05)
			{
				gb->timer.tima = value;
			}
			else if (addr == 0xFF06)
			{
				gb->timer.tma = value;
			}
			else if (addr == 0xFF07)
			{
				const bool timer_prev_active = gb->timer.tac.enable == 1;
				// Masking is probably not needed.
				gb->timer.tac.reg = value & 0x07;
				if (!timer_prev_active && (gb->timer.tac.enable == 1))
				{
					gb->timer.reset = true;
				}
			}
			// Interrupt request flags
			else if (addr == 0xFF0F)
			{
				gb->cpu.interrupt.if_flags.reg = value & 0x1F;
			}
			// Sound channels
			else if ((addr >= 0xFF10 && addr <= 0xFF14) || (addr >= 0xFF16 && addr <= 0xFF1E) ||
					(addr >= 0xFF20 && addr <= 0xFF26))
			{
				// TODO SND
				gb->sound.nr[addr - 0xFF10] = value;
			}
			// Wave pattern
			else if (addr >= 0xFF30 && addr <= 0xFF3F)
			{
				// TODO SND
				gb->sound.w[addr - 0xFF30] = value;
			}
			// Display
			else if (addr == 0xFF40)
			{
				struct gb_Ppu *ppu = &gb->ppu;

				const uint8_t prev_lcd_enable = ppu->lcdc.lcd_enable;
				ppu->lcdc.reg = value;
				if (prev_lcd_enable == 1 && ppu->lcdc.lcd_enable == 0)
				{
					// Disable LCD
					// See: https://www.reddit.com/r/Gameboy/comments/a1c8h0/comment/eap4f8c/
					gb->ppu.ly = 0;
					gb->ppu.ly_win_internal = 0;
					gb->ppu.stat.coincidence_flag = gb->ppu.ly == gb->ppu.lyc;
					ppu->stat.mode = GB_PPU_MODE_HBLANK;  // NOTE: VBA does this, conflicts with the link above.
					ppu->mode_clock = 0;

					// Clear framebuffer to color 0.
					// TODO(stefalie): It should be an even "whiter" color.
					// See: https://gbdev.io/pandocs/LCDC.html#lcdc7--lcd-enable.
					for (size_t y = 0; y < GB_FRAMEBUFFER_HEIGHT; ++y)
					{
						for (size_t x = 0; x < GB_FRAMEBUFFER_WIDTH; ++x)
						{
							gb->display.pixels[GB_FRAMEBUFFER_WIDTH * y + x] = gb__DefaultPalette[0];
						}
					}
				}
				else if (prev_lcd_enable == 0 && ppu->lcdc.lcd_enable == 1)
				{
					// TODO(stefalie): I still don't know what state the LCD starts in.
					ppu->stat.mode = GB_PPU_MODE_OAM_SCAN;
					assert(gb->ppu.ly == 0);
					assert(gb->ppu.stat.coincidence_flag == (gb->ppu.ly == gb->ppu.lyc));
					if (gb__LcdStatInt48Line(gb))
					{
						gb->cpu.interrupt.if_flags.lcd_stat = 1;
					}
				}
			}
			else if (addr == 0xFF41)
			{
				union gb_PpuStat *stat = &gb->ppu.stat;
				bool prev_int48_signal = gb__LcdStatInt48Line(gb);

				gb_PpuMode mode_read_only = stat->mode;
				uint8_t coincidence_flag_read_only = stat->coincidence_flag;

				stat->reg = value;
				stat->mode = mode_read_only;
				stat->coincidence_flag = coincidence_flag_read_only;

				// Spurious STAT interrupt bug. STAT becomes 0xFF for 1 cycle.
				// See:
				// https://www.devrs.com/gb/files/faqs.html#GBBugs
				// https://gbdev.io/pandocs/STAT.html
				// The various sources disagree on when exactly it thappens.
				if ((stat->mode == GB_PPU_MODE_HBLANK || stat->mode == GB_PPU_MODE_VBLANK) || gb->ppu.ly == gb->ppu.lyc)
				{
					if (!prev_int48_signal)
					{
						gb->cpu.interrupt.if_flags.lcd_stat = 1;
					}
				}

				// TODO(stefalie): According to the following link, stat blocking only applies
				// between the different LCD modes but not between LCD modes and the coincidence flag.
				// https://forums.nesdev.org/viewtopic.php?t=19016#top

				// NOTE: This is different from what VBA does.
				// If both gb__LcdStatInt48Line calls are high, we have stat blocking.
				// In VBA it's possible for the int48 signal to go low before it goes up again
				// immediately (because it first masks off the newly writting 0 stat flags, and
				// only afterwards rises it again if applicable).

				if (!prev_int48_signal && gb__LcdStatInt48Line(gb))
				{
					gb->cpu.interrupt.if_flags.lcd_stat = 1;
				}
			}
			else if (addr == 0xFF42)
			{
				gb->ppu.scy = value;
			}
			else if (addr == 0xFF43)
			{
				gb->ppu.scx = value;
			}
			else if (addr == 0xFF44)
			{
				// According to gbdev.io LY is read-only (https://gbdev.io/pandocs/STAT.html)
				// and also according to The Cycle-Accurate Game Boy Docs.
				//
				// According to the GameBoy CPU Manual, writing LY resets it to 0.
				// (Shouldn't that also reset the mode of the PPU then)?
				// That's untrue, see:
				// See: https://www.reddit.com/r/Gameboy/comments/a1c8h0/comment/eap4f8c/
			}
			else if (addr == 0xFF45)
			{
				if (gb->ppu.lyc != value)
				{
					bool prev_int48_signal = gb__LcdStatInt48Line(gb);
					gb->ppu.lyc = value;
					gb__CompareLyToLyc(gb, prev_int48_signal);
				}
			}
			else if (addr == 0xFF46)
			{
				// DMA
				// Src: $XX00-$XX9F   ;XX = $00 to $DF
				// Dst: $FE00-$FE9F
				//
				// TODO(stefalie):
				// Technically during DMA only HRAM is accessible.
				// See: https://gbdev.io/pandocs/OAM_DMA_Transfer.html#oam-dma-transfer
				// We simply ignore that and don't verify that.
				// We also ignore that DMA is not instantaneous.
				for (uint16_t i = 0; i < 0xA0; ++i)
				{
					gb->memory.oam.bytes[i] = gb_MemoryReadByte(gb, (value << 8u) + i);
				}
				break;
			}
			else if (addr == 0xFF47)
			{
				gb->ppu.bgp = value;
			}
			else if (addr == 0xFF48)
			{
				gb->ppu.obp0 = value;
			}
			else if (addr == 0xFF49)
			{
				gb->ppu.obp1 = value;
			}
			else if (addr == 0xFF4A)
			{
				gb->ppu.wy = value;
			}
			else if (addr == 0xFF4B)
			{
				gb->ppu.wx = value;
			}
			else if (addr == 0xFF50)
			{
				gb->memory.bios_mapped = false;
			}
			// Zero page RAM
			else if (addr >= 0xFF80 && addr < 0xFFFF)
			{
				assert((addr - 0xFF80) < 0x80);
				mem->zero_page_ram[addr & 0x7F] = value;
			}
			// Interrupt enable
			else if (addr == 0xFFFF)
			{
				gb->cpu.interrupt.ie_flags.reg = value & 0x1F;
			}
			else
			{
				// NOTE: Once more, Tetris tries to write into non-existing memory.
			}
			break;
		}
		break;
	default:
		assert(false);
		break;
	}
}

static inline void
gb__MemoryWriteWord(gb_GameBoy *gb, uint16_t addr, uint16_t value)
{
	gb__MemoryWriteByte(gb, addr, gb__Lo(value));
	gb__MemoryWriteByte(gb, addr + 1, gb__Hi(value));
}

static inline uint16_t
gb__MemoryReadWord(const gb_GameBoy *gb, uint16_t addr)
{
	return gb_MemoryReadByte(gb, addr) + (gb_MemoryReadByte(gb, addr + 1) << 8u);
}

static inline void
gb__PushWordToStack(gb_GameBoy *gb, uint16_t value)
{
	gb->cpu.sp -= 2;
	gb__MemoryWriteWord(gb, gb->cpu.sp, value);
}

static inline uint16_t
gb__PopWordToStack(gb_GameBoy *gb)
{
	uint16_t result = gb__MemoryReadWord(gb, gb->cpu.sp);
	gb->cpu.sp += 2;
	return result;
}

void
gb_Reset(gb_GameBoy *gb, bool skip_bios)
{
	struct gb_Memory *mem = &gb->memory;

	// Reset everything to zero except the ROM info and the MBC type.
	const struct gb_Rom prev_rom = gb->rom;
	const gb_MbcType prev_mbc_type = mem->mbc_type;
	*gb = (gb_GameBoy){ 0 };
	gb->rom = prev_rom;
	mem->mbc_type = prev_mbc_type;

	gb->display.updated = true;

	if (skip_bios)
	{
		gb->cpu.pc = ROM_HEADER_START_ADDRESS;
	}
	else
	{
		mem->bios_mapped = true;
	}

	if (mem->mbc_type == GB_MBC_TYPE_2)
	{
		mem->mbc2.rom_bank = 1;
	}
	// TODO(stefalie): Are MBC3 values correct if initialized to 0?
	// gbdev.io doesn't give default values.
	// TODO(stefalie): How to init RTC in MBC3?

	// (see page 18 of the GameBoy CPU Manual)
	gb->cpu.a = 0x01;
	gb->cpu.f = 0xB0;
	gb->cpu.bc = 0x0013;
	gb->cpu.de = 0x00D8;
	gb->cpu.hl = 0x014D;
	gb->cpu.sp = 0xFFFE;

	// TODO(stefalie): Which PPU mode and with which LY value should
	// the GameBoy start in when skipping the BIOS?
	// no$gmb seems to start in mode 1 with LY == 0.
	// This shouldn't really matter, should it?
	// VBA starts in mode 2.
	gb->ppu.stat.mode = GB_PPU_MODE_OAM_SCAN;
	gb->ppu.stat.coincidence_flag = gb->ppu.ly == gb->ppu.lyc;
	gb->ppu.stat._ = 1;

	gb->joypad.buttons = 0x0F;
	gb->joypad.dpad = 0x0F;
	gb->joypad.dpad_select = 1;
	gb->joypad.buttons_select = 1;
	gb->joypad._ = 0x3;

	gb__MemoryWriteByte(gb, 0xFF05, 0x00);
	gb__MemoryWriteByte(gb, 0xFF06, 0x00);
	gb__MemoryWriteByte(gb, 0xFF07, 0x00);
	gb__MemoryWriteByte(gb, 0xFF10, 0x80);
	gb__MemoryWriteByte(gb, 0xFF11, 0xBF);
	gb__MemoryWriteByte(gb, 0xFF12, 0xF3);
	gb__MemoryWriteByte(gb, 0xFF14, 0xBF);
	gb__MemoryWriteByte(gb, 0xFF16, 0x3F);
	gb__MemoryWriteByte(gb, 0xFF17, 0x00);
	gb__MemoryWriteByte(gb, 0xFF19, 0xBF);
	gb__MemoryWriteByte(gb, 0xFF1A, 0x7F);
	gb__MemoryWriteByte(gb, 0xFF1B, 0xFF);
	gb__MemoryWriteByte(gb, 0xFF1C, 0x9F);
	gb__MemoryWriteByte(gb, 0xFF1E, 0xBF);
	gb__MemoryWriteByte(gb, 0xFF20, 0xFF);
	gb__MemoryWriteByte(gb, 0xFF21, 0x00);
	gb__MemoryWriteByte(gb, 0xFF22, 0x00);
	gb__MemoryWriteByte(gb, 0xFF23, 0xBF);
	gb__MemoryWriteByte(gb, 0xFF24, 0x77);
	gb__MemoryWriteByte(gb, 0xFF25, 0xF3);
	gb__MemoryWriteByte(gb, 0xFF26, 0xF1);
	gb__MemoryWriteByte(gb, 0xFF40, 0x91);
	gb__MemoryWriteByte(gb, 0xFF42, 0x00);
	gb__MemoryWriteByte(gb, 0xFF43, 0x00);
	gb__MemoryWriteByte(gb, 0xFF45, 0x00);
	gb__MemoryWriteByte(gb, 0xFF47, 0xFC);
	gb__MemoryWriteByte(gb, 0xFF48, 0xFF);
	gb__MemoryWriteByte(gb, 0xFF49, 0xFF);
	gb__MemoryWriteByte(gb, 0xFF4A, 0x00);
	gb__MemoryWriteByte(gb, 0xFF4B, 0x00);
	gb__MemoryWriteByte(gb, 0xFFFF, 0x00);
}

typedef struct
{
	char *name;
	uint8_t num_operand_bytes;
	uint8_t num_machine_cycles_wo_branch;
	uint8_t num_machine_cycles_with_branch;  // 0 if no branch
} gb__InstructionInfo;

static const gb__InstructionInfo gb__basic_instruction_infos[256] = {
	[0x00] = { "NOP", 0, 1 },
	[0x01] = { "LD BC, u16", 2, 3 },
	[0x02] = { "LD (BC), A", 0, 2 },
	[0x03] = { "INC BC", 0, 2 },
	[0x04] = { "INC B", 0, 1 },
	[0x05] = { "DEC B", 0, 1 },
	[0x06] = { "LD B, u8", 1, 2 },
	[0x07] = { "RLCA", 0, 1 },

	[0x08] = { "LD (u16), SP", 2, 5 },
	[0x09] = { "ADD HL, BC", 0, 2 },
	[0x0A] = { "LD A, (BC)", 0, 2 },
	[0x0B] = { "DEC BC", 0, 2 },
	[0x0C] = { "INC C", 0, 1 },
	[0x0D] = { "DEC C", 0, 1 },
	[0x0E] = { "LD C, u8", 1, 2 },
	[0x0F] = { "RRCA", 0, 1 },

	[0x10] = { "STOP", 0, 1 },
	[0x11] = { "LD DE, u16", 2, 3 },
	[0x12] = { "LD (DE), A", 0, 2 },
	[0x13] = { "INC DE", 0, 2 },
	[0x14] = { "INC D", 0, 1 },
	[0x15] = { "DEC D", 0, 1 },
	[0x16] = { "LD D, u8", 1, 2 },
	[0x17] = { "RLA", 0, 1 },

	[0x18] = { "JR i8", 1, 3 },
	[0x19] = { "ADD HL, DE", 0, 2 },
	[0x1A] = { "LD A, (DE)", 0, 2 },
	[0x1B] = { "DEC DE", 0, 2 },
	[0x1C] = { "INC E", 0, 1 },
	[0x1D] = { "DEC E", 0, 1 },
	[0x1E] = { "LD E, u8", 1, 2 },
	[0x1F] = { "RRA", 0, 1 },

	[0x20] = { "JR NZ, i8", 1, 2, 3 },
	[0x21] = { "LD HL, u16", 2, 3 },
	[0x22] = { "LD (HL+), A", 0, 2 },
	[0x23] = { "INC HL", 0, 2 },
	[0x24] = { "INC H", 0, 1 },
	[0x25] = { "DEC H", 0, 1 },
	[0x26] = { "LD H, u8", 1, 2 },
	[0x27] = { "DAA", 0, 1 },

	[0x28] = { "JR Z, i8", 1, 2, 3 },
	[0x29] = { "ADD HL, HL", 0, 2 },
	[0x2A] = { "LD A, (HL+)", 0, 2 },
	[0x2B] = { "DEC HL", 0, 2 },
	[0x2C] = { "INC L", 0, 1 },
	[0x2D] = { "DEC L", 0, 1 },
	[0x2E] = { "LD L, u8", 1, 2 },
	[0x2F] = { "CPL", 0, 1 },

	[0x30] = { "JR NC, i8", 1, 2, 3 },
	[0x31] = { "LD SP, u16", 2, 3 },
	[0x32] = { "LD (HL-), A", 0, 2 },
	[0x33] = { "INC SP", 0, 2 },
	[0x34] = { "INC (HL)", 0, 3 },
	[0x35] = { "DEC (HL)", 0, 3 },
	[0x36] = { "LD (HL), u8", 1, 3 },
	[0x37] = { "SCF", 0, 1 },

	[0x38] = { "JR C, i8", 1, 2, 3 },
	[0x39] = { "ADD HL, SP", 0, 2 },
	[0x3A] = { "LD A, (HL-)", 0, 2 },
	[0x3B] = { "DEC SP", 0, 2 },
	[0x3C] = { "INC A", 0, 1 },
	[0x3D] = { "DEC A", 0, 1 },
	[0x3E] = { "LD A, u8", 1, 2 },
	[0x3F] = { "CCF", 0, 1 },

	[0x40] = { "LD B, B", 0, 1 },
	[0x41] = { "LD B, C", 0, 1 },
	[0x42] = { "LD B, D", 0, 1 },
	[0x43] = { "LD B, E", 0, 1 },
	[0x44] = { "LD B, H", 0, 1 },
	[0x45] = { "LD B, L", 0, 1 },
	[0x46] = { "LD B, (HL)", 0, 2 },
	[0x47] = { "LD B, A", 0, 1 },

	[0x48] = { "LD C, B", 0, 1 },
	[0x49] = { "LD C, C", 0, 1 },
	[0x4A] = { "LD C, D", 0, 1 },
	[0x4B] = { "LD C, E", 0, 1 },
	[0x4C] = { "LD C, H", 0, 1 },
	[0x4D] = { "LD C, L", 0, 1 },
	[0x4E] = { "LD C, (HL)", 0, 2 },
	[0x4F] = { "LD C, A", 0, 1 },

	[0x50] = { "LD D, B", 0, 1 },
	[0x51] = { "LD D, C", 0, 1 },
	[0x52] = { "LD D, D", 0, 1 },
	[0x53] = { "LD D, E", 0, 1 },
	[0x54] = { "LD D, H", 0, 1 },
	[0x55] = { "LD D, L", 0, 1 },
	[0x56] = { "LD D, (HL)", 0, 2 },
	[0x57] = { "LD D, A", 0, 1 },

	[0x58] = { "LD E, B", 0, 1 },
	[0x59] = { "LD E, C", 0, 1 },
	[0x5A] = { "LD E, D", 0, 1 },
	[0x5B] = { "LD E, E", 0, 1 },
	[0x5C] = { "LD E, H", 0, 1 },
	[0x5D] = { "LD E, L", 0, 1 },
	[0x5E] = { "LD E, (HL)", 0, 2 },
	[0x5F] = { "LD E, A", 0, 1 },

	[0x60] = { "LD H, B", 0, 1 },
	[0x61] = { "LD H, C", 0, 1 },
	[0x62] = { "LD H, D", 0, 1 },
	[0x63] = { "LD H, E", 0, 1 },
	[0x64] = { "LD H, H", 0, 1 },
	[0x65] = { "LD H, L", 0, 1 },
	[0x66] = { "LD H, (HL)", 0, 2 },
	[0x67] = { "LD H, A", 0, 1 },

	[0x68] = { "LD L, B", 0, 1 },
	[0x69] = { "LD L, C", 0, 1 },
	[0x6A] = { "LD L, D", 0, 1 },
	[0x6B] = { "LD L, E", 0, 1 },
	[0x6C] = { "LD L, H", 0, 1 },
	[0x6D] = { "LD L, L", 0, 1 },
	[0x6E] = { "LD L, (HL)", 0, 2 },
	[0x6F] = { "LD L, A", 0, 1 },

	[0x70] = { "LD (HL), B", 0, 2 },
	[0x71] = { "LD (HL), C", 0, 2 },
	[0x72] = { "LD (HL), D", 0, 2 },
	[0x73] = { "LD (HL), E", 0, 2 },
	[0x74] = { "LD (HL), H", 0, 2 },
	[0x75] = { "LD (HL), L", 0, 2 },
	[0x76] = { "HALT", 0, 2 },
	[0x77] = { "LD (HL), A", 0, 2 },

	[0x78] = { "LD A, B", 0, 1 },
	[0x79] = { "LD A, C", 0, 1 },
	[0x7A] = { "LD A, D", 0, 1 },
	[0x7B] = { "LD A, E", 0, 1 },
	[0x7C] = { "LD A, H", 0, 1 },
	[0x7D] = { "LD A, L", 0, 1 },
	[0x7E] = { "LD A, (HL)", 0, 2 },
	[0x7F] = { "LD A, A", 0, 1 },

	[0x80] = { "ADD A, B", 0, 1 },
	[0x81] = { "ADD A, C", 0, 1 },
	[0x82] = { "ADD A, D", 0, 1 },
	[0x83] = { "ADD A, E", 0, 1 },
	[0x84] = { "ADD A, H", 0, 1 },
	[0x85] = { "ADD A, L", 0, 1 },
	[0x86] = { "ADD A, (HL)", 0, 2 },
	[0x87] = { "ADD A, A", 0, 1 },

	[0x88] = { "ADC A, B", 0, 1 },
	[0x89] = { "ADC A, C", 0, 1 },
	[0x8A] = { "ADC A, D", 0, 1 },
	[0x8B] = { "ADC A, E", 0, 1 },
	[0x8C] = { "ADC A, H", 0, 1 },
	[0x8D] = { "ADC A, L", 0, 1 },
	[0x8E] = { "ADC A, (HL)", 0, 2 },
	[0x8F] = { "ADC A, A", 0, 1 },

	[0x90] = { "SUB A, B", 0, 1 },
	[0x91] = { "SUB A, C", 0, 1 },
	[0x92] = { "SUB A, D", 0, 1 },
	[0x93] = { "SUB A, E", 0, 1 },
	[0x94] = { "SUB A, H", 0, 1 },
	[0x95] = { "SUB A, L", 0, 1 },
	[0x96] = { "SUB A, (HL)", 0, 2 },
	[0x97] = { "SUB A, A", 0, 1 },

	[0x98] = { "SBC A, B", 0, 1 },
	[0x99] = { "SBC A, C", 0, 1 },
	[0x9A] = { "SBC A, D", 0, 1 },
	[0x9B] = { "SBC A, E", 0, 1 },
	[0x9C] = { "SBC A, H", 0, 1 },
	[0x9D] = { "SBC A, L", 0, 1 },
	[0x9E] = { "SBC A, (HL)", 0, 2 },
	[0x9F] = { "SBC A, A", 0, 1 },

	[0xA0] = { "AND A, B", 0, 1 },
	[0xA1] = { "AND A, C", 0, 1 },
	[0xA2] = { "AND A, D", 0, 1 },
	[0xA3] = { "AND A, E", 0, 1 },
	[0xA4] = { "AND A, H", 0, 1 },
	[0xA5] = { "AND A, L", 0, 1 },
	[0xA6] = { "AND A, (HL)", 0, 2 },
	[0xA7] = { "AND A, A", 0, 1 },

	[0xA8] = { "XOR A, B", 0, 1 },
	[0xA9] = { "XOR A, C", 0, 1 },
	[0xAA] = { "XOR A, D", 0, 1 },
	[0xAB] = { "XOR A, E", 0, 1 },
	[0xAC] = { "XOR A, H", 0, 1 },
	[0xAD] = { "XOR A, L", 0, 1 },
	[0xAE] = { "XOR A, (HL)", 0, 2 },
	[0xAF] = { "XOR A, A", 0, 1 },

	[0xB0] = { "OR A, B", 0, 1 },
	[0xB1] = { "OR A, C", 0, 1 },
	[0xB2] = { "OR A, D", 0, 1 },
	[0xB3] = { "OR A, E", 0, 1 },
	[0xB4] = { "OR A, H", 0, 1 },
	[0xB5] = { "OR A, L", 0, 1 },
	[0xB6] = { "OR A, (HL)", 0, 2 },
	[0xB7] = { "OR A, A", 0, 1 },

	[0xB8] = { "CP A, B", 0, 1 },
	[0xB9] = { "CP A, C", 0, 1 },
	[0xBA] = { "CP A, D", 0, 1 },
	[0xBB] = { "CP A, E", 0, 1 },
	[0xBC] = { "CP A, H", 0, 1 },
	[0xBD] = { "CP A, L", 0, 1 },
	[0xBE] = { "CP A, (HL)", 0, 2 },
	[0xBF] = { "CP A, A", 0, 1 },

	[0xC0] = { "RET NZ", 0, 2, 5 },
	[0xC1] = { "POP BC", 0, 3 },
	[0xC2] = { "JP NZ, u16", 2, 3, 4 },
	[0xC3] = { "JP u16", 2, 4 },
	[0xC4] = { "CALL NZ, u16", 2, 3, 6 },
	[0xC5] = { "PUSH BC", 0, 4 },
	[0xC6] = { "ADD A, u8", 1, 2 },
	[0xC7] = { "RST 00h", 0, 4 },

	[0xC8] = { "RET Z", 0, 2, 5 },
	[0xC9] = { "RET", 0, 4 },
	[0xCA] = { "JP Z, u16", 2, 3, 4 },
	[0xCB] = { "PREFIX", 0, 0 },  // Num cycles is 0 here because it's taken from the extended instruction table.
	[0xCC] = { "CALL Z, u16", 2, 3, 6 },
	[0xCD] = { "CALL u16", 2, 6 },
	[0xCE] = { "ADC A, u8", 1, 2 },
	[0xCF] = { "RST 08h", 0, 4 },

	[0xD0] = { "RET NC", 0, 2, 5 },
	[0xD1] = { "POP DE", 0, 3 },
	[0xD2] = { "JP NC, u16", 2, 3, 4 },
	[0xD4] = { "CALL NC, u16", 2, 3, 6 },
	[0xD5] = { "PUSH DE", 0, 4 },
	[0xD6] = { "SUB A, u8", 1, 2 },
	[0xD7] = { "RST 10h", 0, 4 },

	[0xD8] = { "RET C", 0, 2, 5 },
	[0xD9] = { "RETI", 0, 4 },
	[0xDA] = { "JP C, u16", 2, 3, 4 },
	[0xDC] = { "CALL C, u16", 2, 3, 6 },
	[0xDE] = { "SBC A, u8", 1, 2 },
	[0xDF] = { "RST 18h", 0, 4 },

	[0xE0] = { "LD (FF00+u8), A", 1, 3 },
	[0xE1] = { "POP HL", 0, 3 },
	[0xE2] = { "LD (FF00+C), A", 0, 2 },
	[0xE5] = { "PUSH HL", 0, 4 },
	[0xE6] = { "AND A, u8", 1, 2 },
	[0xE7] = { "RST 20h", 0, 4 },

	[0xE8] = { "ADD SP, i8", 1, 4 },
	[0xE9] = { "JP HL", 0, 1 },
	[0xEA] = { "LD (u16), A", 2, 4 },
	[0xEE] = { "XOR A, u8", 1, 2 },
	[0xEF] = { "RST 28h", 0, 4 },

	[0xF0] = { "LD A, (FF00+u8)", 1, 3 },
	[0xF1] = { "POP AF", 0, 3 },
	[0xF2] = { "LD A, (FF00+C)", 0, 2 },
	[0xF3] = { "DI", 0, 1 },
	[0xF5] = { "PUSH AF", 0, 4 },
	[0xF6] = { "OR A, u8", 1, 2 },
	[0xF7] = { "RST 30h", 0, 4 },

	[0xF8] = { "LD HL, SP+i8", 1, 3 },
	[0xF9] = { "LD SP, HL", 0, 2 },
	[0xFA] = { "LD A, (u16)", 2, 4 },
	[0xFB] = { "EI", 0, 1 },
	[0xFE] = { "CP A, u8", 1, 2 },
	[0xFF] = { "RST 38h", 0, 4 },
};

static const gb__InstructionInfo gb__extended_instruction_infos[256] = {
	[0x00] = { "RLC B", 0, 2 },
	[0x01] = { "RLC C", 0, 2 },
	[0x02] = { "RLC D", 0, 2 },
	[0x03] = { "RLC E", 0, 2 },
	[0x04] = { "RLC H", 0, 2 },
	[0x05] = { "RLC L", 0, 2 },
	[0x06] = { "RLC (HL)", 0, 4 },
	[0x07] = { "RLC A", 0, 2 },

	[0x08] = { "RRC B", 0, 2 },
	[0x09] = { "RRC C", 0, 2 },
	[0x0A] = { "RRC D", 0, 2 },
	[0x0B] = { "RRC E", 0, 2 },
	[0x0C] = { "RRC H", 0, 2 },
	[0x0D] = { "RRC L", 0, 2 },
	[0x0E] = { "RRC (HL)", 0, 4 },
	[0x0F] = { "RRC A", 0, 2 },

	[0x10] = { "RL B", 0, 2 },
	[0x11] = { "RL C", 0, 2 },
	[0x12] = { "RL D", 0, 2 },
	[0x13] = { "RL E", 0, 2 },
	[0x14] = { "RL H", 0, 2 },
	[0x15] = { "RL L", 0, 2 },
	[0x16] = { "RL (HL)", 0, 4 },
	[0x17] = { "RL A", 0, 2 },

	[0x18] = { "RR B", 0, 2 },
	[0x19] = { "RR C", 0, 2 },
	[0x1A] = { "RR D", 0, 2 },
	[0x1B] = { "RR E", 0, 2 },
	[0x1C] = { "RR H", 0, 2 },
	[0x1D] = { "RR L", 0, 2 },
	[0x1E] = { "RR (HL)", 0, 4 },
	[0x1F] = { "RR A", 0, 2 },

	[0x20] = { "SLA B", 0, 2 },
	[0x21] = { "SLA C", 0, 2 },
	[0x22] = { "SLA D", 0, 2 },
	[0x23] = { "SLA E", 0, 2 },
	[0x24] = { "SLA H", 0, 2 },
	[0x25] = { "SLA L", 0, 2 },
	[0x26] = { "SLA (HL)", 0, 4 },
	[0x27] = { "SLA A", 0, 2 },

	[0x28] = { "SRA B", 0, 2 },
	[0x29] = { "SRA C", 0, 2 },
	[0x2A] = { "SRA D", 0, 2 },
	[0x2B] = { "SRA E", 0, 2 },
	[0x2C] = { "SRA H", 0, 2 },
	[0x2D] = { "SRA L", 0, 2 },
	[0x2E] = { "SRA (HL)", 0, 4 },
	[0x2F] = { "SRA A", 0, 2 },

	[0x30] = { "SWAP B", 0, 2 },
	[0x31] = { "SWAP C", 0, 2 },
	[0x32] = { "SWAP D", 0, 2 },
	[0x33] = { "SWAP E", 0, 2 },
	[0x34] = { "SWAP H", 0, 2 },
	[0x35] = { "SWAP L", 0, 2 },
	[0x36] = { "SWAP (HL)", 0, 4 },
	[0x37] = { "SWAP A", 0, 2 },

	[0x38] = { "SRL B", 0, 2 },
	[0x39] = { "SRL C", 0, 2 },
	[0x3A] = { "SRL D", 0, 2 },
	[0x3B] = { "SRL E", 0, 2 },
	[0x3C] = { "SRL H", 0, 2 },
	[0x3D] = { "SRL L", 0, 2 },
	[0x3E] = { "SRL (HL)", 0, 4 },
	[0x3F] = { "SRL A", 0, 2 },

	[0x40] = { "BIT 0, B", 0, 2 },
	[0x41] = { "BIT 0, C", 0, 2 },
	[0x42] = { "BIT 0, D", 0, 2 },
	[0x43] = { "BIT 0, E", 0, 2 },
	[0x44] = { "BIT 0, H", 0, 2 },
	[0x45] = { "BIT 0, L", 0, 2 },
	[0x46] = { "BIT 0, (HL)", 0, 3 },
	[0x47] = { "BIT 0, A", 0, 2 },

	[0x48] = { "BIT 1, B", 0, 2 },
	[0x49] = { "BIT 1, C", 0, 2 },
	[0x4A] = { "BIT 1, D", 0, 2 },
	[0x4B] = { "BIT 1, E", 0, 2 },
	[0x4C] = { "BIT 1, H", 0, 2 },
	[0x4D] = { "BIT 1, L", 0, 2 },
	[0x4E] = { "BIT 1, (HL)", 0, 3 },
	[0x4F] = { "BIT 1, A", 0, 2 },

	[0x50] = { "BIT 2, B", 0, 2 },
	[0x51] = { "BIT 2, C", 0, 2 },
	[0x52] = { "BIT 2, D", 0, 2 },
	[0x53] = { "BIT 2, E", 0, 2 },
	[0x54] = { "BIT 2, H", 0, 2 },
	[0x55] = { "BIT 2, L", 0, 2 },
	[0x56] = { "BIT 2, (HL)", 0, 3 },
	[0x57] = { "BIT 2, A", 0, 2 },

	[0x58] = { "BIT 3, B", 0, 2 },
	[0x59] = { "BIT 3, C", 0, 2 },
	[0x5A] = { "BIT 3, D", 0, 2 },
	[0x5B] = { "BIT 3, E", 0, 2 },
	[0x5C] = { "BIT 3, H", 0, 2 },
	[0x5D] = { "BIT 3, L", 0, 2 },
	[0x5E] = { "BIT 3, (HL)", 0, 3 },
	[0x5F] = { "BIT 3, A", 0, 2 },

	[0x60] = { "BIT 4, B", 0, 2 },
	[0x61] = { "BIT 4, C", 0, 2 },
	[0x62] = { "BIT 4, D", 0, 2 },
	[0x63] = { "BIT 4, E", 0, 2 },
	[0x64] = { "BIT 4, H", 0, 2 },
	[0x65] = { "BIT 4, L", 0, 2 },
	[0x66] = { "BIT 4, (HL)", 0, 3 },
	[0x67] = { "BIT 4, A", 0, 2 },

	[0x68] = { "BIT 5, B", 0, 2 },
	[0x69] = { "BIT 5, C", 0, 2 },
	[0x6A] = { "BIT 5, D", 0, 2 },
	[0x6B] = { "BIT 5, E", 0, 2 },
	[0x6C] = { "BIT 5, H", 0, 2 },
	[0x6D] = { "BIT 5, L", 0, 2 },
	[0x6E] = { "BIT 5, (HL)", 0, 3 },
	[0x6F] = { "BIT 5, A", 0, 2 },

	[0x70] = { "BIT 6, B", 0, 2 },
	[0x71] = { "BIT 6, C", 0, 2 },
	[0x72] = { "BIT 6, D", 0, 2 },
	[0x73] = { "BIT 6, E", 0, 2 },
	[0x74] = { "BIT 6, H", 0, 2 },
	[0x75] = { "BIT 6, L", 0, 2 },
	[0x76] = { "BIT 6, (HL)", 0, 3 },
	[0x77] = { "BIT 6, A", 0, 2 },

	[0x78] = { "BIT 7, B", 0, 2 },
	[0x79] = { "BIT 7, C", 0, 2 },
	[0x7A] = { "BIT 7, D", 0, 2 },
	[0x7B] = { "BIT 7, E", 0, 2 },
	[0x7C] = { "BIT 7, H", 0, 2 },
	[0x7D] = { "BIT 7, L", 0, 2 },
	[0x7E] = { "BIT 7, (HL)", 0, 3 },
	[0x7F] = { "BIT 7, A", 0, 2 },

	[0x80] = { "RES 0, B", 0, 2 },
	[0x81] = { "RES 0, C", 0, 2 },
	[0x82] = { "RES 0, D", 0, 2 },
	[0x83] = { "RES 0, E", 0, 2 },
	[0x84] = { "RES 0, H", 0, 2 },
	[0x85] = { "RES 0, L", 0, 2 },
	[0x86] = { "RES 0, (HL)", 0, 4 },
	[0x87] = { "RES 0, A", 0, 2 },

	[0x88] = { "RES 1, B", 0, 2 },
	[0x89] = { "RES 1, C", 0, 2 },
	[0x8A] = { "RES 1, D", 0, 2 },
	[0x8B] = { "RES 1, E", 0, 2 },
	[0x8C] = { "RES 1, H", 0, 2 },
	[0x8D] = { "RES 1, L", 0, 2 },
	[0x8E] = { "RES 1, (HL)", 0, 4 },
	[0x8F] = { "RES 1, A", 0, 2 },

	[0x90] = { "RES 2, B", 0, 2 },
	[0x91] = { "RES 2, C", 0, 2 },
	[0x92] = { "RES 2, D", 0, 2 },
	[0x93] = { "RES 2, E", 0, 2 },
	[0x94] = { "RES 2, H", 0, 2 },
	[0x95] = { "RES 2, L", 0, 2 },
	[0x96] = { "RES 2, (HL)", 0, 4 },
	[0x97] = { "RES 2, A", 0, 2 },

	[0x98] = { "RES 3, B", 0, 2 },
	[0x99] = { "RES 3, C", 0, 2 },
	[0x9A] = { "RES 3, D", 0, 2 },
	[0x9B] = { "RES 3, E", 0, 2 },
	[0x9C] = { "RES 3, H", 0, 2 },
	[0x9D] = { "RES 3, L", 0, 2 },
	[0x9E] = { "RES 3, (HL)", 0, 4 },
	[0x9F] = { "RES 3, A", 0, 2 },

	[0xA0] = { "RES 4, B", 0, 2 },
	[0xA1] = { "RES 4, C", 0, 2 },
	[0xA2] = { "RES 4, D", 0, 2 },
	[0xA3] = { "RES 4, E", 0, 2 },
	[0xA4] = { "RES 4, H", 0, 2 },
	[0xA5] = { "RES 4, L", 0, 2 },
	[0xA6] = { "RES 4, (HL)", 0, 4 },
	[0xA7] = { "RES 4, A", 0, 2 },

	[0xA8] = { "RES 5, B", 0, 2 },
	[0xA9] = { "RES 5, C", 0, 2 },
	[0xAA] = { "RES 5, D", 0, 2 },
	[0xAB] = { "RES 5, E", 0, 2 },
	[0xAC] = { "RES 5, H", 0, 2 },
	[0xAD] = { "RES 5, L", 0, 2 },
	[0xAE] = { "RES 5, (HL)", 0, 4 },
	[0xAF] = { "RES 5, A", 0, 2 },

	[0xB0] = { "RES 6, B", 0, 2 },
	[0xB1] = { "RES 6, C", 0, 2 },
	[0xB2] = { "RES 6, D", 0, 2 },
	[0xB3] = { "RES 6, E", 0, 2 },
	[0xB4] = { "RES 6, H", 0, 2 },
	[0xB5] = { "RES 6, L", 0, 2 },
	[0xB6] = { "RES 6, (HL)", 0, 4 },
	[0xB7] = { "RES 6, A", 0, 2 },

	[0xB8] = { "RES 7, B", 0, 2 },
	[0xB9] = { "RES 7, C", 0, 2 },
	[0xBA] = { "RES 7, D", 0, 2 },
	[0xBB] = { "RES 7, E", 0, 2 },
	[0xBC] = { "RES 7, H", 0, 2 },
	[0xBD] = { "RES 7, L", 0, 2 },
	[0xBE] = { "RES 7, (HL)", 0, 4 },
	[0xBF] = { "RES 7, A", 0, 2 },

	[0xC0] = { "SET 0, B", 0, 2 },
	[0xC1] = { "SET 0, C", 0, 2 },
	[0xC2] = { "SET 0, D", 0, 2 },
	[0xC3] = { "SET 0, E", 0, 2 },
	[0xC4] = { "SET 0, H", 0, 2 },
	[0xC5] = { "SET 0, L", 0, 2 },
	[0xC6] = { "SET 0, (HL)", 0, 4 },
	[0xC7] = { "SET 0, A", 0, 2 },

	[0xC8] = { "SET 1, B", 0, 2 },
	[0xC9] = { "SET 1, C", 0, 2 },
	[0xCA] = { "SET 1, D", 0, 2 },
	[0xCB] = { "SET 1, E", 0, 2 },
	[0xCC] = { "SET 1, H", 0, 2 },
	[0xCD] = { "SET 1, L", 0, 2 },
	[0xCE] = { "SET 1, (HL)", 0, 4 },
	[0xCF] = { "SET 1, A", 0, 2 },

	[0xD0] = { "SET 2, B", 0, 2 },
	[0xD1] = { "SET 2, C", 0, 2 },
	[0xD2] = { "SET 2, D", 0, 2 },
	[0xD3] = { "SET 2, E", 0, 2 },
	[0xD4] = { "SET 2, H", 0, 2 },
	[0xD5] = { "SET 2, L", 0, 2 },
	[0xD6] = { "SET 2, (HL)", 0, 4 },
	[0xD7] = { "SET 2, A", 0, 2 },

	[0xD8] = { "SET 3, B", 0, 2 },
	[0xD9] = { "SET 3, C", 0, 2 },
	[0xDA] = { "SET 3, D", 0, 2 },
	[0xDB] = { "SET 3, E", 0, 2 },
	[0xDC] = { "SET 3, H", 0, 2 },
	[0xDD] = { "SET 3, L", 0, 2 },
	[0xDE] = { "SET 3, (HL)", 0, 4 },
	[0xDF] = { "SET 3, A", 0, 2 },

	[0xE0] = { "SET 4, B", 0, 2 },
	[0xE1] = { "SET 4, C", 0, 2 },
	[0xE2] = { "SET 4, D", 0, 2 },
	[0xE3] = { "SET 4, E", 0, 2 },
	[0xE4] = { "SET 4, H", 0, 2 },
	[0xE5] = { "SET 4, L", 0, 2 },
	[0xE6] = { "SET 4, (HL)", 0, 4 },
	[0xE7] = { "SET 4, A", 0, 2 },

	[0xE8] = { "SET 5, B", 0, 2 },
	[0xE9] = { "SET 5, C", 0, 2 },
	[0xEA] = { "SET 5, D", 0, 2 },
	[0xEB] = { "SET 5, E", 0, 2 },
	[0xEC] = { "SET 5, H", 0, 2 },
	[0xED] = { "SET 5, L", 0, 2 },
	[0xEE] = { "SET 5, (HL)", 0, 4 },
	[0xEF] = { "SET 5, A", 0, 2 },

	[0xF0] = { "SET 6, B", 0, 2 },
	[0xF1] = { "SET 6, C", 0, 2 },
	[0xF2] = { "SET 6, D", 0, 2 },
	[0xF3] = { "SET 6, E", 0, 2 },
	[0xF4] = { "SET 6, H", 0, 2 },
	[0xF5] = { "SET 6, L", 0, 2 },
	[0xF6] = { "SET 6, (HL)", 0, 4 },
	[0xF7] = { "SET 6, A", 0, 2 },

	[0xF8] = { "SET 7, B", 0, 2 },
	[0xF9] = { "SET 7, C", 0, 2 },
	[0xFA] = { "SET 7, D", 0, 2 },
	[0xFB] = { "SET 7, E", 0, 2 },
	[0xFC] = { "SET 7, H", 0, 2 },
	[0xFD] = { "SET 7, L", 0, 2 },
	[0xFE] = { "SET 7, (HL)", 0, 4 },
	[0xFF] = { "SET 7, A", 0, 2 },
};

static const uint8_t extended_inst_prefix = 0xCB;

gb_Instruction
gb_FetchInstruction(const gb_GameBoy *gb, uint16_t addr)
{
	gb_Instruction inst = {
		.opcode = gb_MemoryReadByte(gb, addr),
	};

	gb__InstructionInfo info;

	if (inst.opcode == extended_inst_prefix)
	{
		++addr;
		inst.is_extended = true;
		inst.opcode = gb_MemoryReadByte(gb, addr);
		info = gb__extended_instruction_infos[inst.opcode];
		assert(info.num_operand_bytes == 0);
	}
	else
	{
		inst.is_extended = false;
		info = gb__basic_instruction_infos[inst.opcode];
	}

	inst.num_operand_bytes = info.num_operand_bytes;

	if (inst.num_operand_bytes == 1)
	{
		inst.operand_byte = gb_MemoryReadByte(gb, addr + 1);
	}
	else if (inst.num_operand_bytes == 2)
	{
		inst.operand_word = gb__MemoryReadWord(gb, addr + 1);
	}

	return inst;
}

uint16_t
gb_InstructionSize(gb_Instruction inst)
{
	return inst.is_extended ? 2 : 1 + inst.num_operand_bytes;
}

// TODO(stefalie): get rid of snprintf, strlen, memcpy to avoid std includes.
// should all be easy, and from snprintf you only need to know how to convert 1 byte numbers to 2-char strings.
size_t
gb_DisassembleInstruction(gb_Instruction inst, char str_buf[], size_t str_buf_len)
{
	const gb__InstructionInfo info =
			inst.is_extended ? gb__extended_instruction_infos[inst.opcode] : gb__basic_instruction_infos[inst.opcode];
	if (!info.name)
	{
		snprintf(str_buf, str_buf_len, "OpCode 0x%02X info is missing!", inst.opcode);
	}
	else if (info.num_operand_bytes == 0)
	{
		size_t len = strlen(info.name) + 1;
		assert(len <= str_buf_len);
		len = MIN(len, str_buf_len);
		memcpy(str_buf, info.name, len);
	}
	else
	{
		assert(info.num_operand_bytes > 0 && info.num_operand_bytes < 3);
		snprintf(str_buf, str_buf_len, "%s, 0x%0*X", info.name, info.num_operand_bytes * 2,
				info.num_operand_bytes == 1 ? inst.operand_byte : inst.operand_word);
	}
	return strlen(str_buf);
}

static inline void
gb__SetFlags(gb_GameBoy *gb, bool zero, bool subtract, bool half_carry, bool carry)
{
	gb->cpu.flags.zero = zero ? 1 : 0;
	gb->cpu.flags.subtract = subtract ? 1 : 0;
	gb->cpu.flags.half_carry = half_carry ? 1 : 0;
	gb->cpu.flags.carry = carry ? 1 : 0;
}

static uint8_t *
gb__MapIndexToReg(gb_GameBoy *gb, uint8_t index)
{
	index &= 0x07;

	switch (index)
	{
	case 0:
		return &gb->cpu.b;
	case 1:
		return &gb->cpu.c;
	case 2:
		return &gb->cpu.d;
	case 3:
		return &gb->cpu.e;
	case 4:
		return &gb->cpu.h;
	case 5:
		return &gb->cpu.l;
	case 7:
		return &gb->cpu.a;
	default:
		assert(false);
		return NULL;
	}
}

static uint8_t
gb__RotateLeftCircular(gb_GameBoy *gb, uint8_t val, bool clear_zero)
{
	const bool co = val & 0x80;
	val <<= 1u;
	if (co)
	{
		val |= 0x01;
	}
	gb__SetFlags(gb, clear_zero ? false : val == 0, false, false, co);
	return val;
}

static uint8_t
gb__RotateLeft(gb_GameBoy *gb, uint8_t val, bool clear_zero)
{
	const bool co = val & 0x80;
	val <<= 1u;
	val |= gb->cpu.flags.carry;
	gb__SetFlags(gb, clear_zero ? false : val == 0, false, false, co);
	return val;
}

static uint8_t
gb__RotateRightCircular(gb_GameBoy *gb, uint8_t val, bool clear_zero)
{
	const bool co = val & 0x01;
	val >>= 1u;
	if (co)
	{
		val |= 0x80;
	}
	gb__SetFlags(gb, clear_zero ? false : val == 0, false, false, co);
	return val;
}

static uint8_t
gb__RotateRight(gb_GameBoy *gb, uint8_t val, bool clear_zero)
{
	const bool co = val & 0x01;
	val >>= 1u;
	val |= gb->cpu.flags.carry << 7u;
	gb__SetFlags(gb, clear_zero ? false : val == 0, false, false, co);
	return val;
}

static void
gb__Add(gb_GameBoy *gb, uint8_t rhs, bool carry_in)
{
	uint8_t ci = (carry_in ? gb->cpu.flags.carry : 0);
	uint16_t sum = gb->cpu.a + rhs + ci;
	bool half_carry = (gb->cpu.a & 0x0F) + (rhs & 0x0F) + ci > 0x0F;
	bool co = sum > 0xFF;
	gb->cpu.a = (uint8_t)sum;
	gb__SetFlags(gb, gb->cpu.a == 0, false, half_carry, co);
}

// Could this be implemented by doing gb__Add(lhs, 2's complement of (rhs + carry_if_enable)),
// and then by flipping the half carry, carry, and negate flags?
// I think so. See here in MAME where SBC and ADC are essentially identical (besides the +/-):
// https://github.com/mamedev/mame/blob/1fdf6d10a7907bc92cd9f36eda1eca0484e47aba/src/devices/cpu/lr35902/opc_main.hxx#L4
static void
gb__Sub(gb_GameBoy *gb, uint8_t rhs, bool carry_in)
{
	uint8_t ci = (carry_in ? gb->cpu.flags.carry : 0);
	uint16_t diff = gb->cpu.a - rhs - ci;
	bool half_carry = (gb->cpu.a & 0x0F) < (rhs & 0x0F) + ci;
	bool co = diff > 0xFF;
	gb->cpu.a = (uint8_t)diff;
	gb__SetFlags(gb, gb->cpu.a == 0, true, half_carry, co);
}

static uint16_t
gb__Add16(gb_GameBoy *gb, uint16_t lhs, uint16_t rhs)
{
	uint32_t sum = lhs + rhs;
	bool half_carry = (lhs & 0x0FFF) + (rhs & 0x0FFF) > 0x0FFF;
	bool co = sum > 0xFFFF;
	gb__SetFlags(gb, gb->cpu.flags.zero == 1, false, half_carry, co);
	return (uint16_t)sum;
}

static void
gb__And(gb_GameBoy *gb, uint8_t rhs)
{
	gb->cpu.a &= rhs;
	gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
}

static void
gb__Xor(gb_GameBoy *gb, uint8_t rhs)
{
	gb->cpu.a ^= rhs;
	gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
}

static void
gb__Or(gb_GameBoy *gb, uint8_t rhs)
{
	gb->cpu.a |= rhs;
	gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
}

static void
gb__Cp(gb_GameBoy *gb, uint8_t rhs)
{
	gb__SetFlags(gb, gb->cpu.a == rhs, true, (gb->cpu.a & 0x0F) < (rhs & 0x0F), gb->cpu.a < rhs);
}

static void
gb__Inc(gb_GameBoy *gb, uint8_t *val)
{
	++(*val);
	gb__SetFlags(gb, (*val) == 0, false, ((*val) & 0x0F) == 0, gb->cpu.flags.carry == 1);
}

static void
gb__Dec(gb_GameBoy *gb, uint8_t *val)
{
	--(*val);
	gb__SetFlags(gb, (*val) == 0, true, ((*val) & 0x0F) == 0x0F, gb->cpu.flags.carry == 1);
}

static uint16_t
gb__ExecuteBasicInstruction(gb_GameBoy *gb, gb_Instruction inst)
{
	assert(gb_MemoryReadByte(gb, gb->cpu.pc - gb_InstructionSize(inst)) != extended_inst_prefix ||
			gb_MemoryReadByte(gb, gb->cpu.pc - gb_InstructionSize(inst)) - 1 == extended_inst_prefix);

	bool branch = false;

	switch (inst.opcode)
	{
	case 0x00:  // NOP
		break;
	case 0x01:  // LD BC, u16
		gb->cpu.bc = inst.operand_word;
		break;
	case 0x02:  // LD (BC), A
		gb__MemoryWriteByte(gb, gb->cpu.bc, gb->cpu.a);
		break;
	case 0x03:  // INC BC
		++gb->cpu.bc;
		break;
	case 0x04:  // INC B
		gb__Inc(gb, &gb->cpu.b);
		break;
	case 0x05:  // DEC B
		gb__Dec(gb, &gb->cpu.b);
		break;
	case 0x06:  // LD B, u8
		gb->cpu.b = inst.operand_byte;
		break;
	case 0x07:  // RLCA
		gb->cpu.a = gb__RotateLeftCircular(gb, gb->cpu.a, true);
		break;

	case 0x08:  // LD (u16), SP
		gb__MemoryWriteWord(gb, inst.operand_word, gb->cpu.sp);
		break;
	case 0x09:  // ADD HL, BC
		gb->cpu.hl = gb__Add16(gb, gb->cpu.hl, gb->cpu.bc);
		break;
	case 0x0A:  // LD A, (BC)
		gb->cpu.a = gb_MemoryReadByte(gb, gb->cpu.bc);
		break;
	case 0x0B:  // DEC BC
		--gb->cpu.bc;
		break;
	case 0x0C:  // INC C
		gb__Inc(gb, &gb->cpu.c);
		break;
	case 0x0D:  // DEC C
		gb__Dec(gb, &gb->cpu.c);
		break;
	case 0x0E:  // LD C, u8
		gb->cpu.c = inst.operand_byte;
		break;
	case 0x0F:  // RRCA
		gb->cpu.a = gb__RotateRightCircular(gb, gb->cpu.a, true);
		break;

	case 0x10:  // STOP
		gb->timer.div = 0;
		gb->cpu.stop = true;
		// TODO(stefalie): not implemented. Supposedly no licensed DMG game ever used it.
		break;
	case 0x11:  // LD DE, u16
		gb->cpu.de = inst.operand_word;
		break;
	case 0x12:  // LD (DE), A
		gb__MemoryWriteByte(gb, gb->cpu.de, gb->cpu.a);
		break;
	case 0x13:  // INC DE
		++gb->cpu.de;
		break;
	case 0x14:  // INC D
		gb__Inc(gb, &gb->cpu.d);
		break;
	case 0x15:  // DEC D
		gb__Dec(gb, &gb->cpu.d);
		break;
	case 0x16:  // LD D, u8
		gb->cpu.d = inst.operand_byte;
		break;
	case 0x17:  // RLA
		gb->cpu.a = gb__RotateLeft(gb, gb->cpu.a, true);
		break;

	case 0x18:  // JR i8
		gb->cpu.pc += (int8_t)inst.operand_byte;
		break;
	case 0x19:  // ADD HL, DE
		gb->cpu.hl = gb__Add16(gb, gb->cpu.hl, gb->cpu.de);
		break;
	case 0x1A:  // LD A, (DE)
		gb->cpu.a = gb_MemoryReadByte(gb, gb->cpu.de);
		break;
	case 0x1B:  // DEC DE
		--gb->cpu.de;
		break;
	case 0x1C:  // INC E
		gb__Inc(gb, &gb->cpu.e);
		break;
	case 0x1D:  // DEC E
		gb__Dec(gb, &gb->cpu.e);
		break;
	case 0x1E:  // LD E, u8
		gb->cpu.e = inst.operand_byte;
		break;
	case 0x1F:  // RRA
		gb->cpu.a = gb__RotateRight(gb, gb->cpu.a, true);
		break;

	case 0x20:  // JR NZ, i8
		if (gb->cpu.flags.zero == 0)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			branch = true;
		}
		break;
	case 0x21:  // LD HL, u16
		gb->cpu.hl = inst.operand_word;
		break;
	case 0x22:  // LD (HL+), A
		gb__MemoryWriteByte(gb, gb->cpu.hl, gb->cpu.a);
		++gb->cpu.hl;
		break;
	case 0x23:  // INC HL
		++gb->cpu.hl;
		break;
	case 0x24:  // INC H
		gb__Inc(gb, &gb->cpu.h);
		break;
	case 0x25:  // DEC H
		gb__Dec(gb, &gb->cpu.h);
		break;
	case 0x26:  // LD H, u8
		gb->cpu.h = inst.operand_byte;
		break;
	case 0x27:  // DAA
	{
		// See:
		// - https://forums.nesdev.org/viewtopic.php?t=15944
		// - https://ehaskins.com/2018-01-30%20Z80%20DAA
		bool co = gb->cpu.flags.carry == 1;
		if (gb->cpu.flags.subtract == 0)
		{  // After an addition, adjust if (half-)carry occurred or if result is out of bounds.
			if (gb->cpu.flags.carry == 1 || gb->cpu.a > 0x99)
			{
				gb->cpu.a += 0x60;
				co = true;
			}
			if (gb->cpu.flags.half_carry == 1 || (gb->cpu.a & 0x0F) > 0x09)
			{
				gb->cpu.a += 0x6;
			}
		}
		else
		{  // After a subtraction, only adjust if (half-)carry occurred.
			if (gb->cpu.flags.carry == 1)
			{
				gb->cpu.a -= 0x60;
			}
			if (gb->cpu.flags.half_carry == 1)
			{
				gb->cpu.a -= 0x6;
			}
		}
		gb__SetFlags(gb, gb->cpu.a == 0, gb->cpu.flags.subtract == 1, false, co);
		break;
	}

	case 0x28:  // JR Z, i8
		if (gb->cpu.flags.zero == 1)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			branch = true;
		}
		break;
	case 0x29:  // ADD HL, HL
		gb->cpu.hl = gb__Add16(gb, gb->cpu.hl, gb->cpu.hl);
		break;
	case 0x2A:  // LD A, (HL+)
		gb->cpu.a = gb_MemoryReadByte(gb, gb->cpu.hl);
		++gb->cpu.hl;
		break;
	case 0x2B:  // DEC HL
		--gb->cpu.hl;
		break;
	case 0x2C:  // INC L
		gb__Inc(gb, &gb->cpu.l);
		break;
	case 0x2D:  // DEC L
		gb__Dec(gb, &gb->cpu.l);
		break;
	case 0x2E:  // LD L, u8
		gb->cpu.l = inst.operand_byte;
		break;
	case 0x2F:  // CPL
		gb->cpu.a = ~gb->cpu.a;
		gb__SetFlags(gb, gb->cpu.flags.zero == 1, true, true, gb->cpu.flags.carry == 1);
		break;

	case 0x30:  // JR NC, i8
		if (gb->cpu.flags.carry == 0)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			branch = true;
		}
		break;
	case 0x31:  // LD SP, u16
		gb->cpu.sp = inst.operand_word;
		break;
	case 0x32:  // LD (HL-), A
		gb__MemoryWriteByte(gb, gb->cpu.hl, gb->cpu.a);
		--gb->cpu.hl;
		break;
	case 0x33:  // INC SP
		++gb->cpu.sp;
		break;
	case 0x34:  // INC (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__Inc(gb, &val);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}
	case 0x35:  // DEC (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__Dec(gb, &val);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}
	case 0x36:  // LD (HL), u8
		gb__MemoryWriteByte(gb, gb->cpu.hl, inst.operand_byte);
		break;
	case 0x37:  // SCF
		gb__SetFlags(gb, gb->cpu.flags.zero == 1, false, false, true);
		break;

	case 0x38:  // JR C, i8
		if (gb->cpu.flags.carry == 1)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			branch = true;
		}
		break;
	case 0x39:  // ADD HL, SP
		gb->cpu.hl = gb__Add16(gb, gb->cpu.hl, gb->cpu.sp);
		break;
	case 0x3A:  // LD A, (HL-)
		gb->cpu.a = gb_MemoryReadByte(gb, gb->cpu.hl);
		--gb->cpu.hl;
		break;
	case 0x3B:  // DEC SP
		--gb->cpu.sp;
		break;
	case 0x3C:  // INC A
		gb__Inc(gb, &gb->cpu.a);
		break;
	case 0x3D:  // DEC A
		gb__Dec(gb, &gb->cpu.a);
		break;
	case 0x3E:  // LD A, u8
		gb->cpu.a = inst.operand_byte;
		break;
	case 0x3F:  // CCF
		gb__SetFlags(gb, gb->cpu.flags.zero == 1, false, false, gb->cpu.flags.carry == 0);
		break;

	case 0x40:  // LD B, B
	case 0x41:  // LD B, C
	case 0x42:  // LD B, D
	case 0x43:  // LD B, E
	case 0x44:  // LD B, H
	case 0x45:  // LD B, L
	case 0x47:  // LD B, A
	case 0x48:  // LD C, B
	case 0x49:  // LD C, C
	case 0x4A:  // LD C, D
	case 0x4B:  // LD C, E
	case 0x4C:  // LD C, H
	case 0x4D:  // LD C, L
	case 0x4F:  // LD C, A
	case 0x50:  // LD D, B
	case 0x51:  // LD D, C
	case 0x52:  // LD D, D
	case 0x53:  // LD D, E
	case 0x54:  // LD D, H
	case 0x55:  // LD D, L
	case 0x57:  // LD D, A
	case 0x58:  // LD E, B
	case 0x59:  // LD E, C
	case 0x5A:  // LD E, D
	case 0x5B:  // LD E, E
	case 0x5C:  // LD E, H
	case 0x5D:  // LD E, L
	case 0x5F:  // LD E, A
	case 0x60:  // LD H, B
	case 0x61:  // LD H, C
	case 0x62:  // LD H, D
	case 0x63:  // LD H, E
	case 0x64:  // LD H, H
	case 0x65:  // LD H, L
	case 0x67:  // LD H, A
	case 0x68:  // LD L, B
	case 0x69:  // LD L, C
	case 0x6A:  // LD L, D
	case 0x6B:  // LD L, E
	case 0x6C:  // LD L, H
	case 0x6D:  // LD L, L
	case 0x6F:  // LD L, A
	case 0x78:  // LD A, B
	case 0x79:  // LD A, C
	case 0x7A:  // LD A, D
	case 0x7B:  // LD A, E
	case 0x7C:  // LD A, H
	case 0x7D:  // LD A, L
	case 0x7F:  // LD A, A
		*gb__MapIndexToReg(gb, (inst.opcode - 0x40) >> 3u) = *gb__MapIndexToReg(gb, inst.opcode);
		break;

	case 0x46:  // LD B, (HL)
	case 0x4E:  // LD C, (HL)
	case 0x56:  // LD D, (HL)
	case 0x5E:  // LD E, (HL)
	case 0x66:  // LD H, (HL)
	case 0x6E:  // LD L, (HL)
	case 0x7E:  // LD A, (HL)
		*gb__MapIndexToReg(gb, (inst.opcode - 0x40) >> 3u) = gb_MemoryReadByte(gb, gb->cpu.hl);
		break;

	case 0x70:  // LD (HL), B
	case 0x71:  // LD (HL), C
	case 0x72:  // LD (HL), D
	case 0x73:  // LD (HL), E
	case 0x74:  // LD (HL), H
	case 0x75:  // LD (HL), L
	case 0x77:  // LD (HL), A
		gb__MemoryWriteByte(gb, gb->cpu.hl, *gb__MapIndexToReg(gb, inst.opcode));
		break;

	case 0x76:  // HALT
		const bool intr_pending = gb->cpu.interrupt.ie_flags.reg & gb->cpu.interrupt.if_flags.reg;
		if (!gb->cpu.interrupt.ime && intr_pending)
		{
			// TODO(stefalie): HALT bug
			// This causes to skip the increase of PC after the next time that PC is read.
			// If the next instruction is a single byte, it will simply duplicate that instruction.
			// If the next instruction has multiple bytes, it will be messed up completely.
			// To implement this here, we would change the following:
			// - Add a 'halt_bug' Boolean flag that is set to 'true'.
			// - In 'gb_FetchInstruction', inside the branch that reads the first operand byte
			//   if 'halt_bug == true' don't increase 'addr' and set 'halt_bug = false'.
			// - After the call to  'gb_FetchInstruction', skip the increase of PC if 'halt_bug == true'
			//   (and assert that it was a single byte instruction in that case).
			//
			// I think, we can still set 'halt = true' here. Interrupt handling will unset it again.
			//
			// There are two exceptions to that which behave differently.
			// See: https://gbdev.io/pandocs/halt.html#halt-bug
			assert(!"The HALT bug is not implemented.");
		}
		else
		{
			gb->cpu.halt = true;
		}
		break;
	case 0x80:  // ADD A, B
	case 0x81:  // ADD A, C
	case 0x82:  // ADD A, D
	case 0x83:  // ADD A, E
	case 0x84:  // ADD A, H
	case 0x85:  // ADD A, L
	case 0x87:  // ADD A, A
		gb__Add(gb, *gb__MapIndexToReg(gb, inst.opcode), false);
		break;
	case 0x86:  // ADD A, (HL)
		gb__Add(gb, gb_MemoryReadByte(gb, gb->cpu.hl), false);
		break;

	case 0x88:  // ADC A, B
	case 0x89:  // ADC A, C
	case 0x8A:  // ADC A, D
	case 0x8B:  // ADC A, E
	case 0x8C:  // ADC A, H
	case 0x8D:  // ADC A, L
	case 0x8F:  // ADC A, A
		gb__Add(gb, *gb__MapIndexToReg(gb, inst.opcode), true);
		break;
	case 0x8E:  // ADC A, (HL)
		gb__Add(gb, gb_MemoryReadByte(gb, gb->cpu.hl), true);
		break;

	case 0x90:  // SUB A, B
	case 0x91:  // SUB A, C
	case 0x92:  // SUB A, D
	case 0x93:  // SUB A, E
	case 0x94:  // SUB A, H
	case 0x95:  // SUB A, L
	case 0x97:  // SUB A, A
		gb__Sub(gb, *gb__MapIndexToReg(gb, inst.opcode), false);
		break;
	case 0x96:  // SUB A, (HL)
		gb__Sub(gb, gb_MemoryReadByte(gb, gb->cpu.hl), false);
		break;

	case 0x98:  // SBC A, B
	case 0x99:  // SBC A, C
	case 0x9A:  // SBC A, D
	case 0x9B:  // SBC A, E
	case 0x9C:  // SBC A, H
	case 0x9D:  // SBC A, L
	case 0x9F:  // SBC A, A
		gb__Sub(gb, *gb__MapIndexToReg(gb, inst.opcode), true);
		break;
	case 0x9E:  // SBC A, (HL)
		gb__Sub(gb, gb_MemoryReadByte(gb, gb->cpu.hl), true);
		break;

	case 0xA0:  // AND A, B
	case 0xA1:  // AND A, C
	case 0xA2:  // AND A, D
	case 0xA3:  // AND A, E
	case 0xA4:  // AND A, H
	case 0xA5:  // AND A, L
	case 0xA7:  // AND A, A
		gb__And(gb, *gb__MapIndexToReg(gb, inst.opcode));
		break;
	case 0xA6:  // AND A, (HL)
		gb__And(gb, gb_MemoryReadByte(gb, gb->cpu.hl));
		break;

	case 0xA8:  // XOR A, B
	case 0xA9:  // XOR A, C
	case 0xAA:  // XOR A, D
	case 0xAB:  // XOR A, E
	case 0xAC:  // XOR A, H
	case 0xAD:  // XOR A, L
	case 0xAF:  // XOR A, A
		gb__Xor(gb, *gb__MapIndexToReg(gb, inst.opcode));
		break;
	case 0xAE:  // XOR A, (HL)
		gb__Xor(gb, gb_MemoryReadByte(gb, gb->cpu.hl));
		break;

	case 0xB0:  // OR A, B
	case 0xB1:  // OR A, C
	case 0xB2:  // OR A, D
	case 0xB3:  // OR A, E
	case 0xB4:  // OR A, H
	case 0xB5:  // OR A, L
	case 0xB7:  // OR A, A
		gb__Or(gb, *gb__MapIndexToReg(gb, inst.opcode));
		break;
	case 0xB6:  // OR A, (HL)
		gb__Or(gb, gb_MemoryReadByte(gb, gb->cpu.hl));
		break;

	case 0xB8:  // CP A, B
	case 0xB9:  // CP A, C
	case 0xBA:  // CP A, D
	case 0xBB:  // CP A, E
	case 0xBC:  // CP A, H
	case 0xBD:  // CP A, L
	case 0xBF:  // CP A, A
		gb__Cp(gb, *gb__MapIndexToReg(gb, inst.opcode));
		break;
	case 0xBE:  // CP A, (HL)
		gb__Cp(gb, gb_MemoryReadByte(gb, gb->cpu.hl));
		break;

	case 0xC0:  // RET NZ
		if (gb->cpu.flags.zero == 0)
		{
			gb->cpu.pc = gb__PopWordToStack(gb);
			branch = true;
		}
		break;
	case 0xC1:  // POP BC
		gb->cpu.bc = gb__PopWordToStack(gb);
		break;
	case 0xC2:  // JP NZ, u16
		if (gb->cpu.flags.zero == 0)
		{
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xC3:  // JP u16
		gb->cpu.pc = inst.operand_word;
		break;
	case 0xC4:  // CALL NZ, u16
		if (gb->cpu.flags.zero == 0)
		{
			gb__PushWordToStack(gb, gb->cpu.pc);
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xC5:  // PUSH BC
		gb__PushWordToStack(gb, gb->cpu.bc);
		break;
	case 0xC6:  // ADD A, u8
		gb__Add(gb, inst.operand_byte, false);
		break;
	case 0xC7:  // RST 00h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x00;
		break;

	case 0xC8:  // RET Z
		if (gb->cpu.flags.zero == 1)
		{
			gb->cpu.pc = gb__PopWordToStack(gb);
			branch = true;
		}
		break;
	case 0xC9:  // RET
		gb->cpu.pc = gb__PopWordToStack(gb);
		break;
	case 0xCA:  // JP Z, u16
		if (gb->cpu.flags.zero == 1)
		{
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xCB:  // PREFIX
		// Handled seperatly above.
		assert(false);
		break;
	case 0xCC:  // CALL Z, u16
		if (gb->cpu.flags.zero == 1)
		{
			gb__PushWordToStack(gb, gb->cpu.pc);
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xCD:  // CALL u16
	{
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = inst.operand_word;
		break;
	}
	case 0xCE:  // ADC A, u8
		gb__Add(gb, inst.operand_byte, true);
		break;
	case 0xCF:  // RST 08h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x08;
		break;

	case 0xD0:  // RET NC
		if (gb->cpu.flags.carry == 0)
		{
			gb->cpu.pc = gb__PopWordToStack(gb);
			branch = true;
		}
		break;
	case 0xD1:  // POP DE
		gb->cpu.de = gb__PopWordToStack(gb);
		break;
	case 0xD2:  // JP NC, u16
		if (gb->cpu.flags.carry == 0)
		{
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xD4:  // CALL NC, u16
		if (gb->cpu.flags.carry == 0)
		{
			gb__PushWordToStack(gb, gb->cpu.pc);
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xD5:  // PUSH DE
		gb__PushWordToStack(gb, gb->cpu.de);
		break;
	case 0xD6:  // SUB A, u8
		gb__Sub(gb, inst.operand_byte, false);
		break;
	case 0xD7:  // RST 10h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x10;
		break;

	case 0xD8:  // RET C
		if (gb->cpu.flags.carry == 1)
		{
			gb->cpu.pc = gb__PopWordToStack(gb);
			branch = true;
		}
		break;
	case 0xD9:  // RETI
		gb->cpu.pc = gb__PopWordToStack(gb);
		gb->cpu.interrupt.ime = true;
		break;
	case 0xDA:  // JP C, u16
		if (gb->cpu.flags.carry == 1)
		{
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xDC:  // CALL C, u16
		if (gb->cpu.flags.carry == 1)
		{
			gb__PushWordToStack(gb, gb->cpu.pc);
			gb->cpu.pc = inst.operand_word;
			branch = true;
		}
		break;
	case 0xDE:  // SBC A, u8
		gb__Sub(gb, inst.operand_byte, true);
		break;
	case 0xDF:  // RST 18h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x18;
		break;

	case 0xE0:  // LD (FF00+u8), A
		gb__MemoryWriteByte(gb, 0xFF00 + inst.operand_byte, gb->cpu.a);
		break;
	case 0xE1:  // POP HL
		gb->cpu.hl = gb__PopWordToStack(gb);
		break;
	case 0xE2:  // LD (FF00+C), A
		gb__MemoryWriteByte(gb, 0xFF00 + gb->cpu.c, gb->cpu.a);
		break;
	case 0xE5:  // PUSH HL
		gb__PushWordToStack(gb, gb->cpu.hl);
		break;
	case 0xE6:  // AND A, u8
		gb__And(gb, inst.operand_byte);
		break;
	case 0xE7:  // RST 20h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x20;
		break;

	case 0xE8:  // ADD SP, i8
	{
		// See: https://stackoverflow.com/a/57981912
		int8_t operand = (int8_t)inst.operand_byte;
		uint16_t sum = gb->cpu.sp + operand;
		bool half_carry = (sum & 0x0F) < (gb->cpu.sp & 0x0F);
		bool co = (sum & 0xFF) < (gb->cpu.sp & 0xFF);
		gb->cpu.sp = sum;
		gb__SetFlags(gb, false, false, half_carry, co);
		break;
	}
	case 0xE9:  // JP HL
		gb->cpu.pc = gb->cpu.hl;
		break;
	case 0xEA:  // LD (u16), A
		gb__MemoryWriteByte(gb, inst.operand_word, gb->cpu.a);
		break;
	case 0xEE:  // XOR A, u8
		gb__Xor(gb, inst.operand_byte);
		break;
	case 0xEF:  // RST 28h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x28;
		break;

	case 0xF0:  // LD A, (FF00+u8)
		gb->cpu.a = gb_MemoryReadByte(gb, 0xFF00 + inst.operand_byte);
		break;
	case 0xF1:  // POP AF
		gb->cpu.af = gb__PopWordToStack(gb);
		// Unclear if necessary. Those bits are initialized to 0 and should never be written anyway.
		gb->cpu.flags._ = 0;
		break;
	case 0xF2:  // LD A, (FF00+C)
		gb->cpu.a = gb_MemoryReadByte(gb, 0xFF00 + gb->cpu.c);
		break;
	case 0xF3:  // DI
		gb->cpu.interrupt.ime = false;
		gb->cpu.interrupt.ime_after_next_inst = false;
		break;
	case 0xF5:  // PUSH AF
		gb__PushWordToStack(gb, gb->cpu.af);
		break;
	case 0xF6:  // OR A, u8
		gb__Or(gb, inst.operand_byte);
		break;
	case 0xF7:  // RST 30h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x30;
		break;

	case 0xF8:  // LD HL, SP+i8
	{
		// See: https://stackoverflow.com/a/57981912
		int8_t operand = (int8_t)inst.operand_byte;
		uint16_t sum = gb->cpu.sp + operand;
		bool half_carry = (sum & 0x0F) < (gb->cpu.sp & 0x0F);
		bool co = (sum & 0xFF) < (gb->cpu.sp & 0xFF);
		gb->cpu.hl = sum;
		gb__SetFlags(gb, false, false, half_carry, co);
		break;
	}
	case 0xF9:  // LD SP, HL
		gb->cpu.sp = gb->cpu.hl;
		break;
	case 0xFA:  // LD A, (u16)
		gb->cpu.a = gb_MemoryReadByte(gb, inst.operand_word);
		break;
	case 0xFB:  // EI
		gb->cpu.interrupt.ime_after_next_inst = true;
		break;
	case 0xFE:  // CP A, u8
		gb__Cp(gb, inst.operand_byte);
		break;
	case 0xFF:  // RST 38h
		gb__PushWordToStack(gb, gb->cpu.pc);
		gb->cpu.pc = 0x38;
		break;

	case 0xD3:
	case 0xDB:
	case 0xDD:
	case 0xE3:
	case 0xE4:
	case 0xEB:
	case 0xEC:
	case 0xED:
	case 0xF4:
	case 0xFC:
	case 0xFD:
		// Undefined opcodes
		assert(false);
		break;

	default:
		assert(false);
		break;
	}

	gb__InstructionInfo info = gb__basic_instruction_infos[inst.opcode];
	uint16_t result = branch ? info.num_machine_cycles_with_branch : info.num_machine_cycles_wo_branch;
	return result;
}

static uint16_t
gb__ExecuteExtendedInstruction(gb_GameBoy *gb, gb_Instruction inst)
{
	assert(gb_MemoryReadByte(gb, gb->cpu.pc - gb_InstructionSize(inst)) == extended_inst_prefix);

	// TODO(stefalie): Consider using if/else if branches. It will be more compact than using a switch.
	switch (inst.opcode)
	{
	case 0x00:  // RLC B
	case 0x01:  // RLC C
	case 0x02:  // RLC D
	case 0x03:  // RLC E
	case 0x04:  // RLC H
	case 0x05:  // RLC L
	case 0x07:  // RLC A
	{
		uint8_t *reg = gb__MapIndexToReg(gb, inst.opcode);
		*reg = gb__RotateLeftCircular(gb, *reg, false);
		break;
	}
	case 0x06:  // RLC (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		val = gb__RotateLeftCircular(gb, val, false);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}
	case 0x08:  // RRC B
	case 0x09:  // RRC C
	case 0x0A:  // RRC D
	case 0x0B:  // RRC E
	case 0x0C:  // RRC H
	case 0x0D:  // RRC L
	case 0x0F:  // RRC A
	{
		uint8_t *reg = gb__MapIndexToReg(gb, inst.opcode);
		*reg = gb__RotateRightCircular(gb, *reg, false);
		break;
	}
	case 0x0E:  // RRC (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		val = gb__RotateRightCircular(gb, val, false);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0x10:  // RL B
	case 0x11:  // RL C
	case 0x12:  // RL D
	case 0x13:  // RL E
	case 0x14:  // RL H
	case 0x15:  // RL L
	case 0x17:  // RL A
	{
		uint8_t *reg = gb__MapIndexToReg(gb, inst.opcode);
		*reg = gb__RotateLeft(gb, *reg, false);
		break;
	}
	case 0x16:  // RL (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		val = gb__RotateLeft(gb, val, false);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0x18:  // RR B
	case 0x19:  // RR C
	case 0x1A:  // RR D
	case 0x1B:  // RR E
	case 0x1C:  // RR H
	case 0x1D:  // RR L
	case 0x1F:  // RR A
	{
		uint8_t *reg = gb__MapIndexToReg(gb, inst.opcode);
		*reg = gb__RotateRight(gb, *reg, false);
		break;
	}
	case 0x1E:  // RR (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		val = gb__RotateRight(gb, val, false);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0x20:  // SLA B
	case 0x21:  // SLA C
	case 0x22:  // SLA D
	case 0x23:  // SLA E
	case 0x24:  // SLA H
	case 0x25:  // SLA L
	case 0x27:  // SLA A
	{
		uint8_t *reg = gb__MapIndexToReg(gb, inst.opcode);
		const bool co = *reg & 0x80;
		*reg <<= 1u;
		gb__SetFlags(gb, *reg == 0, false, false, co);
		break;
	}
	case 0x26:  // SLA (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		const bool co = val & 0x80;
		val <<= 1u;
		gb__SetFlags(gb, val == 0, false, false, co);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0x28:  // SRA B
	case 0x29:  // SRA C
	case 0x2A:  // SRA D
	case 0x2B:  // SRA E
	case 0x2C:  // SRA H
	case 0x2D:  // SRA L
	case 0x2F:  // SRA A
	{
		int8_t *reg = (int8_t *)gb__MapIndexToReg(gb, inst.opcode);
		const bool co = *reg & 0x01;
		*reg >>= 1u;
		gb__SetFlags(gb, *reg == 0, false, false, co);
		break;
	}
	case 0x2E:  // SRA (HL)
	{
		int8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		const bool co = val & 0x01;
		val >>= 1u;
		gb__SetFlags(gb, val == 0, false, false, co);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0x30:  // SWAP B
	case 0x31:  // SWAP C
	case 0x32:  // SWAP D
	case 0x33:  // SWAP E
	case 0x34:  // SWAP H
	case 0x35:  // SWAP L
	case 0x37:  // SWAP A
	{
		uint8_t *reg = gb__MapIndexToReg(gb, inst.opcode);
		*reg = (*reg >> 4u) | (*reg << 4u);
		gb__SetFlags(gb, *reg == 0, false, false, false);
		break;
	}
	case 0x36:  // SWAP (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		val = (val >> 4u) | (val << 4u);
		gb__SetFlags(gb, val == 0, false, false, false);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0x38:  // SRL B
	case 0x39:  // SRL C
	case 0x3A:  // SRL D
	case 0x3B:  // SRL E
	case 0x3C:  // SRL H
	case 0x3D:  // SRL L
	case 0x3F:  // SRL A
	{
		uint8_t *reg = gb__MapIndexToReg(gb, inst.opcode);
		const bool co = *reg & 0x01;
		*reg >>= 1u;
		gb__SetFlags(gb, *reg == 0, false, false, co);
		break;
	}
	case 0x3E:  // SRL (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		const bool co = val & 0x01;
		val >>= 1u;
		gb__SetFlags(gb, val == 0, false, false, co);
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0x40:  // BIT 0, B
	case 0x41:  // BIT 0, C
	case 0x42:  // BIT 0, D
	case 0x43:  // BIT 0, E
	case 0x44:  // BIT 0, H
	case 0x45:  // BIT 0, L
	case 0x47:  // BIT 0, A

	case 0x48:  // BIT 1, B
	case 0x49:  // BIT 1, C
	case 0x4A:  // BIT 1, D
	case 0x4B:  // BIT 1, E
	case 0x4C:  // BIT 1, H
	case 0x4D:  // BIT 1, L
	case 0x4F:  // BIT 1, A

	case 0x50:  // BIT 2, B
	case 0x51:  // BIT 2, C
	case 0x52:  // BIT 2, D
	case 0x53:  // BIT 2, E
	case 0x54:  // BIT 2, H
	case 0x55:  // BIT 2, L
	case 0x57:  // BIT 2, A

	case 0x58:  // BIT 3, B
	case 0x59:  // BIT 3, C
	case 0x5A:  // BIT 3, D
	case 0x5B:  // BIT 3, E
	case 0x5C:  // BIT 3, H
	case 0x5D:  // BIT 3, L
	case 0x5F:  // BIT 3, A

	case 0x60:  // BIT 4, B
	case 0x61:  // BIT 4, C
	case 0x62:  // BIT 4, D
	case 0x63:  // BIT 4, E
	case 0x64:  // BIT 4, H
	case 0x65:  // BIT 4, L
	case 0x67:  // BIT 4, A

	case 0x68:  // BIT 5, B
	case 0x69:  // BIT 5, C
	case 0x6A:  // BIT 5, D
	case 0x6B:  // BIT 5, E
	case 0x6C:  // BIT 5, H
	case 0x6D:  // BIT 5, L
	case 0x6F:  // BIT 5, A

	case 0x70:  // BIT 6, B
	case 0x71:  // BIT 6, C
	case 0x72:  // BIT 6, D
	case 0x73:  // BIT 6, E
	case 0x74:  // BIT 6, H
	case 0x75:  // BIT 6, L
	case 0x77:  // BIT 6, A

	case 0x78:  // BIT 7, B
	case 0x79:  // BIT 7, C
	case 0x7A:  // BIT 7, D
	case 0x7B:  // BIT 7, E
	case 0x7C:  // BIT 7, H
	case 0x7D:  // BIT 7, L
	case 0x7F:  // BIT 7, A
	{
		size_t bit_index = (inst.opcode - 0x40) >> 3u;
		size_t bit = (*gb__MapIndexToReg(gb, inst.opcode) >> bit_index) & 0x01;
		gb__SetFlags(gb, bit == 0, false, true, gb->cpu.flags.carry == 1);
		break;
	}

	case 0x46:  // BIT 0, (HL)
	case 0x4E:  // BIT 1, (HL)
	case 0x56:  // BIT 2, (HL)
	case 0x5E:  // BIT 3, (HL)
	case 0x66:  // BIT 4, (HL)
	case 0x6E:  // BIT 5, (HL)
	case 0x76:  // BIT 6, (HL)
	case 0x7E:  // BIT 7, (HL)
	{
		size_t bit_index = (inst.opcode - 0x40) >> 3u;
		size_t bit = (gb_MemoryReadByte(gb, gb->cpu.hl) >> bit_index) & 0x01;
		gb__SetFlags(gb, bit == 0, false, true, gb->cpu.flags.carry == 1);
		break;
	}

	case 0x80:  // RES 0, B
	case 0x81:  // RES 0, C
	case 0x82:  // RES 0, D
	case 0x83:  // RES 0, E
	case 0x84:  // RES 0, H
	case 0x85:  // RES 0, L
	case 0x87:  // RES 0, A

	case 0x88:  // RES 1, B
	case 0x89:  // RES 1, C
	case 0x8A:  // RES 1, D
	case 0x8B:  // RES 1, E
	case 0x8C:  // RES 1, H
	case 0x8D:  // RES 1, L
	case 0x8F:  // RES 1, A

	case 0x90:  // RES 2, B
	case 0x91:  // RES 2, C
	case 0x92:  // RES 2, D
	case 0x93:  // RES 2, E
	case 0x94:  // RES 2, H
	case 0x95:  // RES 2, L
	case 0x97:  // RES 2, A

	case 0x98:  // RES 3, B
	case 0x99:  // RES 3, C
	case 0x9A:  // RES 3, D
	case 0x9B:  // RES 3, E
	case 0x9C:  // RES 3, H
	case 0x9D:  // RES 3, L
	case 0x9F:  // RES 3, A

	case 0xA0:  // RES 4, B
	case 0xA1:  // RES 4, C
	case 0xA2:  // RES 4, D
	case 0xA3:  // RES 4, E
	case 0xA4:  // RES 4, H
	case 0xA5:  // RES 4, L
	case 0xA7:  // RES 4, A

	case 0xA8:  // RES 5, B
	case 0xA9:  // RES 5, C
	case 0xAA:  // RES 5, D
	case 0xAB:  // RES 5, E
	case 0xAC:  // RES 5, H
	case 0xAD:  // RES 5, L
	case 0xAF:  // RES 5, A

	case 0xB0:  // RES 6, B
	case 0xB1:  // RES 6, C
	case 0xB2:  // RES 6, D
	case 0xB3:  // RES 6, E
	case 0xB4:  // RES 6, H
	case 0xB5:  // RES 6, L
	case 0xB7:  // RES 6, A

	case 0xB8:  // RES 7, B
	case 0xB9:  // RES 7, C
	case 0xBA:  // RES 7, D
	case 0xBB:  // RES 7, E
	case 0xBC:  // RES 7, H
	case 0xBD:  // RES 7, L
	case 0xBF:  // RES 7, A
	{
		const size_t bit_index = (inst.opcode - 0x80) >> 3u;
		const uint8_t mask = ~(1u << bit_index);
		*gb__MapIndexToReg(gb, inst.opcode) &= mask;
		break;
	}

	case 0x86:  // RES 0, (HL)
	case 0x8E:  // RES 1, (HL)
	case 0x96:  // RES 2, (HL)
	case 0x9E:  // RES 3, (HL)
	case 0xA6:  // RES 4, (HL)
	case 0xAE:  // RES 5, (HL)
	case 0xB6:  // RES 6, (HL)
	case 0xBE:  // RES 7, (HL)
	{
		const size_t bit_index = (inst.opcode - 0x80) >> 3u;
		const uint8_t mask = ~(1u << bit_index);
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		val &= mask;
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	case 0xC0:  // SET 0, B
	case 0xC1:  // SET 0, C
	case 0xC2:  // SET 0, D
	case 0xC3:  // SET 0, E
	case 0xC4:  // SET 0, H
	case 0xC5:  // SET 0, L
	case 0xC7:  // SET 0, A

	case 0xC8:  // SET 1, B
	case 0xC9:  // SET 1, C
	case 0xCA:  // SET 1, D
	case 0xCB:  // SET 1, E
	case 0xCC:  // SET 1, H
	case 0xCD:  // SET 1, L
	case 0xCF:  // SET 1, A

	case 0xD0:  // SET 2, B
	case 0xD1:  // SET 2, C
	case 0xD2:  // SET 2, D
	case 0xD3:  // SET 2, E
	case 0xD4:  // SET 2, H
	case 0xD5:  // SET 2, L
	case 0xD7:  // SET 2, A

	case 0xD8:  // SET 3, B
	case 0xD9:  // SET 3, C
	case 0xDA:  // SET 3, D
	case 0xDB:  // SET 3, E
	case 0xDC:  // SET 3, H
	case 0xDD:  // SET 3, L
	case 0xDF:  // SET 3, A

	case 0xE0:  // SET 4, B
	case 0xE1:  // SET 4, C
	case 0xE2:  // SET 4, D
	case 0xE3:  // SET 4, E
	case 0xE4:  // SET 4, H
	case 0xE5:  // SET 4, L
	case 0xE7:  // SET 4, A

	case 0xE8:  // SET 5, B
	case 0xE9:  // SET 5, C
	case 0xEA:  // SET 5, D
	case 0xEB:  // SET 5, E
	case 0xEC:  // SET 5, H
	case 0xED:  // SET 5, L
	case 0xEF:  // SET 5, A

	case 0xF0:  // SET 6, B
	case 0xF1:  // SET 6, C
	case 0xF2:  // SET 6, D
	case 0xF3:  // SET 6, E
	case 0xF4:  // SET 6, H
	case 0xF5:  // SET 6, L
	case 0xF7:  // SET 6, A

	case 0xF8:  // SET 7, B
	case 0xF9:  // SET 7, C
	case 0xFA:  // SET 7, D
	case 0xFB:  // SET 7, E
	case 0xFC:  // SET 7, H
	case 0xFD:  // SET 7, L
	case 0xFF:  // SET 7, A
	{
		const size_t bit_index = (inst.opcode - 0xC0) >> 3u;
		*gb__MapIndexToReg(gb, inst.opcode) |= 1u << bit_index;
		break;
	}

	case 0xC6:  // SET 0, (HL)
	case 0xCE:  // SET 1, (HL)
	case 0xD6:  // SET 2, (HL)
	case 0xDE:  // SET 3, (HL)
	case 0xE6:  // SET 4, (HL)
	case 0xEE:  // SET 5, (HL)
	case 0xF6:  // SET 6, (HL)
	case 0xFE:  // SET 7, (HL)
	{
		const size_t bit_index = (inst.opcode - 0xC0) >> 3u;
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		val |= 1u << bit_index;
		gb__MemoryWriteByte(gb, gb->cpu.hl, val);
		break;
	}

	default:
		assert(false);
		break;
	}

	gb__InstructionInfo info = gb__extended_instruction_infos[inst.opcode];
	assert(info.num_machine_cycles_with_branch == 0);
	return info.num_machine_cycles_wo_branch;
}

static uint16_t
gb__HandleInterrupts(gb_GameBoy *gb)
{
	uint16_t num_m_cycles = 0;

	struct gb_Interrupt *intr = &gb->cpu.interrupt;

	const union gb_InterruptBits intr_pending = {
		.reg = intr->ie_flags.reg & intr->if_flags.reg,
	};

	if (intr->ime && intr_pending.reg)
	{
		intr->ime = false;
		gb__PushWordToStack(gb, gb->cpu.pc);

		num_m_cycles = 5;
		if (gb->cpu.halt)
		{
			gb->cpu.halt = false;

			// Another 1 m clock is needed if the CPU is halted.
			// See Sec. 4.9 of The Cycle-Accurate Game Boy Docs
			num_m_cycles += 1;
		}

		if (intr_pending.vblank)
		{
			intr->if_flags.vblank = 0;
			gb->cpu.pc = 0x40;
		}
		else if (intr_pending.lcd_stat)
		{
			intr->if_flags.lcd_stat = 0;
			gb->cpu.pc = 0x48;
		}
		else if (intr_pending.timer)
		{
			intr->if_flags.timer = 0;
			gb->cpu.pc = 0x50;
		}
		else if (intr_pending.serial)
		{
			intr->if_flags.serial = 0;
			gb->cpu.pc = 0x58;
		}
		else if (intr_pending.joypad)
		{
			intr->if_flags.joypad = 0;
			gb->cpu.pc = 0x60;
		}
		else
		{
			assert(false);
		}
	}
	else if (gb->cpu.halt && !intr->ime && intr_pending.reg)
	{
		// Exiting halt mode with interrupts disabled.
		// See 3rd to last paragraph in Sec. 4.9 of The Cycle-Accurate Game Boy Docs
		gb->cpu.halt = false;
		num_m_cycles = 1;
	}

	return num_m_cycles;
}

static void
gb__UpdateClockAndTimer(gb_GameBoy *gb, size_t elapsed_m_cycles)
{
	// Only advance if CPU is not stopped.
	// See: https://gbdev.io/pandocs/Timer_and_Divider_Registers.html#ff04--div-divider-register
	// TODO(stefalie): Is this correct? Or does only DIV not advance but the clock keeps running?
	if (!gb->cpu.stop)
	{
		gb->timer.t_clock += (uint16_t)(4 * elapsed_m_cycles);

		// The DIV timer is the higher byte of the 16 bit internal t-clock.
		gb->timer.div = gb__Hi(gb->timer.t_clock);

		if (gb->timer.tac.enable)
		{
			if (gb->timer.reset)
			{
				// If the timer was just activated right now during the current instruction,
				// we assume that it will first have to go through a full period again before
				// increasing the counter.
				gb->timer.remaining_m_cycles = 0;
				gb->timer.reset = false;
			}
			else
			{
				assert(elapsed_m_cycles <= 0xFFFF);
				gb->timer.remaining_m_cycles += (uint16_t)elapsed_m_cycles;
			}

			// The clock of the timer runs at a quarter of the m-clock.
			uint64_t period_in_m_cyles = 0;
			switch (gb->timer.tac.clock_select)
			{
			case 0:
				period_in_m_cyles = 64 * 4;  // 4 kHz
				break;
			case 1:
				period_in_m_cyles = 1 * 4;  // 256 kHz
				break;
			case 2:
				period_in_m_cyles = 4 * 4;  // 64 kHz
				break;
			case 3:
				period_in_m_cyles = 16 * 4;  // 16 kHz
				break;
			}
			assert(period_in_m_cyles >= 0);

			// While instead of if because it could happen several times just after
			// the clock_select was changed.
			while (gb->timer.remaining_m_cycles >= period_in_m_cyles)
			{
				gb->timer.remaining_m_cycles -= (uint16_t)period_in_m_cyles;

				if (gb->timer.tima == 255u)
				{
					gb->timer.tima = gb->timer.tma;
					gb->cpu.interrupt.if_flags.timer = 1;
				}
				else
				{
					++gb->timer.tima;
				}
			}
		}

		// TODO(stefalie): Serial transfer is not implemented.
		// We pretend nothing is connect, read 0xFF from SB, and fire an interrupt.
		if (gb->serial.interrupt_timer > 0)
		{
			gb->serial.interrupt_timer -= (int16_t)elapsed_m_cycles;
			if (gb->serial.interrupt_timer <= 0)
			{
				gb->serial.sc &= 0x7F;
				gb->serial.sb &= 0xFF;
				gb->cpu.interrupt.if_flags.serial = 1;
				gb->serial.interrupt_timer = 0;
			}
		}
	}
}

typedef union gb__TileLine
{
	uint64_t line;
	uint8_t pixels[8];
} gb__TileLine;

static gb__TileLine
gb__Interleave(uint64_t mem_line1, uint64_t mem_line2)
{
	// mem_line1 = -------- -------- -------- -------- -------- -------- -------- 76543210
	// mem_line2 = -------- -------- -------- -------- -------- -------- -------- FEDCBA98
	//
	// n         = 76543210 FEDCBA98 -------- -------- -------- -------- -------- -------- : After (1)
	// n         = ----3210 ----BA98 -------- -------- ----7654 ----FEDC -------- -------- : After (2)
	// n         = ------10 ------98 ------32 ------BA ------54 ------DC ------76 ------FE : After (3)
	// n         = ------80 ------91 ------A2 ------B3 ------C4 ------D5 ------E6 ------F7 : After (3)

	uint64_t n = (mem_line2 << 48) + (mem_line1 << 56);  // (1)
	n = (n ^ (n >> 36)) & 0x0F0F00000F0F0000;  // (2)
	n = (n ^ (n >> 18)) & 0x0303030303030303;  // (3)
	n = (n & 0x0100010001000100) + ((n & 0x0200020002000200) >> 9) +  // (4)
			(n & 0x0002000200020002) + ((n & 0x0001000100010001) << 9);

	return (gb__TileLine){ .line = n };
}

static gb__TileLine
gb__GetTileLine(gb_GameBoy *gb, uint8_t address_mode, uint8_t tile_index, uint8_t line_index)
{
	assert(address_mode == 0 || address_mode == 1);
	assert(line_index < 8);

	const int signed_tile_idx = address_mode == 0 ? (int8_t)tile_index : tile_index;

	const size_t set_offset = address_mode == 0 ? 0x1000 : 0x0;
	size_t vram_offset = set_offset;

	// A tile uses 16 bytes (2 bytes per line for 8x8 pixels).
	vram_offset += signed_tile_idx << 4;
	vram_offset += line_index << 1;
	assert(vram_offset < 0x17FF);
	assert((vram_offset & 1u) == 0);

	const gb__TileLine tile_line = gb__Interleave(gb->memory.vram[vram_offset], gb->memory.vram[vram_offset + 1]);
	return tile_line;
}

static inline gb__TileLine
gb__GetMapTileLine(
		gb_GameBoy *gb, uint8_t map_index, uint8_t address_mode, uint8_t tile_x, uint8_t tile_y, uint8_t line_index)
{
	const size_t vram_offset = map_index ? 0x1C00 : 0x1800;
	const size_t map_offset = vram_offset + (tile_y << 5u) + tile_x;
	const uint8_t tile_idx = gb->memory.vram[map_offset];

	gb__TileLine line = gb__GetTileLine(gb, address_mode, tile_idx, line_index);
	return line;
}

// TODO(stefalie): This is simplified.
// Rendering background, window, and sprites is interleaved using
// a 2-byte shift register. For details, see:
// https://www.youtube.com/watch?v=HyzD8pNlpwI
static void
gb__RenderScanLine(gb_GameBoy *gb)
{
	assert(gb->ppu.ly < 144);

	const union gb_PpuLcdc *lcdc = &gb->ppu.lcdc;

	const uint8_t bgp_map[4] = {
		(gb->ppu.bgp >> 0u) & 0x03,
		(gb->ppu.bgp >> 2u) & 0x03,
		(gb->ppu.bgp >> 4u) & 0x03,
		(gb->ppu.bgp >> 6u) & 0x03,
	};

	const uint8_t address_mode = lcdc->bg_and_win_addr_mode;

	// Values in [0, 3] per pixel.
	uint8_t scan_line[GB_FRAMEBUFFER_WIDTH] = { 0 };

	// Apply palette and remember that "transparent", i.e., 0-valued, pixels.
	bool transparent[GB_FRAMEBUFFER_WIDTH];

	// Background
	if (lcdc->bg_and_win_enable)
	{
		uint8_t y = gb->ppu.scy + gb->ppu.ly;
		uint8_t tile_y = y >> 3u;
		uint8_t in_tile_y = y & 7u;

		uint8_t fb_x = 0;
		while (fb_x < GB_FRAMEBUFFER_WIDTH)
		{
			uint8_t x = gb->ppu.scx + fb_x;
			uint8_t tile_x = x >> 3u;
			gb__TileLine line =
					gb__GetMapTileLine(gb, lcdc->bg_tilemap_select, address_mode, tile_x, tile_y, in_tile_y);

			for (size_t in_tile_x = x & 7u; in_tile_x < 8 && fb_x < GB_FRAMEBUFFER_WIDTH; ++fb_x, ++in_tile_x)
			{
				const uint8_t pixel = line.pixels[in_tile_x];
				transparent[fb_x] = pixel == 0;
				scan_line[fb_x] = bgp_map[pixel];
			}
		}
	}

	// Window
	if (lcdc->win_enable && lcdc->bg_and_win_enable)
	{
		// NOTE: This uses a separate, window internal "LY". See:
		// - https://www.reddit.com/r/EmuDev/comments/zzltyt/what_is_the_window_internal_line_counter
		// - https://gbdev.io/pandocs/Tile_Maps.html

		const bool win_visible = gb->ppu.ly >= gb->ppu.wy && gb->ppu.wy <= 143 && gb->ppu.wx >= 0 && gb->ppu.wx <= 166;
		if (win_visible)
		{
			// TODO(stefalie): Window bugs not implemented.
			// See: https://gbdev.io/pandocs/Scrolling.html#ff4aff4b--wy-wx-window-y-position-x-position-plus-7
			// Can't assert, Donkey Kong sets WX = 166. Sigh.
			// assert(gb->ppu.wx != 0 && gb->ppu.wx != 166);

			uint8_t y = gb->ppu.ly_win_internal;
			uint8_t tile_y = y >> 3u;
			uint8_t in_tile_y = y & 7u;

			size_t fb_x = MAX(0, (int)gb->ppu.wx - 7);
			while (fb_x < GB_FRAMEBUFFER_WIDTH)
			{
				uint8_t x = (uint8_t)(7 - gb->ppu.wx + fb_x);  // fb_x = x + wx - 7
				assert(x < GB_FRAMEBUFFER_WIDTH + 7);
				uint8_t tile_x = x >> 3u;
				gb__TileLine line =
						gb__GetMapTileLine(gb, lcdc->win_tilemap_select, address_mode, tile_x, tile_y, in_tile_y);

				for (size_t in_tile_x = x & 7u; in_tile_x < 8 && fb_x < GB_FRAMEBUFFER_WIDTH; ++fb_x, ++in_tile_x)
				{
					const uint8_t pixel = line.pixels[in_tile_x];
					transparent[fb_x] = pixel == 0;
					scan_line[fb_x] = bgp_map[pixel];
				}
			}

			++gb->ppu.ly_win_internal;
		}
	}

	// Sprites
	if (lcdc->sprite_enable)
	{
		const uint8_t obp0_map[4] = {
			(gb->ppu.obp0 >> 0u) & 0x03,
			(gb->ppu.obp0 >> 2u) & 0x03,
			(gb->ppu.obp0 >> 4u) & 0x03,
			(gb->ppu.obp0 >> 6u) & 0x03,
		};
		const uint8_t obp1_map[4] = {
			(gb->ppu.obp1 >> 0u) & 0x03,
			(gb->ppu.obp1 >> 2u) & 0x03,
			(gb->ppu.obp1 >> 4u) & 0x03,
			(gb->ppu.obp1 >> 6u) & 0x03,
		};

		const gb_Sprite *sprites = gb->memory.oam.sprites;
		const int sprite_height = gb->ppu.lcdc.sprite_size == 1 ? 16 : 8;

		// Gather the first 10 sprites that overlap with scanline.
		int num_scanned_sprites = 0;
		gb_Sprite scanned_sprites[10];

		for (int i = 0; i < 40 && num_scanned_sprites < 10; ++i)
		{
			int in_tile_y = gb->ppu.ly - (sprites[i].y_pos - 16);
			if (in_tile_y >= 0 && in_tile_y < sprite_height)
			{
				scanned_sprites[num_scanned_sprites] = sprites[i];
				++num_scanned_sprites;
			}
		}

		// Insertion sort the scanned tiles by x position
		// (For <= 10 elements this is likely faster than anything else.)
		for (int i = 1; i < num_scanned_sprites; ++i)
		{
			gb_Sprite x = scanned_sprites[i];
			int j = i;
			while (j > 0 && scanned_sprites[j - 1].x_pos > x.x_pos)
			{
				scanned_sprites[j] = scanned_sprites[j - 1];
				--j;
			}
			scanned_sprites[j] = x;
		}

		// Render sprites in order
		//
		// Whenever a sprite affects a pixel (even if it is behind the
		// background), no other sprites will affect the same pixel.
		// We use mask to track this.
		//
		// See: https://gbdev.io/pandocs/OAM.html
		bool masked[GB_FRAMEBUFFER_WIDTH] = { 0 };

		for (size_t i = 0; i < num_scanned_sprites; ++i)
		{
			const gb_Sprite sprite = scanned_sprites[i];

			int in_tile_y = gb->ppu.ly - (sprite.y_pos - 16);
			if (in_tile_y >= 0 && in_tile_y < sprite_height)
			{
				const int fb_start_x = sprite.x_pos - 8;
				if (fb_start_x > -8 && fb_start_x < GB_FRAMEBUFFER_WIDTH)
				{
					// Flip vertically.
					if (sprite.y_flip == 1)
					{
						in_tile_y = (sprite_height - 1) - in_tile_y;
					}
					assert(in_tile_y < sprite_height);

					// Second half of 8x16 sprite is in the next tile
					uint8_t tile_idx = sprite.tile_index;
					if (sprite_height == 16)
					{
						// Double tile sprites always start on an even tile index.
						tile_idx &= 0xFE;

						if (in_tile_y >= 8)
						{
							in_tile_y -= 8;
							tile_idx |= 0x01;
						}
					}

					const gb__TileLine line = gb__GetTileLine(gb, 1, tile_idx, (uint8_t)in_tile_y);
					const uint8_t *pal_map = sprite.dmg_palette == 0 ? obp0_map : obp1_map;

					for (int in_tile_x = 0; in_tile_x < 8; ++in_tile_x)
					{
						const int fb_x = fb_start_x + (sprite.x_flip == 0 ? in_tile_x : 7 - in_tile_x);

						if (fb_x >= 0 && fb_x < GB_FRAMEBUFFER_WIDTH)
						{
							const uint8_t sprite_pixel = line.pixels[in_tile_x];

							if (sprite_pixel != 0)  // Not transparent
							{
								if (!masked[fb_x] && (transparent[fb_x] || sprite.priority == 0))
								{
									scan_line[fb_x] = pal_map[sprite_pixel];
								}
								masked[fb_x] = true;
							}
						}
					}
				}
			}
		}
	}

	// Convert values [0, 3] to colors
	for (size_t fb_x = 0; fb_x < GB_FRAMEBUFFER_WIDTH; ++fb_x)
	{
		gb->display.pixels[GB_FRAMEBUFFER_WIDTH * gb->ppu.ly + fb_x] = gb__DefaultPalette[scan_line[fb_x]];
	}
}

#define MODE_HBLANK_LENGTH 204
#define MODE_OAM_SCAN_LENGTH 80
#define MODE_VRAM_SCAN_LENGTH 172
#define MODE_VBLANK_LINE_LENGTH 456

static void
gb__AdvancePpu(gb_GameBoy *gb, uint16_t elapsed_m_cycles)
{
	struct gb_Ppu *ppu = &gb->ppu;

	if (ppu->lcdc.lcd_enable == 0)
	{
		// See: https://www.reddit.com/r/Gameboy/comments/a1c8h0/comment/eap4f8c/
		assert(ppu->ly == 0);
		assert(ppu->stat.mode == GB_PPU_MODE_HBLANK);
		assert(ppu->mode_clock == 0);
		return;
	}

	// TODO(stefalie): We don't emulate this:
	// "When re-enabling the LCD, the PPU will immediately start drawing again,
	// but the screen will stay blank during the first frame."
	// From: https://gbdev.io/pandocs/LCDC.html

	ppu->mode_clock += 4 * elapsed_m_cycles;

	// const bool irq_line_was_low = !gb__StatInterruptLine(&ppu->;
	union gb_PpuStat *stat = &ppu->stat;
	bool prev_int48_signal = gb__LcdStatInt48Line(gb);

	switch (stat->mode)
	{
	case GB_PPU_MODE_HBLANK:
		assert(stat->mode != GB_PPU_MODE_VBLANK);
		assert(stat->mode != GB_PPU_MODE_OAM_SCAN);
		if (ppu->mode_clock >= MODE_HBLANK_LENGTH)
		{
			ppu->mode_clock -= MODE_HBLANK_LENGTH;
			++ppu->ly;
			gb__CompareLyToLyc(gb, prev_int48_signal);
			assert(ppu->ly <= 144);

			if (ppu->ly == 144)
			{
				stat->mode = GB_PPU_MODE_VBLANK;
				gb->cpu.interrupt.if_flags.vblank = 1;
				gb->display.updated = true;

				if (!prev_int48_signal && stat->interrupt_mode_vblank)
				{
					gb->cpu.interrupt.if_flags.lcd_stat = 1;
				}
			}
			else
			{
				stat->mode = GB_PPU_MODE_OAM_SCAN;

				if (!prev_int48_signal && stat->interrupt_mode_oam_scan)
				{
					gb->cpu.interrupt.if_flags.lcd_stat = 1;
				}
			}
		}
		break;
	case GB_PPU_MODE_VBLANK: {
		assert(stat->mode != GB_PPU_MODE_HBLANK);
		assert(stat->mode != GB_PPU_MODE_OAM_SCAN);

		const bool last_line_ly_update = ppu->ly == 153 && ppu->mode_clock >= 56;
		if (last_line_ly_update)
		{
			// At line 153 the LY update comes a lot earlier than on other lines.
			// LY == 0 will therefore take that much longer.
			// See: https://gameboy.mongenel.com/dmg/istat98.txt
			ppu->ly = 0;
			gb->ppu.ly_win_internal = 0;
			gb__CompareLyToLyc(gb, prev_int48_signal);
		}
		if (ppu->mode_clock >= MODE_VBLANK_LINE_LENGTH)
		{
			ppu->mode_clock -= MODE_VBLANK_LINE_LENGTH;
			if (ppu->ly != 0)
			{
				++ppu->ly;
				gb__CompareLyToLyc(gb, prev_int48_signal);
				assert(ppu->ly > 144 && ppu->ly <= 153);
			}

			if (ppu->ly == 0)  // LY already advanced to line 0.
			{
				stat->mode = GB_PPU_MODE_OAM_SCAN;

				if (!prev_int48_signal && stat->interrupt_mode_oam_scan)
				{
					gb->cpu.interrupt.if_flags.lcd_stat = 1;
				}
			}
		}
		break;
	}
	case GB_PPU_MODE_OAM_SCAN:
		assert(stat->mode != GB_PPU_MODE_HBLANK);
		assert(stat->mode != GB_PPU_MODE_VBLANK);
		if (ppu->mode_clock >= MODE_OAM_SCAN_LENGTH)
		{
			ppu->mode_clock -= MODE_OAM_SCAN_LENGTH;
			stat->mode = GB_PPU_MODE_VRAM_SCAN;

			// This is vastly simplified. Ideally the LCD should get updated after
			// every T cycle. See: https://gbdev.io/pandocs/Rendering.html
			gb__RenderScanLine(gb);
		}
		break;
	case GB_PPU_MODE_VRAM_SCAN:
		assert(stat->mode != GB_PPU_MODE_HBLANK);
		assert(stat->mode != GB_PPU_MODE_VBLANK);
		assert(stat->mode != GB_PPU_MODE_OAM_SCAN);
		if (ppu->mode_clock >= MODE_VRAM_SCAN_LENGTH)
		{
			ppu->mode_clock -= MODE_VRAM_SCAN_LENGTH;
			stat->mode = GB_PPU_MODE_HBLANK;

			if (!prev_int48_signal && stat->interrupt_mode_hblank)
			{
				gb->cpu.interrupt.if_flags.lcd_stat = 1;
			}

			// NOTE: By rendering full scan lines at a time instead of pushing out
			// pixels at every clock tick we have the problem that it's not clear
			// when the best time to render the scan line is.
			// We have either the beginning or the end of the VRAM scan mode. I
			// decided to go with the beginning (therefore the call to gb__RenderScanLine
			// is in the GB_PPU_MODE_OAM_SCAN case). This fixes a subtle bug in Castelian
			// where the siwtch from rendering the score board to the background happens
			// one line to early and causes flickering.
			// gb__RenderScanLine(gb);
		}
		break;
	}

	assert(stat->mode != GB_PPU_MODE_HBLANK || ppu->mode_clock < MODE_HBLANK_LENGTH);
	assert(stat->mode != GB_PPU_MODE_VBLANK || ppu->mode_clock < MODE_VBLANK_LINE_LENGTH);
	assert(stat->mode != GB_PPU_MODE_OAM_SCAN || ppu->mode_clock < MODE_OAM_SCAN_LENGTH);
	assert(stat->mode != GB_PPU_MODE_VRAM_SCAN || ppu->mode_clock < MODE_VRAM_SCAN_LENGTH);
}

size_t
gb_ExecuteNextInstruction(gb_GameBoy *gb)
{
	assert(gb->rom.data);
	assert(gb->rom.num_bytes);

	if (gb->cpu.pc == 0x0100)
	{
		// The BIOS gets automatically unmapped by writing to 0xFF50, and the
		// BIOS itself does that.
		assert(!gb->memory.bios_mapped);

		// See Sec. 5.1 of The Cycle-Accurate Game Boy Docs
		// The timer will be bogus will running the BIOS.
		gb->timer.t_clock = 0xABCC;
	}

	uint16_t num_cycles = 0;
	if (!gb->cpu.halt)
	{
		const gb_Instruction inst = gb_FetchInstruction(gb, gb->cpu.pc);
		gb->cpu.pc += gb_InstructionSize(inst);

		num_cycles =
				inst.is_extended ? gb__ExecuteExtendedInstruction(gb, inst) : gb__ExecuteBasicInstruction(gb, inst);

		// NOTE: Unclear if the updates should be done before or after the execution
		// of the instruction. (Before has the problem that you don't know how many
		// cycles it took in the case of a conditional jump.) That is the curse of
		// instruction-stepping and of always rendering full scan lines at once.
		gb__AdvancePpu(gb, num_cycles);
		gb__UpdateClockAndTimer(gb, num_cycles);

#if BLARGG_TEST_ENABLE
		if (gb->serial.sc == 0x81)
		{
			char c = gb->serial.sb;
			printf("%c", c);
			gb->serial.sc = 0;
		}
#endif
	}

	const uint16_t num_interrupt_cycles = gb__HandleInterrupts(gb);
	if (num_interrupt_cycles > 0)
	{
		gb__UpdateClockAndTimer(gb, num_interrupt_cycles);
		gb__AdvancePpu(gb, num_interrupt_cycles);
		num_cycles += num_interrupt_cycles;
	}

	if (gb->cpu.interrupt.ime_after_next_inst)
	{
		gb->cpu.interrupt.ime = true;
		gb->cpu.interrupt.ime_after_next_inst = false;
	}

	if (num_cycles == 0)
	{
		// When the CPU is halted, we still need to let cycles "elapse" so that the
		// timer progresses.
		// TODO(stefalie): Take bigger steps when just advancing the timer? 4 m cycles instead?
		num_cycles = 1;
		gb__UpdateClockAndTimer(gb, num_cycles);
		gb__AdvancePpu(gb, num_cycles);
	}

	if (gb->serial.enable_interrupt_timer)
	{
		// This will trigger an interrupt 8 bit clocks (8192 Hz) later on.
		// See page 31 of the GameBoy CPU manual.
		// GB_MACHINE_M_FREQ / 8192 == 128 m cyles
		gb->serial.interrupt_timer = 128 * 8;

		gb->serial.enable_interrupt_timer = false;
	}

	assert(num_cycles > 0);
	return num_cycles;
}

void
gb_SetInput(gb_GameBoy *gb, gb_Input input, bool down)
{
	struct gb_Joypad *pad = &gb->joypad;
	uint8_t *reg = NULL;
	uint8_t bit = 0;
	switch (input)
	{
	case GB_INPUT_BUTTON_A:
		reg = &pad->buttons;
		bit = 0x01;
		break;
	case GB_INPUT_BUTTON_B:
		reg = &pad->buttons;
		bit = 0x02;
		break;
	case GB_INPUT_BUTTON_SELECT:
		reg = &pad->buttons;
		bit = 0x04;
		break;
	case GB_INPUT_BUTTON_START:
		reg = &pad->buttons;
		bit = 0x08;
		break;
	case GB_INPUT_ARROW_RIGHT:
		reg = &pad->dpad;
		bit = 0x01;
		break;
	case GB_INPUT_ARROW_LEFT:
		reg = &pad->dpad;
		bit = 0x02;
		break;
	case GB_INPUT_ARROW_UP:
		reg = &pad->dpad;
		bit = 0x04;
		break;
	case GB_INPUT_ARROW_DOWN:
		reg = &pad->dpad;
		bit = 0x08;
		break;
	}

	if (down)
	{
		const bool any_input_low_prev =
				(pad->buttons_select == 0 && pad->buttons != 0xF) || (pad->dpad_select == 0 && pad->dpad != 0xF);

		*reg &= ~bit;

		const bool any_input_low_now =
				(pad->buttons_select == 0 && pad->buttons != 0xF) || (pad->dpad_select == 0 && pad->dpad != 0xF);

		// See 7.3 in The Cycle-Accurate Game Boy Docs.
		//
		// Check for falling edge of input lines.
		if (any_input_low_now && !any_input_low_prev)
		{
			gb->cpu.interrupt.if_flags.joypad = 1;
		}
	}
	else
	{
		*reg |= bit;
	}
	assert((*reg & 0xF0) == 0);
}

gb_Tile
gb_GetTile(gb_GameBoy *gb, uint8_t address_mode, uint8_t tile_index)
{
	gb_Tile result;

	for (int y = 0; y < 8; ++y)
	{
		gb__TileLine tile_line = gb__GetTileLine(gb, address_mode, tile_index, (uint8_t)y);
		for (size_t i = 0; i < 8; ++i)
		{
			result.pixels[y][i] = gb__DefaultPalette[tile_line.pixels[i]];
		}
	}

	return result;
}

gb_Tile
gb_GetMapTile(gb_GameBoy *gb, uint8_t map_index, uint8_t address_mode, size_t tile_x, size_t tile_y)
{
	gb_Tile result;

	for (int y = 0; y < 8; ++y)
	{
		gb__TileLine tile_line =
				gb__GetMapTileLine(gb, map_index, address_mode, (uint8_t)tile_x, (uint8_t)tile_y, (uint8_t)y);
		for (size_t i = 0; i < 8; ++i)
		{
			result.pixels[y][i] = gb__DefaultPalette[tile_line.pixels[i]];
		}
	}

	return result;
}

bool
gb_FramebufferUpdated(gb_GameBoy *gb)
{
	bool result = gb->display.updated;
	gb->display.updated = false;
	return result;
}

uint32_t
gb_MagFramebufferSizeInBytes(gb_MagFilter mag_filter)
{
	const uint32_t num_bytes = GB_FRAMEBUFFER_WIDTH * GB_FRAMEBUFFER_HEIGHT * sizeof(gb_Color);

	switch (mag_filter)
	{
	case GB_MAG_FILTER_NONE:
		return num_bytes;
	case GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X:
	case GB_MAG_FILTER_XBR2:
		return num_bytes * (2 * 2);
	case GB_MAG_FILTER_SCALE3X_ADVMAME3X_SCALEF:
		return num_bytes * (3 * 3);
	case GB_MAG_FILTER_SCALE4X_ADVMAME4X:
		// Needs storage for the temporary intermediate buffer. Smarter people might
		// be able to do it in place.
		return gb_MagFramebufferSizeInBytes(GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X) + num_bytes * (4 * 4);
	default:
		assert(false);
		return 0;
	}
}

uint32_t
gb_MaxMagFramebufferSizeInBytes(void)
{
	uint32_t bytes = 0;
	for (int i = 0; i < GB_MAG_FILTER_MAX_VALUE; ++i)
	{
		const uint32_t new_bytes = gb_MagFramebufferSizeInBytes(i);
		if (new_bytes > bytes)
		{
			bytes = new_bytes;
		}
	}
	return bytes;
}

static inline uint32_t
gb__SrcPixel(const gb_Framebuffer input, int x, int y)
{
	x = CLAMP(x, 0, input.width - 1);
	y = CLAMP(y, 0, input.height - 1);
	return input.pixels[y * input.width + x].as_u32;
}

// TODO(stefalie): None of the magnification filters below are optimized/vectorized.
// Often reading pixel values can be done in the outer loop and be reused in subsequent
// iterations of the inner loop.

// Uses notation from:
// McGuire, Gagiu; 2021; MMPX Style-Preserving Pixel-Art Magnification)
// Also see: https://www.scale2x.it
static gb_Framebuffer
gb__MagFramebufferEpxScale2xAdvMame2x(const gb_Framebuffer input, gb_Color *pixels)
{
	for (int y = 0; y < input.height; ++y)
	{
		for (int x = 0; x < input.width; ++x)
		{
			const uint32_t B = gb__SrcPixel(input, x + 0, y - 1);
			const uint32_t D = gb__SrcPixel(input, x - 1, y + 0);  // Read in outer loop, and then as prev E.
			const uint32_t E = gb__SrcPixel(input, x + 0, y + 0);  // Read in outer loop, and then as prev F.
			const uint32_t F = gb__SrcPixel(input, x + 1, y + 0);
			const uint32_t H = gb__SrcPixel(input, x + 0, y + 1);

			uint32_t J = E, K = E, L = E, M = E;

			if (D == B && D != H && D != F)
			{
				J = D;
			}
			if (B == F && B != D && B != H)
			{
				K = B;
			}
			if (H == D && H != F && H != B)
			{
				L = H;
			}
			if (F == H && F != B && F != D)
			{
				M = F;
			}

			int dst_idx = (y * 2) * 2 * input.width + (2 * x);
			pixels[dst_idx++].as_u32 = J;
			pixels[dst_idx].as_u32 = K;
			dst_idx += 2 * input.width - 1;
			pixels[dst_idx++].as_u32 = L;
			pixels[dst_idx].as_u32 = M;
		}
	}

	return (gb_Framebuffer){
		.width = 2 * input.width,
		.height = 2 * input.height,
		.pixels = pixels,
	};
}

// Uses notation from:
// https://en.wikipedia.org/wiki/Pixel-art_scaling_algorithms#Scale3%C3%97/AdvMAME3%C3%97_and_ScaleFX
// (The input notation is identical to:
// McGuire, Gagiu; 2021; MMPX Style-Preserving Pixel-Art Magnification)
// Also see: https://www.scale2x.it
static gb_Framebuffer
gb__MagFramebufferScale3xAdvMame3xScaleF(gb_Framebuffer input, gb_Color *pixels)
{
	for (int y = 0; y < input.height; ++y)
	{
		for (int x = 0; x < input.width; ++x)
		{
			const uint32_t A = gb__SrcPixel(input, x - 1, y - 1);
			const uint32_t B = gb__SrcPixel(input, x + 0, y - 1);
			const uint32_t C = gb__SrcPixel(input, x + 1, y - 1);
			const uint32_t D = gb__SrcPixel(input, x - 1, y + 0);
			const uint32_t E = gb__SrcPixel(input, x + 0, y + 0);
			const uint32_t F = gb__SrcPixel(input, x + 1, y + 0);
			const uint32_t G = gb__SrcPixel(input, x - 1, y + 1);
			const uint32_t H = gb__SrcPixel(input, x + 0, y + 1);
			const uint32_t I = gb__SrcPixel(input, x + 1, y + 1);

			uint32_t _1 = E, _2 = E, _3 = E, _4 = E, _5 = E, _6 = E, _7 = E, _8 = E, _9 = E;

			const bool upper_left = D == B && D != H && B != F;
			const bool upper_right = B == F && B != D && F != H;
			const bool bottom_left = H == D && H != F && D != B;
			const bool bottom_right = F == H && F != B && H != D;

			if (upper_left)
			{
				_1 = D;
			}
			if ((upper_left && E != C) || (upper_right && E != A))
			{
				_2 = B;
			}
			if (upper_right)
			{
				_3 = F;
			}
			if ((bottom_left && E != A) || (upper_left && E != G))
			{
				_4 = D;
			}
			if ((upper_right && E != I) || (bottom_right && E != C))
			{
				_6 = F;
			}
			if (bottom_left)
			{
				_7 = D;
			}
			if ((bottom_right && E != G) || (bottom_left && E != I))
			{
				_8 = H;
			}
			if (bottom_right)
			{
				_9 = F;
			}

			int dst_idx = (y * 3) * 3 * input.width + (3 * x);
			pixels[dst_idx++].as_u32 = _1;
			pixels[dst_idx++].as_u32 = _2;
			pixels[dst_idx].as_u32 = _3;
			dst_idx += 3 * input.width - 2;
			pixels[dst_idx++].as_u32 = _4;
			pixels[dst_idx++].as_u32 = _5;
			pixels[dst_idx].as_u32 = _6;
			dst_idx += 3 * input.width - 2;
			pixels[dst_idx++].as_u32 = _7;
			pixels[dst_idx++].as_u32 = _8;
			pixels[dst_idx].as_u32 = _9;
		}
	}

	return (gb_Framebuffer){
		.width = 3 * input.width,
		.height = 3 * input.height,
		.pixels = pixels,
	};
}

static inline gb_Framebuffer
gb__MagFramebufferScale4xAdvMame4x(const gb_Framebuffer input, gb_Color *pixels)
{
	// Run Scale2x twice.
	gb_Color *pixels2x = pixels + (4 * input.width) * (4 * input.height);
	const gb_Framebuffer fb2x = gb__MagFramebufferEpxScale2xAdvMame2x(input, pixels2x);
	return gb__MagFramebufferEpxScale2xAdvMame2x(fb2x, pixels);
}

// xBR
// Heavily based on:
// - https://github.com/Treeki/libxbr-standalone, GPL 2.1 license)
//   Copyright 2011, 2012 Hyllian/Jararaca <sergiogdb@gmail.com>
//   Copyright 2014 Arwa Arif <arwaarif1994@gmail.com>
//   Copyright 2015 Treeki <treeki@gmail.com>
// - https://github.com/joseprio/xBRjs, MIT license.
//   Copyright 2020 Josep del Rio.
// - https://forums.libretro.com/t/xbr-algorithm-tutorial/123

// Pixel difference
// I don't understand how the original C version builds the rgb2yuv lookup table
// (https://github.com/Treeki/libxbr-standalone/blob/master/xbr.c#L317). The JS
// version and the tutorial are alot easier to grok, and therefore we base the
// implementation here on that.
#define XBR_THRESHHOLD_Y 48.0f
#define XBR_THRESHHOLD_U 7.0f
#define XBR_THRESHHOLD_V 6.0f

typedef struct gb__XbrYuv
{
	float y, u, v;
} gb__XbrYuv;

static inline gb__XbrYuv
gb__XbrRgb2Yuv(uint32_t rgb)
{
	const uint8_t r = rgb & 0xFF;
	const uint8_t g = (rgb >> 8u) & 0xFF;
	const uint8_t b = (rgb >> 16u) & 0xFF;
	return (gb__XbrYuv){
		.y = 0.299f * r + 0.587f * g + 0.114f * b,
		.u = -0.169f * r - 0.331f * g + 0.500f * b,
		.v = 0.500f * r - 0.419f * g - 0.081f * b,
	};
}

static inline uint32_t
gb__XbrDiff(uint32_t a, uint32_t b)
{
	const gb__XbrYuv a_yuv = gb__XbrRgb2Yuv(a);
	const gb__XbrYuv b_yuv = gb__XbrRgb2Yuv(b);
	return (uint32_t)(fabsf(a_yuv.y - b_yuv.y) * XBR_THRESHHOLD_Y + fabsf(a_yuv.u - b_yuv.u) * XBR_THRESHHOLD_U +
			fabsf(a_yuv.v - b_yuv.v) * XBR_THRESHHOLD_V);
}

static inline bool
gb__XbrEq(uint32_t a, uint32_t b)
{
	const gb__XbrYuv a_yuv = gb__XbrRgb2Yuv(a);
	const gb__XbrYuv b_yuv = gb__XbrRgb2Yuv(b);
	return fabsf(a_yuv.y - b_yuv.y) <= XBR_THRESHHOLD_Y && fabsf(a_yuv.u - b_yuv.u) <= XBR_THRESHHOLD_U &&
			fabsf(a_yuv.v - b_yuv.v) <= XBR_THRESHHOLD_V;
}

static inline uint32_t
gb__XbrInterp(uint32_t a, uint32_t b, uint32_t m, uint32_t s)
{
	const uint32_t part_mask = 0x00FF00FF;

	const uint32_t a_part1 = a & part_mask;
	const uint32_t b_part1 = b & part_mask;
	const uint32_t a_part2 = (a >> 8u) & part_mask;
	const uint32_t b_part2 = (b >> 8u) & part_mask;

	const uint32_t result_part1 = part_mask & (a_part1 + (((b_part1 - a_part1) * m) >> s));
	const uint32_t result_part2 = (part_mask & (a_part2 + (((b_part2 - a_part2) * m) >> s))) << 8u;

	return result_part1 + result_part2;
}

#define XBR_INTERP_32(a, b) gb__XbrInterp(a, b, 1, 3)
#define XBR_INTERP_64(a, b) gb__XbrInterp(a, b, 1, 2)
#define XBR_INTERP_128(a, b) gb__XbrInterp(a, b, 1, 1)
#define XBR_INTERP_192(a, b) gb__XbrInterp(a, b, 3, 2)
#define XBR_INTERP_224(a, b) gb__XbrInterp(a, b, 7, 3)

static inline void
gb__XbrFilter2(uint32_t E, uint32_t I, uint32_t H, uint32_t F, uint32_t G, uint32_t C, uint32_t D, uint32_t B,
		uint32_t F4, uint32_t I4, uint32_t H5, uint32_t I5, int N1, int N2, int N3, uint32_t *E_out)
{
	if (E != H && E != F)
	{
		const uint32_t e = gb__XbrDiff(E, C) + gb__XbrDiff(E, G) + gb__XbrDiff(I, H5) + gb__XbrDiff(I, F4) +
				(4 * gb__XbrDiff(H, F));
		const uint32_t i = gb__XbrDiff(H, D) + gb__XbrDiff(H, I5) + gb__XbrDiff(F, I4) + gb__XbrDiff(F, B) +
				(4 * gb__XbrDiff(E, I));
		if (e <= i)
		{
			const uint32_t px = gb__XbrDiff(E, F) <= gb__XbrDiff(E, H) ? F : H;
			if (e < i &&
					((!gb__XbrEq(F, B) && !gb__XbrEq(H, D)) ||
							(gb__XbrEq(E, I) && (!gb__XbrEq(F, I4) && !gb__XbrEq(H, I5))) || gb__XbrEq(E, G) ||
							gb__XbrEq(E, C)))
			{
				const uint32_t ke = gb__XbrDiff(F, G);
				const uint32_t ki = gb__XbrDiff(H, C);
				const int left = ke << 1 <= ki && E != G && D != G;
				const int up = ke >= ki << 1 && E != C && B != C;
				if (left && up)
				{
					E_out[N3] = XBR_INTERP_224(E_out[N3], px);
					E_out[N2] = XBR_INTERP_64(E_out[N2], px);
					E_out[N1] = E_out[N2];
				}
				else if (left)
				{
					E_out[N3] = XBR_INTERP_192(E_out[N3], px);
					E_out[N2] = XBR_INTERP_64(E_out[N2], px);
				}
				else if (up)
				{
					E_out[N3] = XBR_INTERP_192(E_out[N3], px);
					E_out[N1] = XBR_INTERP_64(E_out[N1], px);
				}
				else
				{
					E_out[N3] = XBR_INTERP_128(E_out[N3], px);
				}
			}
			else  // if (e ==i)
			{
				E_out[N3] = XBR_INTERP_128(E_out[N3], px);
			}
		}
	}
}

static gb_Framebuffer
gb__MagFramebufferXbr2(const gb_Framebuffer input, gb_Color *pixels)
{
	const int nl = input.width * 2;

	for (int y = 0; y < input.height; ++y)
	{
		// Output
		uint32_t *E_out = &pixels[0].as_u32 + y * input.width * 2 * 2;

		const uint32_t *row0 = &input.pixels[0].as_u32 + (y - 2) * input.width - 2;
		const uint32_t *row1 = row0 + input.width;
		const uint32_t *row2 = row1 + input.width;
		const uint32_t *row3 = row2 + input.width;
		const uint32_t *row4 = row3 + input.width;

		// Clamping
		if (y == 0)
		{
			row0 = row2;
			row1 = row2;
		}
		else if (y == 1)
		{
			row0 = row1;
		}
		else if (y == input.height - 2)
		{
			row4 = row3;
		}
		else if (y == input.height - 1)
		{
			row4 = row2;
			row3 = row2;
		}

		for (int x = 0; x < input.width; ++x)
		{

			const int prev = 2 - (x > 0);
			const int prev2 = prev - (x > 1);
			const int next = 2 + (x < input.width - 1);
			const int next2 = next + (x < input.width - 2);

			const uint32_t A0 = row1[prev2];
			const uint32_t D0 = row2[prev2];
			const uint32_t G0 = row3[prev2];

			const uint32_t A1 = row0[prev];
			const uint32_t A = row1[prev];
			const uint32_t D = row2[prev];
			const uint32_t G = row3[prev];
			const uint32_t G5 = row4[prev];

			const uint32_t B1 = row0[2];
			const uint32_t B = row1[2];
			const uint32_t E = row2[2];
			const uint32_t H = row3[2];
			const uint32_t H5 = row4[2];

			const uint32_t C1 = row0[next];
			const uint32_t C = row1[next];
			const uint32_t F = row2[next];
			const uint32_t I = row3[next];
			const uint32_t I5 = row4[next];

			const uint32_t C4 = row1[next2];
			const uint32_t F4 = row2[next2];
			const uint32_t I4 = row3[next2];

			// This uses only the n == 2 version
			E_out[0] = E_out[1] = E_out[nl] = E_out[nl + 1] = E;
			gb__XbrFilter2(E, I, H, F, G, C, D, B, F4, I4, H5, I5, 1, nl, nl + 1, E_out);
			gb__XbrFilter2(E, C, F, B, I, A, H, D, B1, C1, F4, C4, 0, nl + 1, 1, E_out);
			gb__XbrFilter2(E, A, B, D, C, G, F, H, D0, A0, B1, A1, nl, 1, 0, E_out);
			gb__XbrFilter2(E, G, D, H, A, I, B, F, H5, G5, D0, G0, nl + 1, 0, nl, E_out);

			++row0;
			++row1;
			++row2;
			++row3;
			++row4;
			E_out += 2;
		}
	}

	return (gb_Framebuffer){
		.width = 2 * GB_FRAMEBUFFER_WIDTH,
		.height = 2 * GB_FRAMEBUFFER_HEIGHT,
		.pixels = pixels,
	};
}

gb_Framebuffer
gb_MagFramebuffer(const gb_GameBoy *gb, gb_MagFilter mag_filter, gb_Color *pixels)
{
	const gb_Framebuffer input = {
		.width = GB_FRAMEBUFFER_WIDTH,
		.height = GB_FRAMEBUFFER_HEIGHT,
		.pixels = gb->display.pixels,
	};

	switch (mag_filter)
	{
	case GB_MAG_FILTER_NONE:
		// Returns the internal pixel buffer.
		return input;
	case GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X:
		return gb__MagFramebufferEpxScale2xAdvMame2x(input, pixels);
	case GB_MAG_FILTER_SCALE3X_ADVMAME3X_SCALEF:
		return gb__MagFramebufferScale3xAdvMame3xScaleF(input, pixels);
	case GB_MAG_FILTER_SCALE4X_ADVMAME4X:
		return gb__MagFramebufferScale4xAdvMame4x(input, pixels);
	case GB_MAG_FILTER_XBR2:
		return gb__MagFramebufferXbr2(input, pixels);
	default:
		assert(false);
		return (gb_Framebuffer){ 0 };
	}
}
