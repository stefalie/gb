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

#define ROM_HEADER_START_ADDRESS 0x100
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
	// TODO: I think this is identical.
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
		assert(!"TODO");
		const size_t rom_bank = 1;  // TODO
		const uint16_t rom_bank_0_end = 0x4000;
		return gb->rom.data[(rom_bank - 1) * rom_bank_0_end + addr];
	// VRAM
	case 0x8000:
	case 0x9000:
		assert(!"TODO");
		return gb->memory.vram[addr & 0x1FFF];
	// Switchable RAM bank
	case 0xA000:
	case 0xB000:
		assert(!"TODO");
		return gb->memory.external_ram[addr & 0x1FFF];
	// (Internal) working RAM
	case 0xC000:
	case 0xD000:
		assert(!"TODO");
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
				assert(!"TODO");
				return 0;
			}
			else  // Empty
			{
				return 0;
			}
		case 0x0F00:
			if (addr < 0xFF4C)  // I/O
			{
				assert(!"TODO");
				return 0;
			}
			else if (addr < 0xFF80)  // Empty
			{
				return 0;
			}
			else  // Zero page
			{
				assert((addr - 0xFF80) < 0x80);
				assert(!"TODO");
				return gb->memory.zero_page[addr & 0x7F];
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
	(void)gb;
	(void)addr;
	(void)value;

//	switch (addr & 0xF000)
//	{
//	// ROM bank 0 or BIOS
//	case 0x0000:
//	case 0x1000:
//	case 0x2000:
//	case 0x3000:
//		if (gb->memory.bios_mapped && addr < 0x100)
//		{
//			assert(false);
//		}
//		assert(!"TODO");
//		break;
//	// Switchable ROM bank
//	case 0x4000:
//	case 0x5000:
//	case 0x6000:
//	case 0x7000:
//		assert(!"TODO");
//		break;
//	// VRAM
//	case 0x8000:
//	case 0x9000:
//		assert(!"TODO");
//		break;
//	// Switchable RAM bank
//	case 0xA000:
//	case 0xB000:
//		assert(!"TODO");
//		gb->memory.external_ram[addr & 0x1FFF] = value;
//		break;
//	// (Internal) working RAM
//	case 0xC000:
//	case 0xD000:
//		assert(!"TODO");
//		gb->memory.wram[addr & 0x1FFF] = value;
//		break;
//	// Echo of (Internal) working RAM, I/O, zero page
//	case 0xE000:
//	case 0xF000:
//		switch (addr & 0x0F00)
//		{
//		// Echo of (Internal) working RAM
//		default:  // [0xE000, 0xFE00)
//			assert((addr - 0xE000) < 0x1E00);
//			gb->memory.wram[addr & 0x1FFF] = value;
//		case 0x0E00:
//			if (addr < 0xFEA0)  // Sprite Attrib Memory (OAM)
//			{
//				assert(!"TODO");
//			}
//			else
//			{
//				// The empty area is ignored
//				assert(!"TODO");
//			}
//			break;
//		case 0x0F00:
//			if (addr < 0xFF4C)  // I/O
//			{
//				assert(!"TODO");
//			}
//			else if (addr < 0xFF80)
//			{
//				// The empty area is ignored
//				assert(!"TODO");
//			}
//			else  // Zero page
//			{
//				assert((addr - 0xFF80) < 0x80);
//				assert(!"TODO");
//				gb->memory.zero_page[addr & 0x7F] = value;
//			}
//			break;
//		}
//		break;
//	default:
//		assert(false);
//		break;
//	}
}
// TODO: needed?
// static inline void
// gb__MemoryWriteWord(gb_GameBoy* gb, uint16_t addr, uint16_t value)
//{
//	gb__MemoryWriteByte(gb, addr, gb__Lo(value));
//	gb__MemoryWriteByte(gb, addr + 1, gb__Hi(value));
// }

void
gb_Reset(gb_GameBoy *gb)
{
	gb->memory.bios_mapped = true;

	// (see page 18 of the GameBoy CPU Manual)
	gb->cpu.a = 0x01;
	gb->cpu.f = 0xB0;
	gb->cpu.bc = 0x0013;
	gb->cpu.de = 0x00D8;
	gb->cpu.hl = 0x014D;
	gb->cpu.sp = 0xFFFE;
	gb->cpu.pc = 0x0;  // TODO: do we end up here autmatically?
	// gb->cpu.pc = 0x0100;  // TODO: do we end up here autmatically?

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
	uint8_t num_machine_cycles;
} gb__InstructionInfo;

static const gb__InstructionInfo gb__instruction_infos[256] = {
	{ "NOP", 0, 1 },

	[0x01] = { "LD BC", 2, 3 },

	[0x11] = { "LD DE", 2, 3 },

	[0x21] = { "LD HL", 2, 3 },

	[0x31] = { "LD SP", 2, 3 },
	[0x32] = { "LD (HL-), A", 0, 1 },

	[0xA0] = { "AND B", 0, 1 },
	[0xA1] = { "AND C", 0, 1 },
	[0xA2] = { "AND D", 0, 1 },
	[0xA3] = { "AND E", 0, 1 },
	[0xA4] = { "AND H", 0, 1 },
	[0xA5] = { "AND L", 0, 1 },
	[0xA6] = { "AND (HL)", 0, 2 },
	[0xA7] = { "AND A", 0, 1 },

	[0xCB] = { "PREFIX", 0, 1 },

	[0xA8] = { "XOR B", 0, 1 },
	[0xA9] = { "XOR C", 0, 1 },
	[0xAA] = { "XOR D", 0, 1 },
	[0xAB] = { "XOR E", 0, 1 },
	[0xAC] = { "XOR H", 0, 1 },
	[0xAD] = { "XOR L", 0, 1 },
	[0xAE] = { "XOR (HL)", 0, 2 },
	[0xAF] = { "XOR A", 0, 1 },

	[0xE7] = { "AND n", 1, 2 },
	[0xEE] = { "XOR n", 1, 2 },
};

gb_Instruction
gb_FetchInstruction(const gb_GameBoy *gb, uint16_t addr)
{
	gb_Instruction inst = {
		.opcode = gb_MemoryReadByte(gb, addr),
	};

	const gb__InstructionInfo info = gb__instruction_infos[inst.opcode];
	inst.num_operand_bytes = info.num_operand_bytes;
	inst.num_machine_cycles = info.num_machine_cycles;

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

// TODO: get rid of snprintf, strlen, memcpy to avoid std includes.
// should all be easy, and from snprintf you only need to know how to convert 1 byte numbers to 2-char strings.
size_t
gb_DisassembleInstruction(gb_Instruction inst, char str_buf[], size_t str_buf_len)
{
	const gb__InstructionInfo info = gb__instruction_infos[inst.opcode];
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

size_t
gb_ExecuteNextInstruction(gb_GameBoy* gb)
{
	assert(gb->rom.data);
	assert(gb->rom.num_bytes);

	if (gb->cpu.pc == 256)
	{
		gb->memory.bios_mapped = false;
	}

	const gb_Instruction inst = gb_FetchInstruction(gb, gb->cpu.pc);
	const gb__InstructionInfo info = gb__instruction_infos[inst.opcode];

	gb->cpu.pc += 1 /* opcode */ + info.num_operand_bytes;

	switch (inst.opcode)
	{
	case 0:  // NOP
		break;

	case 0x01:  // LD BC, nn
		gb->cpu.bc = inst.operand_word;
		assert(false);
		break;

	case 0x11:  // LD DE, nn
		gb->cpu.de = inst.operand_word;
		assert(false);
		break;

	case 0x21:  // LD HL, nn
		gb->cpu.hl = inst.operand_word;
		break;

	case 0x31:  // LD SP, nn
		gb->cpu.sp = inst.operand_word;
		break;
	case 0x32:  // LD (HL-), A
		gb__MemoryWriteByte(gb, gb->cpu.hl, gb->cpu.a);
		--gb->cpu.hl;
		break;

	case 0xA0:  // AND B
		gb->cpu.a &= gb->cpu.b;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;
	case 0xA1:  // AND C
		gb->cpu.a &= gb->cpu.c;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;
	case 0xA2:  // AND D
		gb->cpu.a &= gb->cpu.d;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;
	case 0xA3:  // AND E
		gb->cpu.a &= gb->cpu.e;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;
	case 0xA4:  // AND H
		gb->cpu.a &= gb->cpu.h;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;
	case 0xA5:  // AND L
		gb->cpu.a &= gb->cpu.l;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;
	case 0xA6:  // AND (HL)
		gb->cpu.a &= gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;
	case 0xA7:  // AND A
		gb->cpu.a &= gb->cpu.a;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;

	case 0xA8:  // XOR B
		gb->cpu.a ^= gb->cpu.b;
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
		break;
	case 0xA9:  // XOR C
		gb->cpu.a ^= gb->cpu.c;
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
		break;
	case 0xAA:  // XOR D
		gb->cpu.a ^= gb->cpu.d;
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
		break;
	case 0xAB:  // XOR E
		gb->cpu.a ^= gb->cpu.e;
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
		break;
	case 0xAC:  // XOR H
		gb->cpu.a ^= gb->cpu.h;
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
		break;
	case 0xAD:  // XOR L
		gb->cpu.a ^= gb->cpu.l;
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
		break;
	case 0xAE:  // XOR (HL)
		gb->cpu.a ^= gb_MemoryReadByte(gb, gb->cpu.hl);
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		assert(false);
		break;
	case 0xAF:  // XOR A
		gb->cpu.a ^= gb->cpu.a;  // Identical to: gb->cpu.a = 0
		gb__SetFlags(gb, gb->cpu.a == 0, false, false, false);
		break;

	case 0xCB:  // PREFIX
		// TODO: call extened operations in seperate function with separate switch
		assert(false);
		break;


	case 0xE7:  // AND n
		gb->cpu.a &= inst.operand_byte;
		gb__SetFlags(gb, gb->cpu.a == 0, false, true, false);
		assert(false);
		break;

	case 0xEE:  // XOR n
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
		// Revert PC
		gb->cpu.pc -= 1 /* opcode */ + info.num_operand_bytes;
		// Asserting that the return value is not -1 in the caller allows
		// implementing the instructions step by step. Whenever assert fails,
		// it will tell us which is the next instruction that needs to be
		// implemented next for the program to continue.
		return (size_t)-1;
	}

	return info.num_machine_cycles;
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
