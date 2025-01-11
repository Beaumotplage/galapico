/* dkong.h */
//Galapico TODO: Tiles wrapping by one pixel to the left
// Needs different sound sample rate.

#ifdef CPU_EMULATION

#include "dkong_rom1.h"

static inline unsigned char dkong_RdZ80(unsigned short Addr) {
	if (Addr < 16384)
		return dkong_rom_cpu1[Addr];

	// 0x6000 - 0x77ff
	if (((Addr & 0xf000) == 0x6000) || ((Addr & 0xf800) == 0x7000))
		return memory[Addr - 0x6000];

	if ((Addr & 0xfff0) == 0x7c00) {
		// get a mask of currently pressed keys
		unsigned char keymask = buttons_get();

		unsigned char retval = 0x00;
		if (keymask & BUTTON_RIGHT) retval |= 0x01;
		if (keymask & BUTTON_LEFT)  retval |= 0x02;
		if (keymask & BUTTON_UP)    retval |= 0x04;
		if (keymask & BUTTON_DOWN)  retval |= 0x08;
		if (keymask & BUTTON_FIRE)  retval |= 0x10;
		return retval;
	}

	if ((Addr & 0xfff0) == 0x7c80) {  // IN1
		return 0x00;
	}

	if ((Addr & 0xfff0) == 0x7d00) {
		// get a mask of currently pressed keys
		unsigned char keymask = buttons_get();

		unsigned char retval = 0x00;
		if (keymask & BUTTON_COIN)   retval |= 0x80;
		if (keymask & BUTTON_START)  retval |= 0x04;
		return retval;
	}

	if ((Addr & 0xfff0) == 0x7d80)
		return DKONG_DIP;

	return 0xff;
}

static inline void dkong_WrZ80(unsigned short Addr, unsigned char Value) {
	if (((Addr & 0xf000) == 0x6000) || ((Addr & 0xf800) == 0x7000)) {
		memory[Addr - 0x6000] = Value;
		return;
	}

	// ignore DMA register access
	if ((Addr & 0xfe00) == 0x7800)
		return;

	if ((Addr & 0xfe00) == 0x7c00) {  // 7cxx and 7dxx
	  // music effect
		if (Addr == 0x7c00) dkong_sfx_index = Value;

		// 7d0x
		if ((Addr & 0xfff0) == 0x7d00) {

			// trigger samples 0 (walk), 1 (jump) and 2 (stomp)
			if ((Addr & 0x0f) <= 2 && Value)
				dkong_trigger_sound(Addr & 0x0f);

			if ((Addr & 0x0f) == 3) {
				if (Value & 1) cpu_8048.p2_state &= ~0x20;
				else          cpu_8048.p2_state |= 0x20;
			}

			if ((Addr & 0x0f) == 4)
				cpu_8048.T1 = !(Value & 1);

			if ((Addr & 0x0f) == 5)
				cpu_8048.T0 = !(Value & 1);
		}

		if (Addr == 0x7d80)
			cpu_8048.notINT = !(Value & 1);

		if (Addr == 0x7d84)
			irq_enable[0] = Value & 1;

		if ((Addr == 0x7d85) && (Value & 1)) {
			// trigger DRQ to start DMA
			// Dkong uses the DMA only to copy sprite data from 0x6900 to 0x7000
			memcpy(memory + 0x1000, memory + 0x900, 384);
		}

		if (Addr == 0x7d86) {
			colortable_select &= ~1;
			colortable_select |= (Value & 1);
		}

		if (Addr == 0x7d87) {
			colortable_select &= ~2;
			colortable_select |= ((Value << 1) & 2);
		}
		return;
	}
}

static inline void dkong_run_frame(void) {
	game_started = 1; // TODO: make this from some graphic thing

	// dkong      
	for (int i = 0; i < INST_PER_FRAME_DKONG; i++) {
		StepZ80(cpu); StepZ80(cpu); StepZ80(cpu); StepZ80(cpu);

		// run audio cpu only when audio transfer buffers are not full. The
		// audio CPU seems to need more CPU time than the main Z80 itself.
		if (((dkong_audio_wptr + 1) & DKONG_AUDIO_QUEUE_MASK) != dkong_audio_rptr) {
			i8048_step(&cpu_8048); i8048_step(&cpu_8048);
			i8048_step(&cpu_8048); i8048_step(&cpu_8048);
			i8048_step(&cpu_8048); i8048_step(&cpu_8048);
		}
	}

	if (irq_enable[0])
		IntZ80(cpu, INT_NMI);
}
#endif // CPU_EMULATION

#ifdef IO_EMULATION

#define CHUNKSIZE (8)

#ifndef SINGLE_MACHINE
#include "dkong_logo.h"
#endif
#include "dkong_tilemap.h"
#include "dkong_spritemap.h"
#include "dkong_cmap.h"

#include "dkong_sample_walk0.h"
#include "dkong_sample_walk1.h"
#include "dkong_sample_walk2.h"
#include "dkong_sample_jump.h"
#include "dkong_sample_stomp.h"

static inline void dkong_prepare_frame(void) {
	active_sprites = 0;
	for (int idx = 0; idx < 96 && active_sprites < 92; idx++) {
		// sprites are stored at 0x7000
		unsigned char* sprite_base_ptr = memory + 0x1000 + 4 * idx;
		struct sprite_S spr;

		// adjust sprite position on screen for upright screen
		spr.x = sprite_base_ptr[0] - 23;
		spr.y = sprite_base_ptr[3] + 8;

		spr.code = sprite_base_ptr[1] & 0x7f;
		spr.color = sprite_base_ptr[2] & 0x0f;
		spr.flags = ((sprite_base_ptr[2] & 0x80) ? 1 : 0) |
			((sprite_base_ptr[1] & 0x80) ? 2 : 0);

		// save sprite in list of active sprites
		if ((spr.y > -16) && (spr.y < 288) &&
			(spr.x > -16) && (spr.x < 224))
			sprite[active_sprites++] = spr;
	}
}


static inline void dkong_render_tile_raster(unsigned short chunk)
{

	// TODO: we're still in galagino portait-TFT world so columns are vertical
	// int col = (TV_HEIGHT / CHUNKSIZE) - chunk;

	int col = chunk;

	// A row is actually a column for a horizontal raster beam 
	for (int row = 0; row < GAME_WIDTH/8; row++)
	{
		unsigned short addr = tileaddr[row][(TV_HEIGHT / CHUNKSIZE) - col - 1];

		if ((memory[0x1400 + addr] != 0x10) &&           // skip blank dkong tiles (0x10) in rendering  
			((row >= 2) && (row < 34)))
		{
			const unsigned short* tile = v_5h_b_bin[memory[0x1400 + addr]];

			// donkey kong has some sort of global color table
			const unsigned short* colors = dkong_colormap[colortable_select][row - 2 + 32 * (col / 4)];

		//	unsigned short* ptr = frame_buffer + 8 * (row + (TV_WIDTH * col)); // was column

			unsigned int pointeroffset = +CRT_ROW_OFFSET + 8 * (row + (TV_WIDTH * col));
			if (pointeroffset >= (TV_WIDTH * TV_HEIGHT)-100 )
			{
				pointeroffset = TV_WIDTH * TV_HEIGHT - 100;
			}
			unsigned short* ptr = frame_buffer + pointeroffset;


			
			// 8 pixel rows per tile
			for (char r = 0; r < 8; r++, ptr += (TV_WIDTH - 8))
			{
				unsigned short pix = *tile++;
				// 8 pixel columns per tile
				for (char c = 0; c < 8; c++, pix >>= 2)
				{
					if (pix & 3)
						*ptr = colors[pix & 3];
					ptr++;
				}
			}
		}
	}
}


static inline void dkong_render_sprite_raster(unsigned short chunk)
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
			const unsigned long* spr = dkong_sprites[sprite[s].flags & 3][sprite[s].code];
			const unsigned short* colors = dkong_colormap_sprite[colortable_select][sprite[s].color];

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

			unsigned short* ptr = frame_buffer+ CRT_ROW_OFFSET + ((scanline_start)*TV_WIDTH) + sprite[s].y;

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


static inline void dkong_render_frame_raster()
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

		dkong_render_tile_raster(chunk);
		dkong_render_sprite_raster(chunk);
	}
}


#endif // IO_EMULATION
