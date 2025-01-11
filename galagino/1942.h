/* 1942.h */
//Galapico TODO: Optimise for full rate Z80s
// The half-screen sprite clipping feature isn't implemented
// Tiles aren't cropped so overscan

#ifdef CPU_EMULATION
#include "1942_rom1.h"
#include "1942_rom2.h"
#include "1942_rom1_b0.h"
#include "1942_rom1_b1.h"
#include "1942_rom1_b2.h"

unsigned char _1942_bank = 0;
unsigned char _1942_palette = 0;
unsigned short _1942_scroll = 0;
unsigned char _1942_sound_latch = 0;
unsigned char _1942_ay_addr[2];

static inline unsigned char _1942_RdZ80(unsigned short Addr) {
	if (current_cpu == 0) {
		// main CPU    
		if (Addr < 32768) return _1942_rom_cpu1[Addr];

		// CPU1 banked ROM
		if ((Addr & 0xc000) == 0x8000) {
			if (_1942_bank == 0)                         return _1942_rom_cpu1_b0[Addr - 0x8000];
			else if ((_1942_bank == 1) && (Addr < 0xb000)) return _1942_rom_cpu1_b1[Addr - 0x8000];
			else if (_1942_bank == 2)	                  return _1942_rom_cpu1_b2[Addr - 0x8000];
		}

		// RAM mapping
		// 0000-0fff    4k main CPU work RAM
		// 1000-17ff    2k fgvram
		// 1800-1fff    2k audio CPU work RAM
		// 2000-23ff    1k bgvram
		// 2400-247f   128 spriteram
		// if(Addr == 0xe0a5) return 2;  // invincibility

		// 4k main RAM
		if ((Addr & 0xf000) == 0xe000)
			return memory[Addr - 0xe000];

		// 128 byte spriteram
		if ((Addr & 0xff80) == 0xcc00)
			return memory[Addr - 0xcc00 + 0x2400];

		// 2k fgvram
		if ((Addr & 0xf800) == 0xd000)
			return memory[Addr - 0xd000 + 0x1000];

		// 1k bgvram
		if ((Addr & 0xfc00) == 0xd800)
			return memory[Addr - 0xd800 + 0x2000];

		// ------------ dip/ctrl io -----------

		// COIN1,COIN2,X,SERVICE1,X,START2,START1
		if (Addr == 0xc000) {
			unsigned char keymask = buttons_get();
			unsigned char retval = 0xff;
			static unsigned char last_coin = 0;
			if (keymask & BUTTON_COIN && !last_coin)  retval &= ~0x80;
			if (keymask & BUTTON_START) retval &= ~0x01;
			last_coin = keymask & BUTTON_COIN;
			return retval;
		}

		// P1: X,X,B2,B1,U,D,L,R
		if (Addr == 0xc001) {
			unsigned char keymask = buttons_get();
			unsigned char retval = 0xff;
			if (keymask & BUTTON_RIGHT) retval &= ~0x01;
			if (keymask & BUTTON_LEFT)  retval &= ~0x02;
			if (keymask & BUTTON_DOWN)  retval &= ~0x04;
			if (keymask & BUTTON_UP)    retval &= ~0x08;
			if (keymask & BUTTON_FIRE)  retval &= ~0x10;
			if (keymask & BUTTON_START) retval &= ~0x20;
			return retval;
		}

		// P2: X,X,B2,B1,U,D,L,R
		if (Addr == 0xc002) return 0xff;

		// DIP A
		if (Addr == 0xc003) return ~_1942_DIP_A;

		// DIP B
		if (Addr == 0xc004) return ~_1942_DIP_B;

	}
	else {
		// second/audio CPU
		if (Addr < 16384) return _1942_rom_cpu2[Addr];

		// 2k audio ram
		if ((Addr & 0xf800) == 0x4000)
			return memory[Addr - 0x4000 + 0x1800];

		// read sound command from latch from main CPU
		if (Addr == 0x6000)
			return _1942_sound_latch;
	}

	return 0xff;
}

static inline void _1942_WrZ80(unsigned short Addr, unsigned char Value) {
	// RAM mapping
	// 0000-0fff    4k main CPU work RAM
	// 1000-17ff    2k fgvram
	// 1800-1fff    2k audio CPU work RAM
	// 2000-23ff    1k bgvram
	// 2400-247f   128 spriteram

	if (current_cpu == 0) {
		// 4k main RAM
		if ((Addr & 0xf000) == 0xe000) {
			memory[Addr - 0xe000] = Value;
			return;
		}

		// 128 byte spriteram
		if ((Addr & 0xff80) == 0xcc00) {
			memory[Addr - 0xcc00 + 0x2400] = Value;
			return;
		}

		// 2k fgvram
		if ((Addr & 0xf800) == 0xd000) {
			// printf("#%d@%04x fgvram write %04x = %02x\n", current_cpu, cpu[current_cpu].PC.W, Addr, Value);
			memory[Addr - 0xd000 + 0x1000] = Value;
			return;
		}

		// 1k bgvram
		if ((Addr & 0xfc00) == 0xd800) {
			memory[Addr - 0xd800 + 0x2000] = Value;
			return;
		}

		if ((Addr & 0xfff0) == 0xc800) {

			if (Addr == 0xc800) {
				_1942_sound_latch = Value;
				return;
			}

			if (Addr == 0xc802) {
				_1942_scroll = Value | (_1942_scroll & 0xff00);
				return;
			}

			if (Addr == 0xc803) {
				_1942_scroll = (Value << 8) | (_1942_scroll & 0xff);
				return;
			}

			if (Addr == 0xc804) {
				// bit 7: flip screen
				// bit 4: cpu b reset
				// bit 0: coin counter
				sub_cpu_reset = Value & 0x10;

				if (sub_cpu_reset) {
					current_cpu = 1;
					ResetZ80(&cpu[1]);
				}
				return;
			}

			if (Addr == 0xc805) {
				_1942_palette = Value;
				return;
			}

			if (Addr == 0xc806) {
				_1942_bank = Value;
				return;
			}
		}

	}
	else {
		// 2k audio ram
		if ((Addr & 0xf800) == 0x4000) {
			memory[Addr - 0x4000 + 0x1800] = Value;
			return;
		}

		// AY's
		if ((Addr & 0x8000) == 0x8000) {
			int ay = (Addr & 0x4000) != 0;

			if (Addr & 1) soundregs[16 * ay + _1942_ay_addr[ay]] = Value;
			else   	   _1942_ay_addr[ay] = Value & 15;

			return;
		}
	}
}


static inline void _1942_run_frame(void) {
	game_started = 1;

	for (char f = 0; f < 4; f++) {
		for (int i = 0; i < INST_PER_FRAME_1942 / 3; i++) {
			current_cpu = 0;
			StepZ80(&cpu[0]); StepZ80(&cpu[0]); StepZ80(&cpu[0]); StepZ80(&cpu[0]);
			if (!sub_cpu_reset) {
				current_cpu = 1;
				StepZ80(&cpu[1]); StepZ80(&cpu[1]); StepZ80(&cpu[1]);
			}
		}

		// generate interrupts, main CPU two times per frame
		if (f & 1) {
			current_cpu = 0;
			IntZ80(&cpu[0], 0xc7 | ((f & 2) ? (1 << 3) : (2 << 3)));
		}

		// sound CPU four times per frame
		if (!sub_cpu_reset) {
			// the audio CPU updates the AY registers exactly one time
			// during each interrupt.
			current_cpu = 1;
			IntZ80(&cpu[1], INT_RST38);
		}
	}
}
#endif // CPU_EMULATION

#ifdef IO_EMULATION

#ifndef SINGLE_MACHINE
#include "1942_logo.h"
#endif
#include "1942_character_cmap.h"
#include "1942_spritemap.h"
#include "1942_tilemap.h"
#include "1942_charmap.h"
#include "1942_sprite_cmap.h"
#include "1942_tile_cmap.h"

extern unsigned char _1942_palette;
extern unsigned short _1942_scroll;

static inline void _1942_prepare_frame(void) {
	// Do all the preparations to render a screen.

	/* preprocess sprites */
	active_sprites = 0;
	for (int idx = 0; idx < 32 && active_sprites < 124; idx++) {
		struct sprite_S spr;
		unsigned char* sprite_base_ptr = memory + 0x2400 + 4 * (31 - idx);

		if (sprite_base_ptr[3] && sprite_base_ptr[2]) {
			// unlike all other machine, 1942 has 512 sprites
			spr.code = (sprite_base_ptr[0] & 0x7f) +
				4 * (sprite_base_ptr[1] & 0x20);

			spr.color = sprite_base_ptr[1] & 0x0f;
			spr.x = sprite_base_ptr[2] - 16;
			spr.y = 256 - (sprite_base_ptr[3] - 0x10 * (sprite_base_ptr[1] & 0x10));

			// store 9th code bit in flags
			spr.flags = (sprite_base_ptr[0] & 0x80) >> 7;

			// 16 .. 23 only on left screen
			if ((idx >= 8) && (idx < 16))
				spr.flags |= 0x80;   // flag for "left screen half only"

			if ((idx >= 0) && (idx < 8))
				spr.flags |= 0x40;   // flag for "right screen half only"

			  // attr & 0xc0: double and quad height
			if ((spr.y > -16) && (spr.y < 288) &&
				(spr.x > -16) && (spr.x < 224)) {

				// save sprite in list of active sprites
				sprite[active_sprites++] = spr;

				// handle double and quad sprites
				if (sprite_base_ptr[1] & 0xc0) {
					spr.x += 16;
					spr.code += 1;
					if (spr.x < 224) {
						sprite[active_sprites++] = spr;
						if (sprite_base_ptr[1] & 0x80) {
							spr.x += 16;
							spr.code += 1;
							if (spr.x < 224) {
								sprite[active_sprites++] = spr;
								spr.x += 16;
								spr.code += 1;
								if (spr.x < 224)
									sprite[active_sprites++] = spr;
							}
						}
					}
				}
			}
		}
	}
}

//#define CHUNKSIZE 16
static inline void _1942_render_foreground_raster(unsigned short chunk)
{
	int y_block = chunk;
	//    for (int y_block = 0; y_block < TV_HEIGHT / 8; y_block++)
	{
		for (int x_block = 2; x_block < (TV_WIDTH / 8) - 6; x_block++)
		{
			int row = x_block;
			int col = y_block;

			unsigned short addr = tileaddr[35 - row][col];

			unsigned short chr = memory[0x1000 + addr];
			if (chr != 0x30)  // empty space
			{

				unsigned char attr = memory[0x1400 + addr];
				if (attr & 0x80)
					chr += 256;

				const unsigned short* tile = sr_02_f2[chr];

				// colorprom sb-0.f1 contains 64 color groups
				const unsigned short* colors = _1942_colormap_chars[attr & 63];

				unsigned short* ptr = frame_buffer + CRT_ROW_OFFSET + (8 * row + (8 * col * TV_WIDTH));


				// 8 pixel rows per tile
				for (char r = 0; r < 8; r++, ptr += (TV_WIDTH - 8))
				{
					unsigned short pix = *tile++;
					// 8 pixel columns per tile
					for (char c = 0; c < 8; c++, pix >>= 2) {
						if (pix & 3) *ptr = colors[pix & 3];
						ptr++;
					}
				}
			}
		}
	}
}

static inline void _1942_render_sprite_raster(unsigned short chunk)
{

	// boundaries for sprites to be on this chunk
	int upper_sprite_bound = CHUNKSIZE * (chunk + 1);
	int lower_sprite_bound = CHUNKSIZE * chunk;
	int chunk_vert_start = CHUNKSIZE * chunk;

	for (unsigned char s = 0; s < active_sprites; s++)
	{
		int x_vert = 224 - sprite[s].x - 16;

		// check if sprite is visible on this row
		if ((x_vert < upper_sprite_bound) && ((x_vert + 16) > lower_sprite_bound))
		{
			const unsigned long* spr = _1942_sprites[sprite[s].code + 256 * (sprite[s].flags & 1)];

			const unsigned short* colors = _1942_colormap_sprites[4 * (sprite[s].color & 15)];
			//       const unsigned short* colors = _1942_colormap_sprites[0];
			short sprite_chunk_offset = x_vert - chunk_vert_start;

			int lines2draw = CHUNKSIZE - sprite_chunk_offset;
			if (lines2draw > 16)
				lines2draw = 16;

			// If we're onto the next line for the sprite...
			if (sprite_chunk_offset < 0)
			{
				lines2draw = 16 + sprite_chunk_offset;
			}

			// check which sprite line to begin with
			unsigned short startline = 0;
			if (sprite_chunk_offset < 0)
			{
				startline = -sprite_chunk_offset;
			}

			spr += (startline * 2);
			// calculate pixel lines to paint  
			int scanline_start = chunk_vert_start;
			if (sprite_chunk_offset >= 0)
				scanline_start = (chunk * CHUNKSIZE) + sprite_chunk_offset; // i.e. sprite[].x

			unsigned short* ptr = frame_buffer + CRT_ROW_OFFSET + ((scanline_start)*TV_WIDTH) + sprite[s].y;
			unsigned short* halfscreen_ptr = frame_buffer + TV_WIDTH * (TV_HEIGHT/2);
			

			// Bottom half only sprite
			if (sprite[s].flags & 0x80)
			{
				// Is this breaking the top half somehow?
				for (char r = 0; r < lines2draw; r++, ptr += (TV_WIDTH - 16))
				{
					unsigned long pix = *spr++;

					if (ptr < halfscreen_ptr)
					{
						pix = 0xFFFFFFFF;
					}
					for (char c = 0; c < 8; c++, pix >>= 4) {
						if ((pix & 15) != 15) *ptr = colors[pix & 15];
						ptr++;
					}

					pix = *spr++;
					if (ptr < halfscreen_ptr)
					{
						pix = 0xFFFFFFFF;
					}
					for (char c = 0; c < 8; c++, pix >>= 4) {
						if ((pix & 15) != 15) *ptr = colors[pix & 15];
						ptr++;
					}

				}
			}
			// Top half only sprite"
			else if (sprite[s].flags & 0x40)
			{
				for (char r = 0; r < lines2draw; r++, ptr += (TV_WIDTH - 16))
				{
					unsigned long pix = *spr++;

					if (ptr > halfscreen_ptr)
					{
						pix = 0xFFFFFFFF;
					}
					for (char c = 0; c < 8; c++, pix >>= 4) {
						if ((pix & 15) != 15) *ptr = colors[pix & 15];
						ptr++;
					}

					pix = *spr++;
					if (ptr > halfscreen_ptr)
					{
						pix = 0xFFFFFFFF;
					}
					for (char c = 0; c < 8; c++, pix >>= 4) {
						if ((pix & 15) != 15) *ptr = colors[pix & 15];
						ptr++;
					}

				}
			}

			else
			{
				for (char r = 0; r < lines2draw; r++, ptr += (TV_WIDTH - 16))
				{

					unsigned long pix = *spr++;
					for (char c = 0; c < 8; c++, pix >>= 4) {
						if ((pix & 15) != 15) *ptr = colors[pix & 15];
						ptr++;
					}

					pix = *spr++;
					for (char c = 0; c < 8; c++, pix >>= 4) {
						if ((pix & 15) != 15) *ptr = colors[pix & 15];
						ptr++;
					}
				}
			}
		}
	}
}


static inline void _1942_render_tile_raster(unsigned short chunk)
{
	unsigned short* ptr = &frame_buffer[0]; // was column
	int line = 0;
	int col = 0;
	static int slowcount = 0;
	slowcount++;
	int yoffset = (slowcount >> 5) & 15;
	for (int y_block = 0; y_block < TV_HEIGHT / 16; y_block++)
	{
		for (int x_block = 0; x_block < GAME_WIDTH / 16 -1; x_block++)
		{
			line = (16 + ((TV_WIDTH / 16 - x_block - 5) * 16) + _1942_scroll) & 511;
			int y_max = (TV_HEIGHT / 16) - 1;
			int pixels_off_grid = (line - 1) & 0x0F;

			unsigned short* ptr = &frame_buffer[CRT_ROW_OFFSET + x_block * 16 + (y_block * 16 * TV_WIDTH) + pixels_off_grid]; // was column


			unsigned short addr = 0x2000 + 32 * (((line - 1) & 511) / 16) + 1;
			unsigned char attr = memory[addr + (y_max - y_block) + 16];
			unsigned short chr = memory[addr + (y_max - y_block)] + ((attr & 0x80) << 1);
			const unsigned long* tile = sr_08_a1[(attr >> 5) & 3][chr];// +2 * yoffset;
			const unsigned short* colors = _1942_colormap_tiles[_1942_palette][attr & 31];
			
			if (x_block == 0)
			{
				//This is sub-optimal. You could probably plot empty black tiles off the edges of the screen for less cpu cycles
				// But we're not exactly short of cycles for plotting video
				pixels_off_grid = 16 - pixels_off_grid;

				unsigned int top_mask[9] = { 0xFFFFFFFF, 0xFFFFFFF0, 0xffffff00, 0xfffff000, 0xffff0000, 0xfff00000, 0xff000000, 0xf0000000, 0x00000000 };
				
				if (pixels_off_grid < 8)
				{	
					//  Mask off the first 8 pixels
					// second 8 pixels as-is
					for (int y = 0; y < 16; y++, ptr += TV_WIDTH - 16)
					{
						unsigned long pix = *tile++;
						pix &= top_mask[pixels_off_grid];
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7];
						pix = *tile++;
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7];
					}	
				}
				else
				{
					for (int y = 0; y < 16; y++, ptr += TV_WIDTH - 16)
					{
						// Clear the first 8 pixels
						//  Mask off the second 32-bit word						
						unsigned long pix = *tile++;
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = 0; // First 8 pixels are zero
						pix = *tile++;	
						pix &= top_mask[pixels_off_grid - 8];
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7];
					
					}

				}
			}
			else if (x_block == GAME_WIDTH / 16 - 2)
			{
				unsigned int top_mask[9] = { 0x00000000, 0x0000000F, 0x000000FF, 0x00000FFF, 0x0000ffff, 0x000fffff, 0x00ffffff, 0x0fffffff, 0xffffffff };

				pixels_off_grid = 16 -pixels_off_grid;

				if (pixels_off_grid < 8)
				{
					// Mask off first 8 pixels
					// second 8 pixels clear
					for (int y = 0; y < 16; y++, ptr += TV_WIDTH - 16)
					{
						unsigned long pix = *tile++;
						pix &= top_mask[pixels_off_grid];
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7];
						pix = *tile++;
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = 0;
					}

				}
				else
				{
					// First 8 pixels as-is
					// Mask second 8 pixels

					for (int y = 0; y < 16; y++, ptr += TV_WIDTH - 16)
					{
						
						unsigned long pix = *tile++;
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7]; // First 8 pixels are zero
						
						pix = *tile++;
						pix &= top_mask[(pixels_off_grid - 8)];
						for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7];	
					}
				}
			}
			else
			{
				// A normal tile.
				// Draw all this tile out
				for (int y = 0; y < 16; y++, ptr += TV_WIDTH - 16)
				{
					unsigned long pix = *tile++;
					for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7];
					pix = *tile++;
					for (char c = 0; c < 8; c++, pix >>= 4) *ptr++ = colors[pix & 7];
				}
			}
			
		}
	}
}


static inline void _1942_render_frame_raster()
{

	// Raster beam scan, divided into sprite-sized chunks (x axis) 
	// Means I can ditch the framebuffer(s) if RAM gets low
	// X and Y are in vertical monitor-world, so remember y is horizontal beam and x is vertical (aargh)
	// Real hardware does this line by line, but checking dozens of sprites on each scanline isn't efficient or necessary

	frame_buffer = &nextframeptr[0];
	//Sprites
	// dkong has its own sprite drawing routine since unlike the other
	// games, in dkong black is not always transparent. Black pixels
	// are instead used for masking

	for (int chunk = 0; chunk < TV_HEIGHT / CHUNKSIZE; chunk++)
	{
		unsigned short* store = &nextframeptr[chunk * CHUNKSIZE * TV_WIDTH];

		for (int x = 0; x < TV_WIDTH * CHUNKSIZE; x++)
		{
			store[x] = 0;
		}
	}

	_1942_render_tile_raster(0);

	for (int chunk = 0; chunk < TV_HEIGHT / CHUNKSIZE; chunk++)
	{
		_1942_render_sprite_raster(chunk);
		_1942_render_foreground_raster(chunk);

	}

}
#endif // IO_EMULATION
