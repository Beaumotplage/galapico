/* digdug.h */

//Galapico TODO: Optimise for full rate Z80s, as this is so bad it's missing 60Hz 

#ifdef CPU_EMULATION

#include "digdug_rom1.h"
#include "digdug_rom2.h"
#include "digdug_rom3.h"

static inline unsigned char digdug_RdZ80(unsigned short Addr) {
	static const unsigned char* rom[] = { digdug_rom_cpu1, digdug_rom_cpu2, digdug_rom_cpu3 };

	if (Addr < 16384)
		return rom[current_cpu][Addr];

	/* video/sprite ram */
	if ((Addr & 0xe000) == 0x8000)
		return memory[Addr - 0x8000];

	// 0x7100 namco_06xx_device, ctrl_r, ctrl_w
	if ((Addr & 0xfe00) == 0x7000)
		return namco_read_dd(Addr & 0x1ff);

	//  if((Addr & 0xfff0) == 0xa000)
	//    return 0xff;

	if ((Addr & 0xffc0) == 0xb800)
		return 0x00;

	return 0xff;
}

static inline void digdug_WrZ80(unsigned short Addr, unsigned char Value) {
	if ((Addr & 0xe000) == 0x8000) {
		// 8000 - 83ff - tile ram
		// 8400 - 87ff - work
		// 8800 - 8bff - sprite ram
		// 9000 - 93ff - sprite ram
		// 9800 - 9bff - sprite ram

		// this is actually 2k + 1k + 1k gap + 1k + 1k gap + 1k + 1k gap

		// this comaprison really hurts performance wise. Try to
		// find some other trigger, like IO

		// writing 46 (U), first char of UP, to the top left corner
		// is an indication that the game has booted up      
		if (Addr == 0x8000 + 985 && (Value & 0x7f) == 46)
			game_started = 1;

		memory[Addr - 0x8000] = Value;
		return;
	}

	if ((Addr & 0xffe0) == 0x6800) {
		soundregs[Addr - 0x6800] = Value & 0x0f;
		return;
	}

	if ((Addr & 0xfff8) == 0x6820) {
		if ((Addr & 0x0c) == 0x00) {
			if ((Addr & 3) < 3) {
				irq_enable[Addr & 3] = Value & 1;
			}
			else {
				sub_cpu_reset = !Value;

				if (sub_cpu_reset) {
					// this also resets the 51xx
					namco_command = 0x00;
					namco_mode = 0;
					namco_nmi_counter = 0;

					current_cpu = 1; ResetZ80(&cpu[1]);
					current_cpu = 2; ResetZ80(&cpu[2]);
				}
			}
		}
		return;
	}

	// if(Addr == 0x6830) return; // watchdog etc

	if ((Addr & 0xfe00) == 0x7000) {
		namco_write_dd(Addr & 0x1ff, Value);
		return;
	}

	// control port
	if ((Addr & 0xfff8) == 0xa000) {
		if (Value & 1) digdug_video_latch |= (1 << (Addr & 7));
		else          digdug_video_latch &= ~(1 << (Addr & 7));
		return;
	}

	//if((Addr & 0xffc0) == 0xb800) return;
	//if(Addr == 0xb840) return;

	// digdug cpu #1 writes to 0xffff, 0000 and 0002 @ #1@0866/868/86c
	// if((current_cpu == 1) && ((Addr == 0xffff) || (Addr == 0) || (Addr == 2))) return;
}

static inline void digdug_run_frame(void) {
	for (char c = 0; c < 4; c++) {
		for (int i = 0; i < INST_PER_FRAME_DIGDUG / 4; i++) {
			current_cpu = 0;
			StepZ80(cpu); digdug_StepZ80(cpu); digdug_StepZ80(cpu); digdug_StepZ80(cpu);
			if (!sub_cpu_reset) {
				// running both sub-cpus at full speed as well makes the setup instable :-(
				current_cpu = 1;
				StepZ80(cpu + 1); digdug_StepZ80(cpu + 1); digdug_StepZ80(cpu + 1); digdug_StepZ80(cpu + 1);
				current_cpu = 2;
				StepZ80(cpu + 2); digdug_StepZ80(cpu + 2); digdug_StepZ80(cpu + 2); digdug_StepZ80(cpu + 2);
			}

			// nmi counter for cpu0
			if (namco_nmi_counter) {
				namco_nmi_counter--;
				if (!namco_nmi_counter) {
					current_cpu = 0;
					IntZ80(&cpu[0], INT_NMI);
					namco_nmi_counter = NAMCO_NMI_DELAY;
				}
			}
		}

		// run cpu2 nmi at ~line 64 and line 192
		if (!sub_cpu_reset && !irq_enable[2] && ((c == 1) || (c == 3))) {
			current_cpu = 2;
			IntZ80(&cpu[2], INT_NMI);
		}
	}

	if (irq_enable[0]) {
		current_cpu = 0;
		IntZ80(&cpu[0], INT_RST38);
	}

	if (!sub_cpu_reset && irq_enable[1]) {
		current_cpu = 1;
		IntZ80(&cpu[1], INT_RST38);
	}
}

#endif // CPU_EMULATION

#ifdef IO_EMULATION

#ifndef SINGLE_MACHINE
#include "digdug_logo.h"
#endif
#include "digdug_spritemap.h"
#include "digdug_tilemap.h"
#include "digdug_pftiles.h"
#include "digdug_cmap_tiles.h"
#include "digdug_cmap_sprites.h"
#include "digdug_cmap.h"
#include "digdug_playfield.h"
#include "digdug_wavetable.h"

#define USE_NAMCO_WAVETABLE

unsigned char digdug_video_latch = 0x00;

static inline void digdug_prepare_frame(void) {
	// Do all the preparations to render a screen.

	/* preprocess sprites */
	active_sprites = 0;
	for (int idx = 0; idx < 64 && active_sprites < 124; idx++) {
		unsigned char* sprite_base_ptr = memory + 2 * idx;
		// check if sprite is visible
		if ((sprite_base_ptr[0x1b80 + 1] & 2) == 0) {
			struct sprite_S spr;

			spr.code = sprite_base_ptr[0x0b80];
			spr.color = sprite_base_ptr[0x0b80 + 1] & 0x3f;
			spr.flags = sprite_base_ptr[0x1b80];

			// adjust sprite position on screen for upright screen
			spr.x = sprite_base_ptr[0x1380] - 16;
			spr.y = sprite_base_ptr[0x1380 + 1] - 40 + 1;
			if (spr.y < 16) spr.y += 256;

			if (sprite_base_ptr[0x0b80] & 0x80) {
				spr.flags |= 0x0c;
				spr.code = (spr.code & 0xc0) | ((spr.code & ~0xc0) << 2);
			}

			if ((spr.y > -16) && (spr.y < 288) &&
				(spr.x > -16) && (spr.x < 224)) {

				// save sprite in list of active sprites
				sprite[active_sprites] = spr;
				// for horizontally doubled sprites, this one becomes the code + 2 part
				if (spr.flags & 0x08) sprite[active_sprites].code += 2;
				active_sprites++;
			}

			// handle horizontally doubled sprites
			if ((spr.flags & 0x08) &&
				(spr.y > -16) && (spr.y < 288) &&
				((spr.x + 16) > -16) && ((spr.x + 16) < 224)) {
				// place a copy right to the current one
				sprite[active_sprites] = spr;
				sprite[active_sprites].x += 16;
				active_sprites++;

				// on hflip swap both halfs
				if (spr.flags & 2) {
					int tmp = sprite[active_sprites - 1].code;
					sprite[active_sprites - 1].code = sprite[active_sprites - 2].code;
					sprite[active_sprites - 2].code = tmp;
				}
			}

			// handle vertically doubled sprites (these don't seem to happen
			// in galaga)
			if ((spr.flags & 0x04) &&
				((spr.y + 16) > -16) && ((spr.y + 16) < 288) &&
				(spr.x > -16) && (spr.x < 224)) {
				// place a copy below the current one
				sprite[active_sprites] = spr;
				sprite[active_sprites].code += 3;
				sprite[active_sprites].y += 16;
				active_sprites++;
			}

			// handle in both directions doubled sprites
			if (((spr.flags & 0x0c) == 0x0c) &&
				((spr.y + 16) > -16) && ((spr.y + 16) < 288) &&
				((spr.x + 16) > -16) && ((spr.x + 16) < 224)) {
				// place a copy right and below the current one
				sprite[active_sprites] = spr;
				sprite[active_sprites].code += 1;
				sprite[active_sprites].x += 16;
				sprite[active_sprites].y += 16;
				active_sprites++;

				// on hflip swap both bottom halfs
				if (spr.flags & 2) {
					int tmp = sprite[active_sprites - 1].code;
					sprite[active_sprites - 1].code = sprite[active_sprites - 2].code;
					sprite[active_sprites - 2].code = tmp;
				}
			}
		}
	}
}


static inline void digdug_render_sprite_raster(unsigned short chunk)
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
			const unsigned long* spr = digdug_sprites[sprite[s].flags & 3][sprite[s].code];
			const unsigned short* colors = digdug_colormap_sprites[sprite[s].color & 63];

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

			spr += startline;
			// calculate pixel lines to paint  
			int scanline_start = chunk_vert_start;
			if (sprite_chunk_offset >= 0)
				scanline_start = (chunk * CHUNKSIZE) + sprite_chunk_offset; // i.e. sprite[].x

			unsigned short* ptr = frame_buffer +CRT_ROW_OFFSET + ((scanline_start)*TV_WIDTH) + sprite[s].y;

			for (char r = 0; r < lines2draw; r++, ptr += (TV_WIDTH - 16))

			{
				unsigned long pix = *spr++;//& mask;
				// 16 pixel columns per tile
				for (char c = 0; c < 16; c++, pix >>= 2)
				{
					unsigned short col = colors[pix & 3];
					if (pix & 3)
						*ptr = col;
					ptr++;
				}
			}
		}
	}
}


static inline void digdug_render_tile_raster(unsigned short chunk)
{
	// the playfield rom contains four playfields.
	// 0: normal one
	// 1: cross pattern
	// 2: funny logo
	// 3: monocolor

	int col = chunk;

	// A row is actually a column for a horizontal raster beam 
	for (int row = 0; row < GAME_WIDTH / 8; row++)
	{
		unsigned short addr = tileaddr[row][(TV_HEIGHT / CHUNKSIZE) - col - 1];

		unsigned char chr = digdug_playfield[addr + (digdug_video_latch & 3) * 0x400];
		unsigned char color = chr >> 4;
		const unsigned short* tile = dd1_11[chr];   // playfield

		// colorprom contains 4*16 color groups
		const unsigned short* colors =
			digdug_colormap_tiles[(digdug_video_latch & 0x30) + color];

		chr = memory[addr];
		// upper four bits point directly into the colormap
		const unsigned short* fgtile = dd1_9[chr & 0x7f];
		unsigned short fgcol = ((unsigned short*)digdug_colormaps)
			[((chr >> 4) & 0x0e) | ((chr >> 3) & 2)];

		// this mode is never used in digdug
		if (digdug_video_latch & 4)
			fgcol = ((unsigned short*)digdug_colormaps)[chr & 15];

		unsigned short* ptr = frame_buffer + CRT_ROW_OFFSET + 8 * (row + (TV_WIDTH * col)); // was column

			  // 8 pixel rows per tile
		for (char r = 0; r < 8; r++, ptr += (TV_WIDTH - 8))
		{
			unsigned short bg_pix = *tile++;
			unsigned short fg_pix = *fgtile++;

			// 8 pixel columns per tile
			for (char c = 0; c < 8; c++, bg_pix >>= 2, fg_pix >>= 2) {
				if (!(digdug_video_latch & 8)) *ptr = colors[bg_pix & 3];
				if (fg_pix & 3) *ptr = *ptr = fgcol;
				ptr++;
			}

		}
	}
}



static inline void digdug_render_frame_raster()
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

		digdug_render_tile_raster(chunk);
		digdug_render_sprite_raster(chunk);
	}
}



#endif // IO_EMULATION
