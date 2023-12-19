// Copyright (C) 2022 Stefan Lienhard

#include "gb.h"
// TODO(stefalie): Get rid of the stdlib includes. Make it standalone.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(v, min, max) MIN(MAX(v, min), max)

// Dummy image to test image mag filters.
// TODO(stefalie): Remove once the emulator can actually generate images.
// Converted with: https://www.digole.com/tools/PicturetoC_Hex_converter.php
#include "tetris.h"

void
gb_Init(gb_GameBoy *gb)
{
	*gb = (gb_GameBoy){ 0 };
	memcpy(gb->framebuffer.pixels, gb_tetris_splash_screen, sizeof(gb->framebuffer.pixels));
}

// TODO:
// header struct:
// https://github.com/ThomasRinsma/dromaius/blob/ffe8e2bead2c11c525a07578a99d5ae464515f76/src/memory.h#L62

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
	0x06, 0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xF2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20, 0x4F, 0x16,
	0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17, 0x05, 0x20, 0xF5, 0x22, 0x23,
	0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67,
	0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3c, 0x42, 0xB9, 0xA5, 0xB9, 0xA5,
	0x42, 0x4C, 0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20, 0xF5,
	0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50 };

// TODO: needed? check how many times we use them and inline if not often.
// Replace with union?
// union WordOr2Bytes
//{
//	uint16_t word;
//	struct
//	{
//		uint8_t byte1;
//		uint8_t byte2;
//	};
//};
static inline uint8_t
gb__Hi(uint16_t val)
{
	return (val >> 8u) & 0xFF;
}
static inline uint8_t
gb__Lo(uint16_t val)
{
	return val & 0xFF;
}
// TODO: remove if never used
static inline uint16_t
gb__LoHi(uint8_t lo, uint8_t hi)
{
	return lo + (hi << 8u);
}

bool
gb_LoadRom(gb_GameBoy *gb, const uint8_t *rom, uint32_t num_bytes)
{
	gb->rom.data = rom;
	gb->rom.num_bytes = num_bytes;

	const gb__RomHeader *header = (gb__RomHeader *)&(gb->rom.data[ROM_HEADER_START_ADDRESS]);

	const uint8_t nintendo_logo[] = { 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00,
		0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9,
		0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E };
	// TODO: I think this is identical. Identical to what?
	// TODO: I think this can be skipped if we execute the BIOS startup sequence:
	// the comparison still happens, but it will be emulated.
	// const uint8_t* nintendo_logo = &gb__bios[120];

	if (memcmp(nintendo_logo, header->nintendo_logo, sizeof(header->nintendo_logo)))
	{
		return true;
	}

	// 0x80 is Color GameBoy
	if (header->gbc == 0x80)
	{
		return true;
	}

	// 0x00 is GameBoy, 0x03 is Super GameBoy
	if (header->sgb != 0x00)
	{
		return true;
	}

	gb->rom.name[15] = '\0';
	memcpy(gb->rom.name, header->rom_name, sizeof(header->rom_name));

	// Checksum test
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

	return false;
}

// TODO: Can gb_MemoryReadByte and gb__MemoryWriteByte be merged?
// Maybe a combined MemoryMap function that just returns a pointer.
// gb__MemoryWriteByte does some checks or redirections when accessing control registers, timers, roms. Otherwise it
// uses the map function. The map function asserts for all empty areas and stuff that doesn't have an address. the
// timers maybe (but even that has an address in our case).
//
// If this is possible depends on how many virtual addresses there are that
// behave different when reading from or writing to it.
// We will know more towards the end fo the project.
// static uint8_t*
// gb__MemoryMap(gb_GameBoy* gb, uint16_t addr)
//{
//}

uint8_t
gb_MemoryReadByte(const gb_GameBoy *gb, uint16_t addr)
{
	switch (addr & 0xF000)
	{
	// ROM bank 0 or BIOS
	case 0x0000:
	case 0x1000:
	case 0x2000:
	case 0x3000:
		if (gb->memory.bios_mapped && addr < 0x100)
		{
			return gb__bios[addr];
		}
		else
		{
			return gb->rom.data[addr];
		}
	// Switchable ROM bank
	case 0x4000:
	case 0x5000:
	case 0x6000:
	case 0x7000:
		// assert(!"TODO");
		const size_t rom_bank = 1;  // TODO
		const uint16_t rom_bank_0_end = 0x4000;
		return gb->rom.data[(rom_bank - 1) * rom_bank_0_end + addr];
	// VRAM
	case 0x8000:
	case 0x9000:
		return gb->memory.vram[addr & 0x1FFF];
	// Switchable RAM bank
	case 0xA000:
	case 0xB000:
		// assert(!"TODO");
		return gb->memory.external_ram[addr & 0x1FFF];
	// (Internal) working RAM
	case 0xC000:
	case 0xD000:
		// assert(!"TODO");
		return gb->memory.wram[addr & 0x1FFF];
	// Echo of (Internal) working RAM, I/O, zero page
	case 0xE000:
	case 0xF000:
		switch (addr & 0x0F00)
		{
		// Echo of (Internal) working RAM
		default:  // [0xE000, 0xFE00)
			assert((addr - 0xE000) < 0x1E00);
			return gb->memory.wram[addr & 0x1FFF];
		case 0x0E00:
			if (addr < 0xFEA0)  // Sprite Attrib Memory (OAM)
			{
				return gb->gpu.oam[addr - 0xFEA0];
			}
			else  // Empty
			{
				return 0;
			}
		case 0x0F00:
			if (addr == 0xFFFF)  // Interrupt enable
			{
				return gb->cpu.interrupts;
			}
			else if (addr < 0xFF4C)  // I/O
			{
				// assert(!"TODO");
				return 0;
			}
			else if (addr < 0xFF80)  // Empty
			{
				return 0;
			}
			else  // Zero page
			{
				assert((addr - 0xFF80) < 0x80);
				// assert(!"TODO");
				return gb->memory.zero_page_ram[addr & 0x7F];
			}
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
	switch (addr & 0xF000)
	{
	// ROM bank 0 or BIOS
	case 0x0000:
	case 0x1000:
	case 0x2000:
	case 0x3000:
		if (gb->memory.bios_mapped && addr < 0x100)
		{
			assert(false);
		}
		assert(!"TODO");
		break;
	// Switchable ROM bank
	case 0x4000:
	case 0x5000:
	case 0x6000:
	case 0x7000:
		assert(!"TODO");
		break;
	// VRAM
	case 0x8000:
	case 0x9000:
		gb->memory.vram[addr & 0x1FFF] = value;
		// TODO: Update tile?
		break;
	// Switchable RAM bank
	case 0xA000:
	case 0xB000:
		assert(!"TODO");
		gb->memory.external_ram[addr & 0x1FFF] = value;
		break;
	// (Internal) working RAM
	case 0xC000:
	case 0xD000:
		assert(!"TODO");
		gb->memory.wram[addr & 0x1FFF] = value;
		break;
	// Echo of (Internal) working RAM, I/O, zero page
	case 0xE000:
	case 0xF000:
		switch (addr & 0x0F00)
		{
		// Echo of (Internal) working RAM
		default:  // [0xE000, 0xFE00)
			assert((addr - 0xE000) < 0x1E00);
			gb->memory.wram[addr & 0x1FFF] = value;
		case 0x0E00:
			if (addr < 0xFEA0)  // Sprite Attrib Memory (OAM)
			{
				// return gb->gpu.oam[addr - 0xFEA0];
				assert(!"TODO");
			}
			else
			{
				// The empty area is ignored
				assert(!"TODO");
			}
			break;
		case 0x0F00:
			if (addr == 0xFFFF)  // Interrupt enable
			{
				gb->cpu.interrupts = value;
			}
			else if (addr < 0xFF4C)  // I/O
			{
				// TODO I/O
				// assert(!"TODO");
			}
			else if (addr < 0xFF80)
			{
				// The empty area is ignored
				assert(!"TODO");
			}
			else  // Zero page
			{
				assert((addr - 0xFF80) < 0x80);
				gb->memory.zero_page_ram[addr & 0x7F] = value;
			}
			break;
		}
		break;
	default:
		assert(false);
		break;
	}
}
// TODO: needed?
// static inline void
// gb__MemoryWriteWord(gb_GameBoy* gb, uint16_t addr, uint16_t value)
//{
//	gb__MemoryWriteByte(gb, addr, gb__Lo(value));
//	gb__MemoryWriteByte(gb, addr + 1, gb__Hi(value));
// }

void
gb_Reset(gb_GameBoy *gb, bool skip_bios)
{
	gb->memory.bios_mapped = true;

	// (see page 18 of the GameBoy CPU Manual)
	gb->cpu.a = 0x01;
	gb->cpu.f = 0xB0;
	gb->cpu.bc = 0x0013;
	gb->cpu.de = 0x00D8;
	gb->cpu.hl = 0x014D;
	gb->cpu.sp = 0xFFFE;
	if (!skip_bios)
	{
		gb->cpu.pc = 0x0;
	}
	else
	{
		gb->cpu.pc = ROM_HEADER_START_ADDRESS;
	}
	// TODO: Do we automatically end up at 0x0100 if we run from 0x0?

	gb->cpu.halt = false;

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
	uint8_t min_num_machine_cycles;  // -1 if depending
} gb__InstructionInfo;

static const gb__InstructionInfo gb__instruction_infos[256] = {
	[0x00] = { "NOP", 0, 1 },
	[0x01] = { "LD BC, u16", 2, 3 },
	[0x02] = { "LD (BC), A", 0, 2 },
	[0x03] = { "INC BC", 0, 2 },
	[0x04] = { "INC B", 0, 1 },
	[0x05] = { "DEC B", 0, 1 },
	[0x06] = { "LD B, u8", 1, 2 },
	[0x07] = { "RLCA", 0, 1 },

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

	[0x18] = { "JR n", 1, 3 },

	[0x1A] = { "LD A, (DE)", 0, 2 },
	[0x1B] = { "DEC DE", 0, 2 },
	[0x1C] = { "INC E", 0, 1 },
	[0x1D] = { "DEC E", 0, 1 },
	[0x1E] = { "LD E, u8", 1, 2 },
	[0x1F] = { "RRA", 0, 1 },

	[0x20] = { "JR NZ, i8", 1, (uint8_t)-1 },
	[0x21] = { "LD HL, u16", 2, 3 },
	[0x22] = { "LD (HL+), A", 0, 2 },
	[0x23] = { "INC HL", 0, 2 },
	[0x24] = { "INC H", 0, 1 },
	[0x25] = { "DEC H", 0, 1 },
	[0x26] = { "LD H, u8", 1, 2 },

	[0x28] = { "JR Z, i8", 1, (uint8_t)-1 },

	[0x2A] = { "LD A, (HL+)", 0, 2 },
	[0x2B] = { "DEC HL", 0, 2 },
	[0x2C] = { "INC L", 0, 1 },
	[0x2D] = { "DEC L", 0, 1 },
	[0x2E] = { "LD L, u8", 1, 2 },

	[0x30] = { "JR NC, i8", 1, (uint8_t)-1 },
	[0x31] = { "LD SP, u16", 2, 3 },
	[0x32] = { "LD (HL-), A", 0, 1 },
	[0x33] = { "INC SP", 0, 2 },
	[0x34] = { "INC (HL)", 0, 3 },
	[0x35] = { "DEC (HL)", 0, 3 },
	[0x36] = { "LD (HL), u8", 1, 3 },

	[0x38] = { "JR C, i8", 1, (uint8_t)-1 },

	[0x3A] = { "LD A, (HL-)", 0, 2 },
	[0x3B] = { "DEC SP", 0, 2 },
	[0x3C] = { "INC A", 0, 1 },
	[0x3D] = { "DEC A", 0, 1 },
	[0x3E] = { "LD A, u8", 1, 2 },

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

	[0xCB] = { "PREFIX", 0, 0 },  // Num cycles is 0 here because it's taken from the extended instruction table.

	[0xE6] = { "AND A, 8", 1, 2 },
	[0xEE] = { "XOR A, 8", 1, 2 },
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

	[0xB0] = { "SET 6, B", 0, 2 },
	[0xB1] = { "SET 6, C", 0, 2 },
	[0xB2] = { "SET 6, D", 0, 2 },
	[0xB3] = { "SET 6, E", 0, 2 },
	[0xB4] = { "SET 6, H", 0, 2 },
	[0xB5] = { "SET 6, L", 0, 2 },
	[0xB6] = { "SET 6, (HL)", 0, 4 },
	[0xB7] = { "SET 6, A", 0, 2 },

	[0xB8] = { "SET 7, B", 0, 2 },
	[0xB9] = { "SET 7, C", 0, 2 },
	[0xBA] = { "SET 7, D", 0, 2 },
	[0xBB] = { "SET 7, E", 0, 2 },
	[0xBC] = { "SET 7, H", 0, 2 },
	[0xBD] = { "SET 7, L", 0, 2 },
	[0xBE] = { "SET 7, (HL)", 0, 4 },
	[0xBF] = { "SET 7, A", 0, 2 },

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


gb_Instruction
gb_FetchInstruction(const gb_GameBoy *gb, uint16_t addr)
{
	gb_Instruction inst = {
		.opcode = gb_MemoryReadByte(gb, addr),
	};

	gb__InstructionInfo info;

	const uint8_t extended_inst_prefix = 0xCB;
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
		info = gb__instruction_infos[inst.opcode];
	}

	inst.num_operand_bytes = info.num_operand_bytes;

	if (inst.num_operand_bytes == 1)
	{
		inst.operand_byte = gb_MemoryReadByte(gb, addr + 1);
	}
	else if (inst.num_operand_bytes == 2)
	{
		inst.operand_word = gb_MemoryReadWord(gb, addr + 1);
	}

	return inst;
}

uint16_t
gb_InstructionSize(gb_Instruction inst)
{
	return inst.is_extended ? 2 : 1 + inst.num_operand_bytes;
}

// TODO: get rid of snprintf, strlen, memcpy to avoid std includes.
// should all be easy, and from snprintf you only need to know how to convert 1 byte numbers to 2-char strings.
size_t
gb_DisassembleInstruction(gb_Instruction inst, char str_buf[], size_t str_buf_len)
{
	const gb__InstructionInfo info =
			inst.is_extended ? gb__extended_instruction_infos[inst.opcode] : gb__instruction_infos[inst.opcode];
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

uint8_t *
gl__MapIndexToReg(gb_GameBoy *gb, uint8_t index)
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

uint8_t
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

uint8_t
gb__RotateLeft(gb_GameBoy *gb, uint8_t val, bool clear_zero)
{
	const bool co = val & 0x80;
	val <<= 1u;
	val |= gb->cpu.flags.carry;
	gb__SetFlags(gb, clear_zero ? false : val == 0, false, false, co);
	return val;
}

uint8_t
gb__RotateRightCircular(gb_GameBoy *gb, uint8_t val, bool clear_zero)
{
	const bool co = val & 0x01;
	val >>= 1u;
	if (co)
	{
		val |= 0x08;
	}
	gb__SetFlags(gb, clear_zero ? false : val == 0, false, false, co);
	return val;
}

uint8_t
gb__RotateRight(gb_GameBoy *gb, uint8_t val, bool clear_zero)
{
	const bool co = val & 0x01;
	val >>= 1u;
	val |= gb->cpu.flags.carry << 7u;
	gb__SetFlags(gb, clear_zero ? false : val == 0, false, false, co);
	return val;
}

uint8_t
gb__Add(gb_GameBoy *gb, uint8_t lhs, uint8_t rhs, bool carry_in)
{
	uint8_t ci = (carry_in ? gb->cpu.flags.carry : 0);
	uint16_t sum = lhs + rhs + ci;
	bool half_carry = (lhs & 0x0F) + (rhs & 0x0F) + ci > 0x0F;
	bool co = sum > 0xFF;
	uint8_t result = (uint8_t)sum;
	gb__SetFlags(gb, result == 0, false, half_carry, co);
	return result;
}

// Could this be implemented by doing gb__Add(lhs, 2's complement of (rhs + carry_if_enable)),
// and then by flipping the half carry, carry, and negate flags?
// I think so. See here in MAME where SBC and ADC are essentially identical (besides the +/-):
// https://github.com/mamedev/mame/blob/1fdf6d10a7907bc92cd9f36eda1eca0484e47aba/src/devices/cpu/lr35902/opc_main.hxx#L4
uint8_t
gb__Sub(gb_GameBoy *gb, uint8_t lhs, uint8_t rhs, bool carry_in)
{
	uint8_t ci = (carry_in ? gb->cpu.flags.carry : 0);
	uint16_t diff = lhs - rhs - ci;
	bool half_carry = (lhs & 0x0F) < (rhs & 0x0F) + ci;
	bool co = diff > 0xFF;
	uint8_t result = (uint8_t)diff;
	gb__SetFlags(gb, result == 0, true, half_carry, co);
	return result;
}

void
gb__Inc(gb_GameBoy *gb, uint8_t *val)
{
	++(*val);
	gb__SetFlags(gb, (*val) == 0, false, ((*val) & 0x0F) == 0, gb->cpu.flags.carry == 1);
}

void
gb__Dec(gb_GameBoy *gb, uint8_t *val)
{
	--(*val);
	gb__SetFlags(gb, (*val) == 0, true, ((*val) & 0x0F) == 0x0F, gb->cpu.flags.carry == 1);
}

size_t
gb__ExecuteBasicInstruction(gb_GameBoy *gb, gb_Instruction inst)
{
	const uint8_t extended_inst_prefix = 0xCB;
	assert(gb_MemoryReadByte(gb, gb->cpu.pc - gb_InstructionSize(inst)) != extended_inst_prefix ||
			gb_MemoryReadByte(gb, gb->cpu.pc - gb_InstructionSize(inst)) - 1 == extended_inst_prefix);

	size_t result = gb__instruction_infos[inst.opcode].min_num_machine_cycles;

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
		assert(false);
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
		assert(false);
		break;

	case 0x10:  // STOP
		gb->cpu.stop = true;
		assert(false);
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
		assert(false);
		break;

	case 0x18:  // JR n
		gb->cpu.pc += (int8_t)inst.operand_byte;
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
		assert(false);
		break;

	case 0x20:  // JR NZ, i8
		if (gb->cpu.flags.zero == 0)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			result = 3;
		}
		else
		{
			result = 2;
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

	case 0x28:  // JR Z, i8
		if (gb->cpu.flags.zero == 1)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			result = 3;
		}
		else
		{
			result = 2;
		}
		break;

	case 0x2A:  // LD A, (HL+)
		gb->cpu.a = gb_MemoryReadByte(gb, gb->cpu.de);
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

	case 0x30:  // JR NC, i8
		if (gb->cpu.flags.carry == 0)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			result = 3;
		}
		else
		{
			result = 2;
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

	case 0x38:  // JR C, i8
		if (gb->cpu.flags.carry == 1)
		{
			gb->cpu.pc += (int8_t)inst.operand_byte;
			result = 3;
		}
		else
		{
			result = 2;
		}
		break;

	case 0x3A:  // LD A, (HL-)
		gb->cpu.a = gb_MemoryReadByte(gb, gb->cpu.de);
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
		*gl__MapIndexToReg(gb, (inst.opcode - 0x40) >> 3u) = *gl__MapIndexToReg(gb, inst.opcode);
		assert(false);
		break;

	case 0x46:  // LD B, (HL)
	case 0x4E:  // LD C, (HL)
	case 0x56:  // LD D, (HL)
	case 0x5E:  // LD E, (HL)
	case 0x66:  // LD H, (HL)
	case 0x6E:  // LD L, (HL)
	case 0x7E:  // LD A, (HL)
		*gl__MapIndexToReg(gb, (inst.opcode - 0x40) >> 3u) = gb_MemoryReadByte(gb, gb->cpu.hl);
		break;

	case 0x70:  // LD (HL), B
	case 0x71:  // LD (HL), C
	case 0x72:  // LD (HL), D
	case 0x73:  // LD (HL), E
	case 0x74:  // LD (HL), H
	case 0x75:  // LD (HL), L
	case 0x77:  // LD (HL), A
		gb__MemoryWriteByte(gb, gb->cpu.hl, *gl__MapIndexToReg(gb, inst.opcode));
		assert(false);
		break;

	case 0x76:  // HALT
		gb->cpu.halt = true;
		// TODO: handle interrupts, etc.
		// Read page 19+ in the CPU manual
		assert(false);
		break;

	case 0x80:  // ADD A, B
	case 0x81:  // ADD A, C
	case 0x82:  // ADD A, D
	case 0x83:  // ADD A, E
	case 0x84:  // ADD A, H
	case 0x85:  // ADD A, L
	case 0x87:  // ADD A, A
		gb->cpu.a = gb__Add(gb, gb->cpu.a, *gl__MapIndexToReg(gb, inst.opcode), false);
		break;
	case 0x86:  // ADD A, (HL)
		gb->cpu.a = gb__Add(gb, gb->cpu.a, gb_MemoryReadByte(gb, gb->cpu.hl), false);
		break;

	case 0x88:  // ADC A, B
	case 0x89:  // ADC A, C
	case 0x8A:  // ADC A, D
	case 0x8B:  // ADC A, E
	case 0x8C:  // ADC A, H
	case 0x8D:  // ADC A, L
	case 0x8F:  // ADC A, A
		gb->cpu.a = gb__Add(gb, gb->cpu.a, *gl__MapIndexToReg(gb, inst.opcode), true);
		break;
	case 0x8E:  // ADC A, (HL)
		gb->cpu.a = gb__Add(gb, gb->cpu.a, gb_MemoryReadByte(gb, gb->cpu.hl), true);
		break;

	case 0x90:  // SUB A, B
	case 0x91:  // SUB A, C
	case 0x92:  // SUB A, D
	case 0x93:  // SUB A, E
	case 0x94:  // SUB A, H
	case 0x95:  // SUB A, L
	case 0x97:  // SUB A, A
		gb->cpu.a = gb__Sub(gb, gb->cpu.a, *gl__MapIndexToReg(gb, inst.opcode), false);
		break;
	case 0x96:  // SUB A, (HL)
		gb->cpu.a = gb__Sub(gb, gb->cpu.a, gb_MemoryReadByte(gb, gb->cpu.hl), false);
		break;

	case 0x98:  // SBC A, B
	case 0x99:  // SBC A, C
	case 0x9A:  // SBC A, D
	case 0x9B:  // SBC A, E
	case 0x9C:  // SBC A, H
	case 0x9D:  // SBC A, L
	case 0x9F:  // SBC A, A
		gb->cpu.a = gb__Sub(gb, gb->cpu.a, *gl__MapIndexToReg(gb, inst.opcode), true);
		break;
	case 0x9E:  // SBC A, (HL)
		gb->cpu.a = gb__Sub(gb, gb->cpu.a, gb_MemoryReadByte(gb, gb->cpu.hl), true);
		break;

	case 0xA0:  // AND A, B
	case 0xA1:  // AND A, C
	case 0xA2:  // AND A, D
	case 0xA3:  // AND A, E
	case 0xA4:  // AND A, H
	case 0xA5:  // AND A, L
	case 0xA7:  // AND A, A
		gb->cpu.a &= *gl__MapIndexToReg(gb, inst.opcode);
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		break;
	case 0xA6:  // AND A, (HL)
		gb->cpu.a &= gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		break;

	case 0xA8:  // XOR A, B
	case 0xA9:  // XOR A, C
	case 0xAA:  // XOR A, D
	case 0xAB:  // XOR A, E
	case 0xAC:  // XOR A, H
	case 0xAD:  // XOR A, L
	case 0xAF:  // XOR A, A
		gb->cpu.a ^= *gl__MapIndexToReg(gb, inst.opcode);
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		break;
	case 0xAE:  // XOR A, (HL)
		gb->cpu.a ^= gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		break;

	case 0xB0:  // OR A, B
	case 0xB1:  // OR A, C
	case 0xB2:  // OR A, D
	case 0xB3:  // OR A, E
	case 0xB4:  // OR A, H
	case 0xB5:  // OR A, L
	case 0xB7:  // OR A, A
		gb->cpu.a |= *gl__MapIndexToReg(gb, inst.opcode);
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		break;
	case 0xB6:  // OR A, (HL)
		gb->cpu.a |= gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		break;

	case 0xB8:  // CP A, B
	case 0xB9:  // CP A, C
	case 0xBA:  // CP A, D
	case 0xBB:  // CP A, E
	case 0xBC:  // CP A, H
	case 0xBD:  // CP A, L
	case 0xBF:  // CP A, A
	{
		uint8_t val = *gl__MapIndexToReg(gb, inst.opcode);
		gb__SetFlags(gb, gb->cpu.a == val, false, (gb->cpu.a & 0x0F) < (val & 0x0F), gb->cpu.a < val);
		break;
	}
	case 0xBE:  // CP A, (HL)
	{
		uint8_t val = gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__SetFlags(gb, gb->cpu.a == val, false, (gb->cpu.a & 0x0F) < (val & 0x0F), gb->cpu.a < val);
		break;
	}

	case 0xCB:  // PREFIX
		// Handled seperatly above.
		assert(false);
		break;

	case 0xE6:  // AND A, u8
		gb->cpu.a &= inst.operand_byte;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;

	case 0xEE:  // XOR A, u8
		gb->cpu.a ^= inst.operand_byte;
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
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
		// Checking that the return value is not -1 in the caller allows
		// implementing the instructions step by step. Whenever the check fails,
		// the debugger is openedn and it will tell us which instruction to
		// implement next so that the program can continue.
		result = (size_t)-1;
	}

	return result;
}

size_t
gb__ExecuteExtendedInstruction(gb_GameBoy *gb, gb_Instruction inst)
{
	const uint8_t extended_inst_prefix = 0xCB;
	assert(gb_MemoryReadByte(gb, gb->cpu.pc - gb_InstructionSize(inst)) == extended_inst_prefix);

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
		uint8_t *reg = gl__MapIndexToReg(gb, inst.opcode);
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
	case 0x0E:  // RRC A
	{
		uint8_t *reg = gl__MapIndexToReg(gb, inst.opcode);
		*reg = gb__RotateRightCircular(gb, *reg, false);
		break;
	}
	case 0x0F:  // RRC (HL)
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
		uint8_t *reg = gl__MapIndexToReg(gb, inst.opcode);
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
	case 0x1E:  // RR A
	{
		uint8_t *reg = gl__MapIndexToReg(gb, inst.opcode);
		*reg = gb__RotateRight(gb, *reg, false);
		break;
	}
	case 0x1F:  // RR (HL)
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
		uint8_t *reg = gl__MapIndexToReg(gb, inst.opcode);
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
		int8_t *reg = (int8_t *)gl__MapIndexToReg(gb, inst.opcode);
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
		uint8_t *reg = gl__MapIndexToReg(gb, inst.opcode);
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
		uint8_t *reg = gl__MapIndexToReg(gb, inst.opcode);
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
		size_t bit = (*gl__MapIndexToReg(gb, inst.opcode) >> bit_index) & 0x01;
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
		*gl__MapIndexToReg(gb, inst.opcode) &= mask;
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

	// Wip
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
		*gl__MapIndexToReg(gb, inst.opcode) |= 1u << bit_index;
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
		// See note in the end of gb__ExecuteBasicInstruction.
		return (size_t)-1;
	}

	return gb__extended_instruction_infos[inst.opcode].min_num_machine_cycles;
}

size_t
gb_ExecuteNextInstruction(gb_GameBoy *gb)
{
	assert(gb->rom.data);
	assert(gb->rom.num_bytes);

	if (gb->cpu.pc == 256)
	{
		gb->memory.bios_mapped = false;
	}

	const gb_Instruction inst = gb_FetchInstruction(gb, gb->cpu.pc);
	gb->cpu.pc += gb_InstructionSize(inst);

	const size_t num_elapsed_cylces =
			inst.is_extended ? gb__ExecuteExtendedInstruction(gb, inst) : gb__ExecuteBasicInstruction(gb, inst);

	if (num_elapsed_cylces == -1)
	{
		// Revert PC so that the debugger still displays the missing instruction.
		gb->cpu.pc -= gb_InstructionSize(inst);
	}
	return num_elapsed_cylces;
}

// Adapted from: Ericson, 2005, Real-Time Collision Detection, pages 316-317
uint16_t
gb__Part1By1(uint16_t n)
{
	// n = --------76543210 : Bits initially
	// n = ----7654----3210 : After (1)
	// n = --76--54--32--10 : After (2)
	// n = -7-6-5-4-3-2-1-0 : After (3)
	n = (n ^ (n << 4)) & 0x0f0f0f0f;  // (1)
	n = (n ^ (n << 2)) & 0x33333333;  // (2)
	n = (n ^ (n << 1)) & 0x55555555;  // (3)
	return n;
}
uint16_t
gb__Morton2(uint16_t line1, uint16_t line2)
{
	return (gb__Part1By1(line2) << 1) + gb__Part1By1(line1);
}

uint16_t
gb_GetTileLine(gb_GameBoy *gb, size_t set_index, int tile_index, size_t line_index)
{
	assert(set_index == 0 || set_index == 1);
	assert(line_index < 8);
	assert((tile_index >= 0 && tile_index < 256) || set_index == 0);
	assert((tile_index >= -128 && tile_index < 128) || set_index == 1);

	const size_t set_offset = set_index == 0 ? 0x1000 : 0x0;

	size_t vram_offset = set_offset;

	// A tile uses 16 bytes (2 bytes per line for 8x8 pixels).
	vram_offset += tile_index << 4;
	vram_offset += line_index << 1;
	assert(vram_offset < 0x17FF);
	assert((vram_offset & 1u) == 0);

	const uint16_t tile_line = gb__Morton2(gb->memory.vram[vram_offset], gb->memory.vram[vram_offset + 1]);
	return tile_line;
}

uint64_t
gb_GetMapTileLine(gb_GameBoy *gb, size_t map_index, size_t tile_x_index, size_t y_index)
{
	assert(map_index == 0 || map_index == 1);
	assert(tile_x_index < 32);
	assert(y_index < 8 * 32);

	(void)gb;
	return 0;
}

uint32_t
gb_MagFramebufferSizeInBytes(gb_MagFilter mag_filter)
{
	const uint32_t num_pixels = GB_FRAMEBUFFER_WIDTH * GB_FRAMEBUFFER_HEIGHT;

	switch (mag_filter)
	{
	case GB_MAG_FILTER_NONE:
		return num_pixels;
	case GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X:
	case GB_MAG_FILTER_XBR2:
		return num_pixels * (2 * 2);
	case GB_MAG_FILTER_SCALE3X_ADVMAME3X_SCALEF:
		return num_pixels * (3 * 3);
	case GB_MAG_FILTER_SCALE4X_ADVMAME4X:
		// Needs storage for the temporary intermediate buffer. Smarter people might
		// be able to do it in place.
		return gb_MagFramebufferSizeInBytes(GB_MAG_FILTER_EPX_SCALE2X_ADVMAME2X) + num_pixels * (4 * 4);
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

static inline uint8_t
gb__SrcPixel(const gb_Framebuffer input, int x, int y)
{
	x = CLAMP(x, 0, input.width - 1);
	y = CLAMP(y, 0, input.height - 1);
	return input.pixels[y * input.width + x];
}

// TODO(stefalie): None of the magnification filters below are optimized/vectorized.
// Often reading pixel values can be done in the outer loop and be reused in subsequent
// iterations of the inner loop.

// Uses notation from:
// McGuire, Gagiu; 2021; MMPX Style-Preserving Pixel-Art Magnification)
// Also see: https://www.scale2x.it
static gb_Framebuffer
gb__MagFramebufferEpxScale2xAdvMame2x(const gb_Framebuffer input, uint8_t *pixels)
{
	for (int y = 0; y < input.height; ++y)
	{
		for (int x = 0; x < input.width; ++x)
		{
			const uint8_t B = gb__SrcPixel(input, x + 0, y - 1);
			const uint8_t D = gb__SrcPixel(input, x - 1, y + 0);  // Read in outer loop, and then as prev E.
			const uint8_t E = gb__SrcPixel(input, x + 0, y + 0);  // Read in outer loop, and then as prev F.
			const uint8_t F = gb__SrcPixel(input, x + 1, y + 0);
			const uint8_t H = gb__SrcPixel(input, x + 0, y + 1);

			uint8_t J = E, K = E, L = E, M = E;

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
			pixels[dst_idx++] = J;
			pixels[dst_idx] = K;
			dst_idx += 2 * input.width - 1;
			pixels[dst_idx++] = L;
			pixels[dst_idx] = M;
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
gb__MagFramebufferScale3xAdvMame3xScaleF(gb_Framebuffer input, uint8_t *pixels)
{
	for (int y = 0; y < input.height; ++y)
	{
		for (int x = 0; x < input.width; ++x)
		{
			const uint8_t A = gb__SrcPixel(input, x - 1, y - 1);
			const uint8_t B = gb__SrcPixel(input, x + 0, y - 1);
			const uint8_t C = gb__SrcPixel(input, x + 1, y - 1);
			const uint8_t D = gb__SrcPixel(input, x - 1, y + 0);
			const uint8_t E = gb__SrcPixel(input, x + 0, y + 0);
			const uint8_t F = gb__SrcPixel(input, x + 1, y + 0);
			const uint8_t G = gb__SrcPixel(input, x - 1, y + 1);
			const uint8_t H = gb__SrcPixel(input, x + 0, y + 1);
			const uint8_t I = gb__SrcPixel(input, x + 1, y + 1);

			uint8_t _1 = E, _2 = E, _3 = E, _4 = E, _5 = E, _6 = E, _7 = E, _8 = E, _9 = E;

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
			pixels[dst_idx++] = _1;
			pixels[dst_idx++] = _2;
			pixels[dst_idx] = _3;
			dst_idx += 3 * input.width - 2;
			pixels[dst_idx++] = _4;
			pixels[dst_idx++] = _5;
			pixels[dst_idx] = _6;
			dst_idx += 3 * input.width - 2;
			pixels[dst_idx++] = _7;
			pixels[dst_idx++] = _8;
			pixels[dst_idx] = _9;
		}
	}

	return (gb_Framebuffer){
		.width = 3 * input.width,
		.height = 3 * input.height,
		.pixels = pixels,
	};
}

static inline gb_Framebuffer
gb__MagFramebufferScale4xAdvMame4x(const gb_Framebuffer input, uint8_t *pixels)
{
	// Run Scale2x twice.
	uint8_t *pixels2x = pixels + (4 * input.width) * (4 * input.height);
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
// The Yuv difference is simplified since for monochrome Yuv == (gray, 0, 0).
//
// uint8_t b = abs(((a & BLUE_MASK)  >> 16) - ((b & BLUE_MASK)  >> 16));
// uint8_t g = abs(((a & GREEN_MASK) >>  8) - ((b & GREEN_MASK) >>  8));
// uint8_t r = abs( (a & RED_MASK)          -  (b & RED_MASK)         );
// float y = fabsf( 0.299 * r + 0.587 * g + 0.114 * b);
// float u = fabsf(-0.169 * r - 0.331 * g + 0.500 * b);
// float v = fabsf( 0.500 * r - 0.419 * g - 0.081 * b);
// uint32_t yuv_diff = 48 * y + 7 * u + 6 * v;
#define XBR_THRESHHOLD_Y 48
#define XBR_DIFF(a, b) (XBR_THRESHHOLD_Y * ((uint32_t)abs(a - b)))

#define XBR_EQ(a, b) (abs(a - b) <= XBR_THRESHHOLD_Y)

// The interpolation in the original C version with 4 channels is ingenious.
// It's split up into 2 parts so that always 2 channels are interpolated at once
// and the gaps inbetween are used to prevent spill into the higher up part.
// For our single monotone channel it's easier, but we have to cast to a wider
// type so that the value can temporarily overflow.
#define XBR_INTERP(a, b, m, s) \
	(0xFF & ((0xFF & (uint16_t)a) + ((((0xFF & (uint16_t)b) - (0xFF & (uint16_t)a)) * m) >> s)))
#define XBR_INTERP_32(a, b) XBR_INTERP(a, b, 1, 3)
#define XBR_INTERP_64(a, b) XBR_INTERP(a, b, 1, 2)
#define XBR_INTERP_128(a, b) XBR_INTERP(a, b, 1, 1)
#define XBR_INTERP_192(a, b) XBR_INTERP(a, b, 3, 2)
#define XBR_INTERP_224(a, b) XBR_INTERP(a, b, 7, 3)

static inline void
gb__XbrFilter2(uint8_t E, uint8_t I, uint8_t H, uint8_t F, uint8_t G, uint8_t C, uint8_t D, uint8_t B, uint8_t F4,
		uint8_t I4, uint8_t H5, uint8_t I5, int N1, int N2, int N3, uint8_t *E_out)
{
	if (E != H && E != F)
	{
		const uint32_t e = XBR_DIFF(E, C) + XBR_DIFF(E, G) + XBR_DIFF(I, H5) + XBR_DIFF(I, F4) + (4 * XBR_DIFF(H, F));
		const uint32_t i = XBR_DIFF(H, D) + XBR_DIFF(H, I5) + XBR_DIFF(F, I4) + XBR_DIFF(F, B) + (4 * XBR_DIFF(E, I));
		if (e <= i)
		{
			const uint8_t px = XBR_DIFF(E, F) <= XBR_DIFF(E, H) ? F : H;
			if (e < i &&
					((!XBR_EQ(F, B) && !XBR_EQ(H, D)) || (XBR_EQ(E, I) && (!XBR_EQ(F, I4) && !XBR_EQ(H, I5))) ||
							XBR_EQ(E, G) || XBR_EQ(E, C)))
			{
				const uint32_t ke = XBR_DIFF(F, G);
				const uint32_t ki = XBR_DIFF(H, C);
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
gb__MagFramebufferXbr2(const gb_Framebuffer input, uint8_t *pixels)
{
	const int nl = input.width * 2;

	for (int y = 0; y < input.height; ++y)
	{
		// Output
		uint8_t *E_out = pixels + y * input.width * 2 * 2;

		const uint8_t *row0 = input.pixels + (y - 2) * input.width - 2;
		const uint8_t *row1 = row0 + input.width;
		const uint8_t *row2 = row1 + input.width;
		const uint8_t *row3 = row2 + input.width;
		const uint8_t *row4 = row3 + input.width;

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
		else if (y == input.width - 2)
		{
			row4 = row3;
		}
		else if (y == input.width - 1)
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

			const uint8_t A0 = row1[prev2];
			const uint8_t D0 = row2[prev2];
			const uint8_t G0 = row3[prev2];

			const uint8_t A1 = row0[prev];
			const uint8_t A = row1[prev];
			const uint8_t D = row2[prev];
			const uint8_t G = row3[prev];
			const uint8_t G5 = row4[prev];

			const uint8_t B1 = row0[2];
			const uint8_t B = row1[2];
			const uint8_t E = row2[2];
			const uint8_t H = row3[2];
			const uint8_t H5 = row4[2];

			const uint8_t C1 = row0[next];
			const uint8_t C = row1[next];
			const uint8_t F = row2[next];
			const uint8_t I = row3[next];
			const uint8_t I5 = row4[next];

			const uint8_t C4 = row1[next2];
			const uint8_t F4 = row2[next2];
			const uint8_t I4 = row3[next2];

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
gb_MagFramebuffer(const gb_GameBoy *gb, gb_MagFilter mag_filter, uint8_t *pixels)
{
	const gb_Framebuffer input = {
		.width = GB_FRAMEBUFFER_WIDTH,
		.height = GB_FRAMEBUFFER_HEIGHT,
		.pixels = gb->framebuffer.pixels,
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
