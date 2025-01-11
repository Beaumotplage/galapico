/* galaga.h */
//Galapico TODO: Shift the picture a bit as scores off screen


#ifdef CPU_EMULATION

#include "galaga_rom1.h"
#include "galaga_rom2.h"
#include "galaga_rom3.h"

static inline unsigned char galaga_RdZ80(unsigned short Addr) {
	static const unsigned char* rom[] = { galaga_rom_cpu1, galaga_rom_cpu2, galaga_rom_cpu3 };

	if (Addr < 16384)
		return rom[current_cpu][Addr];

	/* video/sprite ram */
	if ((Addr & 0xe000) == 0x8000)
		return memory[Addr - 0x8000];

	// latch
	if ((Addr & 0xfff8) == 0x6800) {
		unsigned char dip_a = (GALAGA_DIPA & (0x80 >> (Addr & 7))) ? 0 : 1;
		unsigned char dip_b = (GALAGA_DIPB & (0x80 >> (Addr & 7))) ? 0 : 2;
		return dip_a + dip_b;
	}

	// 0x7100 namco_06xx_device, ctrl_r, ctrl_w
	if ((Addr & 0xfe00) == 0x7000) {
		if (Addr & 0x100) {
			return namco_busy ? 0x00 : 0x10;   // cmd ack (game_ctrl.s L1024)
		}
		else {
			unsigned char retval = 0x00;

			if (cs_ctrl & 1) {   // bit[0] -> 51xx selected
				if (!credit_mode) {
					// galaga doesn't seem to use the button mappings in byte 1 and 2 in 
					// non-credit mode. So we don't implement that
					unsigned char map71[] = { 0b11111111, 0xff, 0xff };
					if (namco_cnt > 2) return 0xff;

					retval = map71[namco_cnt];
				}
				else {
					static unsigned char prev_mask = 0;
					static unsigned char fire_timer = 0;

					// byte 0 is credit in BCD, byte 1 and 2: ...FLURD 
					unsigned char mapb1[] = { 16 * (credit / 10) + credit % 10, 0b11111111, 0b11111111 };

					// get a mask of currently pressed keys
					unsigned char keymask = buttons_get();

					// report directions directly
					if (keymask & BUTTON_LEFT)  mapb1[1] &= ~0x08;
					if (keymask & BUTTON_UP)    mapb1[1] &= ~0x04;
					if (keymask & BUTTON_RIGHT) mapb1[1] &= ~0x02;
					if (keymask & BUTTON_DOWN)  mapb1[1] &= ~0x01;

					// report fire only when it was pressed
					if ((keymask & BUTTON_FIRE) && !(prev_mask & BUTTON_FIRE)) {
						mapb1[1] &= ~0x10;
						fire_timer = 1;         // 0 is too short for score enter, 5 is too long
						// should probably be done via a global counter
					}
					else if (fire_timer) {
						mapb1[1] &= ~0x10;
						fire_timer--;
					}

					// 51xx leaves credit mode when user presses start? Nope ...
					if ((keymask & BUTTON_START) && !(prev_mask & BUTTON_START) && credit)
						credit -= 1;

					if ((keymask & BUTTON_COIN) && !(prev_mask & BUTTON_COIN) && (credit < 99))
						credit += 1;

					if (namco_cnt > 2) return 0xff;

					retval = mapb1[namco_cnt];
					prev_mask = keymask;
				}
				namco_cnt++;
			}
			return retval;
		}
	}

	return 0xff;
}

static inline void galaga_WrZ80(unsigned short Addr, unsigned char Value) {
	if (Addr < 16384) return;   // ignore rom writes

	if ((Addr & 0xe000) == 0x8000)
		memory[Addr - 0x8000] = Value;

	// namco 06xx
	if ((Addr & 0xf800) == 0x7000) {

		if (Addr & 0x100) {  // 7100
		  // see task_man.s L450
			namco_cnt = 0;
			cs_ctrl = Value;
			namco_busy = 5000;   // this delay is important for proper startup 

			if (Value == 0xa8) galaga_trigger_sound_explosion();

		}
		else {            // 7000
			if (cs_ctrl & 1) {
				if (coincredMode) {
					coincredMode--;
					return;
				}
				switch (Value) {
				case 1:
					// main cpu sets coin and cred mode for both players (four bytes)
					coincredMode = 4;
					break;

				case 2:
					credit_mode = 1;
					break;

				case 3: // disable joystick remapping
				case 4: // enable joystick remapping
					break;

				case 5:
					credit_mode = 0;
					break;
				}
				namco_cnt++;
			}
		}
	}
	if ((Addr & 0xffe0) == 0x6800) {
		int offset = Addr - 0x6800;
		Value &= 0x0f;

		if (soundregs[offset] != Value)
			soundregs[offset] = Value;

		return;
	}

	if ((Addr & 0xfffc) == 0x6820) {
		if ((Addr & 3) < 3)
			irq_enable[Addr & 3] = Value;
		else {
			sub_cpu_reset = !Value;
			credit_mode = 0;   // this also resets the 51xx

			if (sub_cpu_reset) {
				current_cpu = 1; ResetZ80(&cpu[1]);
				current_cpu = 2; ResetZ80(&cpu[2]);
			}
		}
		return;
	}

	if (Value == 0 && Addr == 0x8210) // gg1-4.s L1725
		game_started = 1;

	if ((Addr & 0xfff0) == 0xa000) {
		if (Value & 1)  starcontrol |= (1 << (Addr & 7));   // set bit
		else           starcontrol &= ~(1 << (Addr & 7));   // clear bit
		return;
	}

#if 0
	// inject a fixed score to be able to check hiscore routines easily
	if (game_started && (Addr == 0x8000 + 1020) && (memory[1017] == 0) && (memory[1018] == 0x24)) {
		// start with score = 23450
		memory[1020] = 2; memory[1019] = 3; memory[1018] = 4; memory[1017] = 5;
	}
#endif
}

static inline void galaga_run_frame(void) {
	for (int i = 0; i < INST_PER_FRAME_GALAGA; i++) {
		current_cpu = 0;
		StepZ80(cpu); StepZ80(cpu); StepZ80(cpu); StepZ80(cpu);
		if (!sub_cpu_reset) {
			current_cpu = 1;
			StepZ80(cpu + 1); StepZ80(cpu + 1); StepZ80(cpu + 1); StepZ80(cpu + 1);
			current_cpu = 2;
			StepZ80(cpu + 2); StepZ80(cpu + 2); StepZ80(cpu + 2); StepZ80(cpu + 2);
		}

		if (namco_busy) namco_busy--;

		// nmi counter for cpu0
		if ((cs_ctrl & 0xe0) != 0) {
			// ctr_ctrl[7:5] * 64 * 330ns (hl)->(de)
			if (nmi_cnt < (cs_ctrl >> 5) * 64) {
				nmi_cnt++;
			}
			else {
				current_cpu = 0;
				IntZ80(&cpu[0], INT_NMI);
				nmi_cnt = 0;
			}
		}

		// run cpu2 nmi at ~line 64 and line 192
		if (!sub_cpu_reset && !irq_enable[2] &&
			((i == INST_PER_FRAME_GALAGA / 4) || (i == 3 * INST_PER_FRAME_GALAGA / 4))) {
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
#include "galaga_logo.h"
#endif
#include "galaga_spritemap.h"
#include "galaga_tilemap.h"
#include "galaga_cmap_tiles.h"
#include "galaga_cmap_sprites.h"
#include "galaga_wavetable.h"
#include "galaga_sample_boom.h"
#include "galaga_starseed.h"

#define USE_NAMCO_WAVETABLE

unsigned char stars_scroll_y = 0;

static inline void galaga_prepare_frame(void) {
	// Do all the preparations to render a screen.

	//PICO leds_state_reset();

	/* preprocess sprites */
	active_sprites = 0;
	for (int idx = 0; idx < 64 && active_sprites < 92; idx++) {
		unsigned char* sprite_base_ptr = memory + 2 * (63 - idx);
		// check if sprite is visible
		if ((sprite_base_ptr[0x1b80 + 1] & 2) == 0) {
			struct sprite_S spr;

			spr.code = sprite_base_ptr[0x0b80];
			spr.color = sprite_base_ptr[0x0b80 + 1];
			spr.flags = sprite_base_ptr[0x1b80];
			spr.x = sprite_base_ptr[0x1380] - 16;
			spr.y = sprite_base_ptr[0x1380 + 1] +
				0x100 * (sprite_base_ptr[0x1b80 + 1] & 1) - 40;

			if ((spr.code < 128) &&
				(spr.y > -16) && (spr.y < 288) &&
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
				((spr.x + 16) >= -16) && ((spr.x + 16) < 224)) {
				// place a copy right to the current one
				sprite[active_sprites] = spr;
				sprite[active_sprites].x += 16;
				active_sprites++;
			}

			// handle vertically doubled sprites
			// (these don't seem to happen in galaga)
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
			}
		}
	}
}



// Do two star sets together 
static void galaga_render_stars_sets_raster(const struct galaga_star* set0, const struct galaga_star* set1) {

	for (int row = 0; row < GAME_WIDTH / 8; row++)
	{
		for (char star_cntr = 0; star_cntr < 63; star_cntr++)
		{

			const struct galaga_star* s = set0 + star_cntr;

			unsigned short x = (244 - s->x) & 0xff;
			unsigned short y = ((s->y + stars_scroll_y) & 0xff) + 16 - row * 8;

			if (y < 8 && x < 224)
				frame_buffer[y + 8 * row + (x * TV_WIDTH) +CRT_ROW_OFFSET] = s->col;

			s = set1 + star_cntr;

			x = (244 - s->x) & 0xff;
			y = ((s->y + stars_scroll_y) & 0xff) + 16 - row * 8;

			if (y < 8 && x < 224)
				frame_buffer[y + 8 * row + (x * TV_WIDTH) + +CRT_ROW_OFFSET] = s->col;
		}
	}
}

static inline void galaga_render_sprite_raster(unsigned short chunk)
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
			const unsigned long* spr = galaga_sprites[sprite[s].flags & 3][sprite[s].code];
			const unsigned short* colors = galaga_colormap_sprites[sprite[s].color & 63];

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

			if (colors[0] != 0)
			{
				lines2draw = 0;
			}

			unsigned short* ptr = frame_buffer+ + CRT_ROW_OFFSET + ((scanline_start)*TV_WIDTH) + sprite[s].y;

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


static inline void galaga_render_tile_raster(unsigned short chunk)
{

	int col = chunk;

	// A row is actually a column for a horizontal raster beam 
	for (int row = 0; row < GAME_WIDTH / 8; row++)
	{
		unsigned short addr = tileaddr[row][(TV_HEIGHT / CHUNKSIZE) - col - 1];

		if (memory[addr] != 0x24)
		{
			const unsigned short* tile = gg1_9_4l[memory[addr]];
			const unsigned short* colors = galaga_colormap_tiles[memory[0x400 + addr] & 63];

			unsigned short* ptr = frame_buffer  +CRT_ROW_OFFSET + 8 * (row + (TV_WIDTH * col)); // was column

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



static inline void galaga_render_frame_raster()
{
	// Raster beam scan, divided into sprite-sized chunks (x axis) 
	// Means I can ditch the framebuffer(s) if RAM gets low
	// X and Y are in vertical monitor-world, so remember y is horizontal beam and x is vertical (aargh)
	// Real hardware does this line by line, but checking dozens of sprites on each scanline isn't efficient or necessary

	frame_buffer = &nextframeptr[0];

	for (int chunk = 0; chunk < TV_HEIGHT / CHUNKSIZE; chunk++)
	{
		unsigned short* store = &nextframeptr[chunk * CHUNKSIZE * TV_WIDTH];

		for (int x = 0; x < TV_WIDTH * CHUNKSIZE; x++)
		{
			store[x] = 0;
		}
	}

	/* two sets of stars controlled by these bits */
	galaga_render_stars_sets_raster(galaga_star_set[(starcontrol & 0x08) ? 1 : 0],
		galaga_star_set[(starcontrol & 0x10) ? 3 : 2]);

	for (int chunk = 0; chunk < TV_HEIGHT / CHUNKSIZE; chunk++)
	{
		galaga_render_tile_raster(chunk);
		galaga_render_sprite_raster(chunk);
	}
}


#endif // IO_EMULATION
