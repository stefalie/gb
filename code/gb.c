// Copyright (C) 2021 Stefan Lienhard

#include "gb.h"
#include <stdio.h>

void
GB_Init(GB_GameBoy* gb)
{
	(void)gb;
	printf("Hello GB!\n");
}

// Dummy image to test image mag filters.
// TODO(stefalie): Remove once the emulator can actually generate images.
// Converted with: https://www.digole.com/tools/PicturetoC_Hex_converter.php
static uint8_t gb_tetris_splash_screen[144][160] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8,
	0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xf8, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x78, 0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x78, 0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x78, 0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x1e, 0xfc, 0x04, 0x02, 0x03, 0x00, 0x80, 0x0c, 0x04, 0x02, 0x03, 0x00, 0x83,
	0xfe, 0x01, 0xe4, 0x00, 0x41, 0x77, 0x78, 0x1e, 0xfd, 0xf5, 0xf2, 0xf3, 0x7c, 0xdf, 0xcd, 0xf5, 0xf2, 0xf3, 0x7c,
	0xb8, 0xfe, 0xf9, 0xcd, 0xfe, 0x77, 0x27, 0x78, 0x1e, 0xfd, 0xed, 0xf3, 0x73, 0x7c, 0xef, 0xcd, 0xed, 0xf3, 0x73,
	0x7c, 0xbe, 0x7e, 0xf9, 0xa6, 0xfe, 0x77, 0x57, 0x78, 0x1e, 0xfd, 0xdd, 0xf3, 0xb3, 0x7c, 0xf7, 0xcd, 0xdd, 0xf3,
	0xb3, 0x7c, 0xbf, 0x3e, 0xf9, 0xb3, 0x7e, 0x77, 0x77, 0x78, 0x1e, 0xfd, 0xdd, 0xf3, 0xb3, 0x7c, 0xfb, 0xcd, 0xdd,
	0xf3, 0xb3, 0x7c, 0xbf, 0xbe, 0xf9, 0x73, 0xbe, 0x77, 0x77, 0x78, 0x1e, 0xfd, 0xbd, 0xf3, 0xd3, 0x7c, 0xfd, 0xcd,
	0xbd, 0xf3, 0xd3, 0x7c, 0xbf, 0x9e, 0xf9, 0x79, 0xbe, 0x7f, 0xff, 0x78, 0x1e, 0xfd, 0xbd, 0xf3, 0xd3, 0x7c, 0xfe,
	0xcd, 0xbd, 0xf3, 0xd3, 0x7c, 0xbf, 0x9e, 0xf9, 0x7c, 0xde, 0x7f, 0xff, 0x78, 0x1e, 0xfd, 0x7d, 0xf3, 0xe3, 0x7c,
	0xff, 0x4d, 0x7d, 0xf3, 0xe3, 0x7c, 0xbf, 0x9e, 0xf9, 0x7c, 0xee, 0x7f, 0xff, 0x78, 0x1e, 0xfd, 0x7d, 0xf3, 0xe3,
	0x7c, 0xff, 0x8d, 0x7d, 0xf3, 0xe3, 0x7c, 0xbf, 0x9e, 0xf9, 0x7e, 0x6e, 0x7f, 0xff, 0x78, 0x1e, 0xfc, 0xfd, 0xf3,
	0xf3, 0x7c, 0xfe, 0xcc, 0xfd, 0xf3, 0xf3, 0x7c, 0xbf, 0x9e, 0xf9, 0xbf, 0x36, 0x7f, 0xff, 0x78, 0x1e, 0xfc, 0xfd,
	0xf3, 0xf3, 0x7c, 0xfc, 0xec, 0xfd, 0xf3, 0xf3, 0x7c, 0xbf, 0x9e, 0xf9, 0xbf, 0x3a, 0x7f, 0xff, 0x78, 0x1e, 0xfd,
	0xfd, 0xf3, 0xfb, 0x7c, 0xf8, 0xfd, 0xfd, 0xf3, 0xfb, 0x7c, 0xbf, 0x9e, 0xf9, 0xbf, 0x9c, 0x7f, 0xff, 0x78, 0x1e,
	0xff, 0xfd, 0xf3, 0xff, 0x7c, 0xf4, 0xff, 0xfd, 0xf3, 0xff, 0x7c, 0xbf, 0x3e, 0xf9, 0xdf, 0xcc, 0x7f, 0xff, 0x78,
	0x1e, 0xff, 0xfd, 0xf3, 0xff, 0x7c, 0xec, 0xff, 0xfd, 0xf3, 0xff, 0x7c, 0xbf, 0x3e, 0xf9, 0xdf, 0xe6, 0x7f, 0xff,
	0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xc0, 0xff, 0xfc, 0x03, 0xff, 0x00, 0x80, 0x7e, 0x01, 0xe0, 0x07, 0x7f,
	0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0x80, 0xff, 0xfc, 0x03, 0xff, 0x00, 0x80, 0xfe, 0x01, 0xf0, 0x03,
	0xff, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xc0, 0xff, 0xfc, 0x03, 0xff, 0x00, 0x87, 0xfe, 0x01, 0xb8,
	0x01, 0xff, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xe0, 0xff, 0xfc, 0x03, 0xff, 0x00, 0x9f, 0xfe, 0x01,
	0x98, 0x00, 0xff, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xf0, 0xff, 0xfc, 0x03, 0xff, 0x00, 0x8f, 0xfe,
	0x01, 0x8c, 0x00, 0x7f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xf8, 0xff, 0xfc, 0x03, 0xff, 0x00, 0x8f,
	0xfe, 0x01, 0x8e, 0x00, 0x7f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xfc, 0xef, 0xfc, 0x03, 0xff, 0x00,
	0x87, 0xfe, 0x01, 0x86, 0x00, 0x3f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xfe, 0xcf, 0xfc, 0x03, 0xff,
	0x00, 0x83, 0xfe, 0x01, 0x83, 0x00, 0x3f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xff, 0x8f, 0xfc, 0x03,
	0xff, 0x00, 0x81, 0xfe, 0x01, 0x81, 0x80, 0x1f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xff, 0x0f, 0xfc,
	0x03, 0xff, 0x00, 0x81, 0xfe, 0x01, 0x81, 0x80, 0x1f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xfe, 0x0f,
	0xfc, 0x03, 0xff, 0x00, 0x80, 0xfe, 0x01, 0x80, 0xc0, 0x1f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00, 0xfc,
	0x0f, 0xfc, 0x03, 0xff, 0x00, 0x80, 0x7e, 0x01, 0x80, 0x60, 0x1f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff, 0x00,
	0xf8, 0x0f, 0xfc, 0x03, 0xff, 0x00, 0x80, 0x3e, 0x01, 0x80, 0x70, 0x1f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03, 0xff,
	0x00, 0xf0, 0x0f, 0xfc, 0x03, 0xff, 0x00, 0x80, 0x3e, 0x01, 0x80, 0x30, 0x1f, 0xff, 0x78, 0x1e, 0xff, 0xfc, 0x03,
	0xff, 0x00, 0xe0, 0x0f, 0xfc, 0x03, 0xff, 0x00, 0x80, 0x1e, 0x01, 0x80, 0x18, 0x3f, 0xff, 0x78, 0x1e, 0xff, 0xfc,
	0x03, 0xff, 0x00, 0xc0, 0x0f, 0xfc, 0x03, 0xff, 0x00, 0x80, 0x0e, 0x01, 0x80, 0x0c, 0x3f, 0xff, 0x78, 0x1e, 0xff,
	0xfc, 0x03, 0xff, 0x00, 0x80, 0x0f, 0xfc, 0x03, 0xff, 0x00, 0x80, 0x06, 0x01, 0x80, 0x08, 0x7f, 0xff, 0x78, 0x1e,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78,
	0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x78, 0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x78, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x78, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xf8, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x18, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xbf, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xdf, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0x9f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x3f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f,
	0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd,
	0x3f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xfd, 0x3f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xfc, 0x3f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfa, 0x9f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xf6, 0x8f, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xed, 0xa7, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xd9, 0x23, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb3, 0x31, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x67, 0x34, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xc6, 0x32, 0x7f, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x8e, 0x3a, 0x3f, 0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0x1e, 0x79, 0x1f, 0xfd, 0xbf, 0x00, 0x00, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf6, 0x3c, 0x78, 0x8f, 0xfd, 0xbf, 0x00, 0x00, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xec, 0x3c, 0x78, 0x47, 0xfb, 0xdf, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd8, 0x78, 0x7c, 0x63, 0xfb, 0xdf, 0x00,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd0, 0x78, 0x7c, 0x33, 0xfb, 0x9f,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb0, 0xf8, 0x7c, 0x31, 0xfd,
	0x3f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xb0, 0xf0, 0x7e, 0x19,
	0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0x60, 0xf0, 0x7e,
	0x08, 0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0x60, 0xf0,
	0x3e, 0x0c, 0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0x60,
	0xf0, 0x3f, 0x0c, 0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xff,
	0x60, 0xf0, 0x1f, 0x0c, 0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xf1,
	0xff, 0x60, 0xf0, 0x1f, 0x0c, 0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc7,
	0xf1, 0xff, 0x60, 0xf8, 0x1f, 0x08, 0xfe, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x83, 0xf1, 0xf7, 0xa0, 0x78, 0x0f, 0x19, 0xfd, 0x3f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xfb, 0x01, 0xe0, 0xeb, 0xb0, 0x7c, 0x0f, 0x11, 0xfd, 0x3f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xfb, 0x01, 0xe0, 0xf7, 0xd0, 0x3c, 0x0e, 0x33, 0xbc, 0x3f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfb, 0x01, 0xe0, 0xf7, 0xd8, 0x1e, 0x0e, 0x23, 0x1a, 0x9f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfb, 0x83, 0xe0, 0xe3, 0xee, 0x0f, 0x06, 0x47, 0xb6, 0x8f, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xf1, 0xff, 0xe0, 0xe3, 0xb0, 0x00, 0x00, 0x0c, 0xad, 0xa7, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0x83, 0x80, 0x63, 0x0b, 0xff, 0xc0, 0x5c, 0x99, 0x23, 0x00, 0x00, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0x83, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x33, 0x31, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xcf, 0x83, 0x9f, 0xc3, 0x99, 0x81, 0x83, 0xff, 0xff, 0xff,
	0x87, 0x83, 0x9f, 0xc3, 0x99, 0x81, 0x83, 0xff, 0xff, 0x8f, 0x8f, 0x99, 0x9f, 0xb1, 0x99, 0x9f, 0x99, 0xff, 0xff,
	0xff, 0x63, 0x99, 0x9f, 0xb1, 0x99, 0x9f, 0x99, 0xff, 0xff, 0x87, 0xcf, 0x99, 0x9f, 0xb1, 0xc3, 0x83, 0x99, 0xff,
	0xff, 0xff, 0xe3, 0x99, 0x9f, 0xb1, 0xc3, 0x83, 0x99, 0xff, 0xff, 0x87, 0xcf, 0x83, 0x9f, 0x81, 0xe7, 0x9f, 0x83,
	0xff, 0xff, 0xff, 0x87, 0x83, 0x9f, 0x81, 0xe7, 0x9f, 0x83, 0xff, 0xff, 0x8f, 0xcf, 0x9f, 0x9f, 0xb1, 0xe7, 0x9f,
	0x97, 0xff, 0xff, 0xff, 0x1f, 0x9f, 0x9f, 0xb1, 0xe7, 0x9f, 0x97, 0xff, 0xff, 0x9f, 0x87, 0x9f, 0x81, 0xb1, 0xe7,
	0x81, 0x99, 0xff, 0xff, 0xff, 0x03, 0x9f, 0x81, 0xb1, 0xe7, 0x81, 0x99, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x39, 0x3f, 0xff, 0xff, 0xfe, 0x7f,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc7, 0xff, 0xff, 0xff, 0xff, 0xff, 0x19, 0x3f, 0xcf, 0xff, 0xfe,
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbb, 0xe7, 0xc3, 0xc3, 0xc3, 0xff, 0x19, 0xff, 0x87, 0xff,
	0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x45, 0xc7, 0xb1, 0xb1, 0xb1, 0xff, 0x29, 0x24, 0xcc,
	0x32, 0x70, 0x61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x5d, 0xe7, 0xb1, 0xc3, 0xb1, 0xff, 0x29, 0x22,
	0x49, 0x91, 0x26, 0x4c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x45, 0xe7, 0xc1, 0xb1, 0xc1, 0xff, 0x31,
	0x26, 0x48, 0x13, 0x26, 0x4c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbb, 0xe7, 0xf1, 0xb1, 0xf1, 0xff,
	0x31, 0x26, 0x49, 0xf3, 0x26, 0x4c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc7, 0xc3, 0xc3, 0xc3, 0xc3,
	0xff, 0x39, 0x26, 0x4c, 0x13, 0x30, 0x61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

GB_FrameBuffer
GB_GetFrameBuffer(const GB_GameBoy* gb /* GB_MagFilter mag_filter*/)
{
	(void)gb;
	return (GB_FrameBuffer){
		.width = 160,
		.height = 144,
		.pixels = (const uint8_t*)gb_tetris_splash_screen,
	};
}
