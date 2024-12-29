/*
 * Galapico.cpp - Galaga & friends arcade emulator for Pi Pico 2 (RP2350)
 * 
 * Port/Conversion of Till Harbaum's wonderful Galagino emulator 
 *
 * Port by John Bennett 2025. Please contact me via Github for questions, 
 * please don't bother the original author.
 * 
 * V0.1. I'll iron out the glitches, then might diverge as I have fun.
 * 
 * Published under GPLv3
 *
 */
#define IO_EMULATION (1)


#include "pc_or_rp2040.h"
#include "galagino\config.h"
#include "galagino\emulation.h"
#include "galapico.h"
#include "galagino\tileaddr.h"

#include "HAL_video.h"
#include "HAL_pwm_audio.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hw_defs.h"

struct sprite_S {
    unsigned char code, color, flags;
    short x, y;
};

// Video buffer from HAL (TODO, include not extern)
extern unsigned short* nextframeptr;
unsigned short* frame_buffer = &nextframeptr[0];

// Translation RAM for logo page. Can be removed if recoded to raster horizontal buffer
unsigned short vert_buffer[224 * 8];


// the hardware supports 64 sprites
unsigned char active_sprites = 0;

sprite_S spriteram[256]; // 128 sufficient?
struct sprite_S* sprite = &spriteram[0];

// Rolling audio buffer to feel HAL audio PWM output
static unsigned char null_audio[2] = {0x80,0x80};
static unsigned short sound_buffer_index = 1;
static unsigned char* sound_buffer_pointer = &null_audio[0];
bool sound_init = 0;

// Inputword for inputs - TODO?
static unsigned int inputs = 0;

#ifndef SINGLE_MACHINE
signed char machine = MCH_MENU;   // start with menu
//signed char machine = MCH_PACMAN;
#endif

// Quick greyscale lookup table
unsigned char grey[256] =
{
0,0,9,9,9,82,82,91,0,9,9,9,82,82,91,91,
9,9,9,82,82,91,91,91,9,9,82,82,91,91,91,164,
9,82,82,91,91,91,164,164,82,82,91,91,91,164,164,164,
82,91,91,91,164,164,164,173,91,91,91,164,164,164,173,173,
9,9,9,82,82,91,91,91,9,9,82,82,91,91,91,164,
9,82,82,91,91,91,164,164,82,82,91,91,91,164,164,164,
82,91,91,91,164,164,164,173,91,91,91,164,164,164,173,173,
91,91,164,164,164,173,173,246,91,164,164,164,173,173,246,246,
9,82,82,91,91,91,164,164,82,82,91,91,91,164,164,164,
82,91,91,91,164,164,164,173,91,91,91,164,164,164,173,173,
91,91,164,164,164,173,173,246,91,164,164,164,173,173,246,246,
164,164,164,173,173,246,246,246,164,164,173,173,246,246,246,255,
82,91,91,91,164,164,164,173,91,91,91,164,164,164,173,173,
91,91,164,164,164,173,173,246,91,164,164,164,173,173,246,246,
164,164,164,173,173,246,246,246,164,164,173,173,246,246,246,255,
164,173,173,246,246,246,255,255,173,173,246,246,246,255,
};


// include converted rom data
#ifdef ENABLE_PACMAN
#include "galagino\pacman.h"
#endif

#ifdef ENABLE_GALAGA
#include "galagino\galaga.h"
#endif

#ifdef ENABLE_DKONG
#include "galagino\dkong.h"
#endif

#ifdef ENABLE_FROGGER
#include "galagino\frogger.h"
#endif

#ifdef ENABLE_DIGDUG
#include "galagino\digdug.h"
#endif

#ifdef ENABLE_1942
#include "galagino\1942.h"
#endif


#ifdef ENABLE_GALAGA
// the ship explosion sound is stored as a digi sample.
// All other sounds are generated on the fly via the
// original wave tables
unsigned short snd_boom_cnt = 0;
const signed char *snd_boom_ptr = NULL;
#endif

#ifdef ENABLE_DKONG
unsigned short dkong_sample_cnt[3] = { 0,0,0 };
const signed char *dkong_sample_ptr[3];
#endif

#if defined(ENABLE_FROGGER) || defined(ENABLE_1942)
int ay_period[2][4] = {{0,0,0,0}, {0,0,0,0}};
int ay_volume[2][3] = {{0,0,0}, {0,0,0}};
int ay_enable[2][3] = {{0,0,0}, {0,0,0}};
int audio_cnt[2][4], audio_toggle[2][4] = {{1,1,1,1},{1,1,1,1}};
unsigned long ay_noise_rng[2] = { 1, 1 };
extern unsigned char soundregs[];
#endif



#ifndef SINGLE_MACHINE

// render one of three the menu logos. Only the active one is colorful
// render logo into current buffer starting with line "row" of the logo
void render_logo(short row, const unsigned short *logo, char active) 
{
  unsigned short marker = logo[0];
  const unsigned short *data = logo+1;

  // current pixel to be drawn
  unsigned short ipix = 0;
    
  // less than 8 rows in image left?
  unsigned short pix2draw = ((row <= 96-8)?(224*8):((96-row)*224));
  
  if(row >= 0) {
    // skip ahead to row
    unsigned short col = 0;
    unsigned short pix = 0;
    while(pix < 224*row) {
      if(data[0] != marker) {
        pix++;
        data++;
      } else {
        pix += data[1]+1;
        col = data[2];
        data += 3;
      }
    }
    
    // draw pixels remaining from previous run
    if(!active) col = grey[col & 0xFF];
    while(ipix < ((pix - 224*row < pix2draw)?(pix - 224*row):pix2draw))
      vert_buffer[ipix++] = col;
  } else
    // if row is negative, then skip target pixel
    ipix -= row * 224;
    
  while(ipix < pix2draw) {
    if(data[0] != marker)
      vert_buffer[ipix++] = active?*data++:grey[0xFF & *data++];
    else {
      unsigned short color = data[2];
      if(!active) color = grey[color & 0xFF];
      for(unsigned short j=0;j<data[1]+1 && ipix < pix2draw;j++)
        vert_buffer[ipix++] = color;

      data += 3;
    }
  }  
}
#endif

//TODO: Get these out of RAM and back to const without hammering the CPU 
#ifndef SINGLE_MACHINE
// menu for more than three machines
unsigned short *logos[] = {
#ifdef ENABLE_PACMAN    
  pacman_logo,
#endif
#ifdef ENABLE_GALAGA
  galaga_logo,
#endif
#ifdef ENABLE_DKONG    
  dkong_logo,
#endif
#ifdef ENABLE_FROGGER    
  frogger_logo,
#endif
#ifdef ENABLE_DIGDUG    
  digdug_logo,
#endif
#ifdef ENABLE_1942
  _1942_logo,
#endif
};
#endif


// render one of 36 tile rows (8 x 224 pixel lines)
void galapico_render_frame()
{

#ifndef SINGLE_MACHINE
    if (machine == MCH_MENU)
    {
        int pixel_address = TV_WIDTH * (TV_HEIGHT - 1);

        for (int row = 0; row < TV_WIDTH / 8; row++)
        {
            if (MACHINES <= 3) {
                // non-scrolling menu for 2 or 3 machines
                for (char i = 0; i < sizeof(logos) / sizeof(unsigned short*); i++) {
                    char offset = i * 12;
                    if (sizeof(logos) / sizeof(unsigned short*) == 2) offset += 6;

                    if (row >= offset && row < offset + 12)
                        render_logo(8 * (row - offset), logos[i], menu_sel == i + 1);
                }
            }
            else {
                // scrolling menu for more than 3 machines

                // valid offset values range from 0 to MACHINE*96-1
                static int offset = 0;

                // check which logo would show up in this row. Actually
                // two may show up in the same character row when scrolling
                int logo_idx = ((row + offset / 8) / 12) % MACHINES;
                if (logo_idx < 0) logo_idx += MACHINES;

                int logo_y = (row * 8 + offset) % 96;  // logo line in this row

                // check if logo at logo_y shows up in current row
                render_logo(logo_y, logos[logo_idx], (menu_sel - 1) == logo_idx);

                // check if a second logo may show up here
                if (logo_y > (96 - 8)) {
                    logo_idx = (logo_idx + 1) % MACHINES;
                    logo_y -= 96;
                    render_logo(logo_y, logos[logo_idx], (menu_sel - 1) == logo_idx);
                }

                if (row == 35) {
                    // finally offset is bound to game, something like 96*game:    
                    int new_offset = 96 * ((unsigned)(menu_sel - 2) % MACHINES);
                    if (menu_sel == 1) new_offset = (MACHINES - 1) * 96;

                    // check if we need to scroll
                    if (new_offset != offset) {
                        int diff = (new_offset - offset) % (MACHINES * 96);
                        if (diff < 0) diff += MACHINES * 96;

                        if (diff < MACHINES * 96 / 2) offset = (offset + 8) % (MACHINES * 96);
                        else                     offset = (offset - 8) % (MACHINES * 96);
                        if (offset < 0) offset += MACHINES * 96;
                    }
                }
            }

            // Rotate the row of data to put it into the raster framebuffer
            // Filthy, naughty waste of CPU cycles and RAM, but I'm not rewriting the menu for raster
            unsigned short* vert_ptr = &vert_buffer[0];
            for (int z = 0; z < 8; z++)
            {
                int pixelstart = pixel_address;
                // Copy a vertical line of pixels
                for (int y = 0; y < 224; y++)
                {
                    nextframeptr[pixel_address] = vert_ptr[y];
                    pixel_address -= TV_WIDTH; // go up a line
                }
                // Advance horizontally from where we started
                pixelstart++;
                pixel_address = pixelstart;
                vert_ptr += 224;

            }
        }
    }
#endif

    else
    {
#ifdef ENABLE_PACMAN
        PACMAN_BEGIN
            pacman_render_frame_raster();
        PACMAN_END
#endif

#ifdef ENABLE_GALAGA
            GALAGA_BEGIN
            galaga_render_frame_raster();
        GALAGA_END
#endif

#ifdef ENABLE_DKONG
            DKONG_BEGIN
            dkong_render_frame_raster();
        DKONG_END
#endif

#ifdef ENABLE_FROGGER
            FROGGER_BEGIN
            frogger_render_frame_raster();
        FROGGER_END
#endif

#ifdef ENABLE_DIGDUG
            DIGDUG_BEGIN
            digdug_render_frame_raster();
        DIGDUG_END
#endif

#ifdef ENABLE_1942
            _1942_BEGIN
            _1942_render_frame_raster();
        _1942_END
#endif
    }
}

#ifdef ENABLE_GALAGA
void galaga_trigger_sound_explosion(void) 
{
  if(game_started) {
    snd_boom_cnt = 2*sizeof(galaga_sample_boom);
    snd_boom_ptr = (const signed char*)galaga_sample_boom;
  }
}
#endif

#ifdef USE_NAMCO_WAVETABLE
static unsigned long snd_cnt[3] = { 0,0,0 };
static unsigned long snd_freq[3];
static const signed char *snd_wave[3];
static unsigned char snd_volume[3];
#endif

#ifdef SND_DIFF
//static unsigned short snd_buffer[128]; // buffer space for two channels
#else
static unsigned char* snd_buffer;  // buffer space for a single channel
#endif

void snd_render_buffer(void) {
#if defined(ENABLE_FROGGER) || defined(ENABLE_1942)
  #ifndef ENABLE_1942        // only frogger
    #define AY        1      // frogger has one AY
    #define AY_INC    9      // and it runs at 1.78 MHz -> 223718/24000 = 9,32
    #define AY_VOL   11      // min/max = -/+ 3*15*11 = -/+ 495
  #else
    #ifndef ENABLE_FROGGER   // only 1942  
      #define AY      2      // 1942 has two AYs
      #define AY_INC  8      // and they runs at 1.5 MHz -> 187500/24000 = 7,81
      #define AY_VOL 10      // min/max = -/+ 6*15*11 = -/+ 990
    #else
      // both enabled
      #define AY ((machine == MCH_FROGGER)?1:2)
      #define AY_INC ((machine == MCH_FROGGER)?9:8)
      #define AY_VOL ((machine == MCH_FROGGER)?11:10)
    #endif
  #endif
  
  if(
#ifdef ENABLE_FROGGER
     MACHINE_IS_FROGGER ||
#endif
#ifdef ENABLE_1942
     MACHINE_IS_1942 ||
#endif
     0) {

    // up to two AY's
    for(char ay=0;ay<AY;ay++) {
      int ay_off = 16*ay;

      // three tone channels
      for(char c=0;c<3;c++) {
	ay_period[ay][c] = soundregs[ay_off+2*c] + 256 * (soundregs[ay_off+2*c+1] & 15);
	ay_enable[ay][c] = (((soundregs[ay_off+7] >> c)&1) | ((soundregs[ay_off+7] >> (c+2))&2))^3;
	ay_volume[ay][c] = soundregs[ay_off+8+c] & 0x0f;
      }
      // noise channel
      ay_period[ay][3] = soundregs[ay_off+6] & 0x1f;
    }
  }
#endif

      snd_buffer = &sound_buffer_pointer[sound_buffer_index];

  if (sound_init)
  {
     sound_buffer_index += 64;
     if (sound_buffer_index >4900)
        sound_buffer_index = 4900;

      // render first buffer contents
      for (int i = 0; i < 64; i++) {
          short v = 0;

#if defined(ENABLE_PACMAN) || defined(ENABLE_GALAGA)
#ifndef SINGLE_MACHINE
          if (0
#ifdef ENABLE_PACMAN
              || (machine == MCH_PACMAN)
#endif
#ifdef ENABLE_GALAGA
              || (machine == MCH_GALAGA)
#endif
#ifdef ENABLE_DIGDUG
              || (machine == MCH_DIGDUG)
#endif
              )
#endif
          {
              // add up to three wave signals
              if (snd_volume[0]) v += snd_volume[0] * snd_wave[0][(snd_cnt[0] >> 13) & 0x1f];
              if (snd_volume[1]) v += snd_volume[1] * snd_wave[1][(snd_cnt[1] >> 13) & 0x1f];
              if (snd_volume[2]) v += snd_volume[2] * snd_wave[2][(snd_cnt[2] >> 13) & 0x1f];

#ifdef ENABLE_GALAGA
              if (snd_boom_cnt) {
                  v += *snd_boom_ptr;
                  if (snd_boom_cnt & 1) snd_boom_ptr++;
                  snd_boom_cnt--;
              }
#endif
          }
#endif    

#ifdef ENABLE_DKONG
          DKONG_BEGIN
          {
            v = 0;  // silence

            // no buffer available
            if (dkong_audio_rptr != dkong_audio_wptr)
                // copy data from dkong buffer into tx buffer
                // 8048 sounds gets 50% of the available volume range
        #ifdef WORKAROUND_I2S_APLL_PROBLEM
                v = dkong_audio_transfer_buffer[dkong_audio_rptr][(dkong_obuf_toggle ? 32 : 0) + (i / 2)];
        #else
                v = dkong_audio_transfer_buffer[dkong_audio_rptr][i];
        #endif
            // include sample sounds
            // walk is 6.25% volume, jump is at 12.5% volume and, stomp is at 25%
            for (char j = 0; j < 3; j++) {
              if (dkong_sample_cnt[j]) {
      #ifdef WORKAROUND_I2S_APLL_PROBLEM
                v += *dkong_sample_ptr[j] >> (2 - j);
                if (i & 1) { // advance read pointer every second sample
                  dkong_sample_ptr[j]++;
                  dkong_sample_cnt[j]--;
                }
      #else
                v += *dkong_sample_ptr[j]++ >> (2 - j);
                dkong_sample_cnt[j]--;
      #endif
              }
            }
          }
              DKONG_END
#endif

#if defined(ENABLE_FROGGER) || defined(ENABLE_1942)
              if (
#ifdef ENABLE_FROGGER
                  MACHINE_IS_FROGGER ||
#endif
#ifdef ENABLE_1942
                  MACHINE_IS_1942 ||
#endif
                  0) {
                  v = 0;  // silence

                  for (char ay = 0; ay < AY; ay++) {

                      // frogger can acually skip the noise generator as
                      // it doesn't use it      
                      if (ay_period[ay][3]) {
                          // process noise generator
                          audio_cnt[ay][3] += AY_INC; // for 24 khz
                          if (audio_cnt[ay][3] > ay_period[ay][3]) {
                              audio_cnt[ay][3] -= ay_period[ay][3];
                              // progress rng
                              ay_noise_rng[ay] ^= (((ay_noise_rng[ay] & 1) ^ ((ay_noise_rng[ay] >> 3) & 1)) << 17);
                              ay_noise_rng[ay] >>= 1;
                          }
                      }

                      for (char c = 0; c < 3; c++) {
                          // a channel is on if period != 0, vol != 0 and tone bit == 0
                          if (ay_period[ay][c] && ay_volume[ay][c] && ay_enable[ay][c]) {
                              short bit = 1;
                              if (ay_enable[ay][c] & 1) bit &= (audio_toggle[ay][c] > 0) ? 1 : 0;  // tone
                              if (ay_enable[ay][c] & 2) bit &= (ay_noise_rng[ay] & 1) ? 1 : 0;     // noise

                              if (bit == 0) bit = -1;
                              v += AY_VOL * bit * ay_volume[ay][c];

                              audio_cnt[ay][c] += AY_INC; // for 24 khz
                              if (audio_cnt[ay][c] > ay_period[ay][c]) {
                                  audio_cnt[ay][c] -= ay_period[ay][c];
                                  audio_toggle[ay][c] = -audio_toggle[ay][c];
                              }
                          }
                      }
                  }
              }
#endif
          // v is now in the range of +/- 512, so expand to +/- 15 bit
          //v = v * 64;

          // TODO: Make it 8-bit for Pico2 (for now)
          v = v >>3;

#ifdef SND_DIFF
          // generate differential output
          snd_buffer[2 * i] = 0x8000 + v;    // positive signal on GPIO26
          snd_buffer[2 * i + 1] = 0x8000 - v;    // negatve signal on GPIO25 
#else
          snd_buffer[i] = 0x80 + v;
#endif

#if defined(ENABLE_PACMAN) || defined(ENABLE_GALAGA)
          snd_cnt[0] += snd_freq[0];
          snd_cnt[1] += snd_freq[1];
          snd_cnt[2] += snd_freq[2];
#endif
      }

#ifdef ENABLE_DKONG
#ifndef SINGLE_MACHINE
      if (machine == MCH_DKONG)
#endif
      {
          // advance write pointer. The buffer is a ring
          dkong_audio_rptr = (dkong_audio_rptr + 1) & DKONG_AUDIO_QUEUE_MASK;
      }
#endif
  }
}

#ifdef USE_NAMCO_WAVETABLE
void audio_namco_waveregs_parse(void) {
#ifndef SINGLE_MACHINE
  if(
#ifdef ENABLE_PACMAN
    MACHINE_IS_PACMAN ||
#endif    
#ifdef ENABLE_GALAGA    
    MACHINE_IS_GALAGA ||
#endif
#ifdef ENABLE_DIGDUG    
    MACHINE_IS_DIGDUG ||
#endif
  0) 
#endif  
  {
    // parse all three wsg channels
    for(char ch=0;ch<3;ch++) {  
      // channel volume
      snd_volume[ch] = soundregs[ch * 5 + 0x15];
    
      if(snd_volume[ch]) {
        // frequency
        snd_freq[ch] = (ch == 0) ? soundregs[0x10] : 0;
        snd_freq[ch] += soundregs[ch * 5 + 0x11] << 4;
        snd_freq[ch] += soundregs[ch * 5 + 0x12] << 8;
        snd_freq[ch] += soundregs[ch * 5 + 0x13] << 12;
        snd_freq[ch] += soundregs[ch * 5 + 0x14] << 16;
      
        // wavetable entry
#ifdef ENABLE_PACMAN
  #if defined(ENABLE_GALAGA) || defined(ENABLE_DIGDUG)  // there's at least a second machine
        if(machine == MCH_PACMAN)
  #endif
          snd_wave[ch] = pacman_wavetable[soundregs[ch * 5 + 0x05] & 0x0f];
  #if defined(ENABLE_GALAGA) || defined(ENABLE_DIGDUG)
        else
  #endif      
#endif      
#ifdef ENABLE_GALAGA
  #ifdef ENABLE_DIGDUG
        if(machine == MCH_GALAGA)
  #endif
          snd_wave[ch] = galaga_wavetable[soundregs[ch * 5 + 0x05] & 0x07];
#endif      
#ifdef ENABLE_DIGDUG
          snd_wave[ch] = digdug_wavetable[soundregs[ch * 5 + 0x05] & 0x0f];
#endif      
      }
    }
  }
}
#endif  // MCH_PACMAN || MCH_GALAGA

#ifdef ENABLE_DKONG
void dkong_trigger_sound(char snd) {
  static const struct {
    const signed char *data;
    const unsigned short length; 
  } samples[] = {
    { (const signed char *)dkong_sample_walk0, sizeof(dkong_sample_walk0) },
    { (const signed char *)dkong_sample_walk1, sizeof(dkong_sample_walk1) },
    { (const signed char *)dkong_sample_walk2, sizeof(dkong_sample_walk2) },
    { (const signed char *)dkong_sample_jump,  sizeof(dkong_sample_jump)  },
    { (const signed char *)dkong_sample_stomp, sizeof(dkong_sample_stomp) }
  };

  // samples 0 = walk, 1 = jump, 2 = stomp

  if(!snd) {
    // walk0, walk1 and walk2 are variants
    char rnd = random() % 3;
    dkong_sample_cnt[0] = samples[rnd].length;
    dkong_sample_ptr[0] = samples[rnd].data;
  } else {
    dkong_sample_cnt[snd] = samples[snd+2].length;
    dkong_sample_ptr[snd] = samples[snd+2].data;
  }
}
#endif


void audio_dkong_bitrate(char is_dkong) {
  //  TODO: Needs to be on Pico2!

  // The audio CPU of donkey kong runs at 6Mhz. A full bus
  // cycle needs 15 clocks which results in 400k cycles
  // per second. The sound CPU typically needs 34 instruction
  // cycles to write an updated audio value to the external
  // DAC connected to port 0.
  
  // The effective sample rate thus is 6M/15/34 = 11764.7 Hz

  //  i2s_set_sample_rates(I2S_NUM_0, is_dkong?11765:24000);

}

void audio_init(void) {  
  // init audio
#if defined(ENABLE_PACMAN) || defined(ENABLE_GALAGA)
  audio_namco_waveregs_parse();
#endif
  //snd_render_buffer();

#if defined(SINGLE_MACHINE) && defined(ENABLE_DKONG)
  // only dkong installed? Then setup rate immediately
  audio_dkong_bitrate(true);
#endif

}

void galapico_update_screen(void) {
 // uint32_t t0 = micros();

#ifdef ENABLE_PACMAN
PACMAN_BEGIN
  pacman_prepare_frame();    
PACMAN_END
#endif

#ifdef ENABLE_GALAGA
GALAGA_BEGIN
  galaga_prepare_frame();
GALAGA_END
#endif  
  
#ifdef ENABLE_DKONG
DKONG_BEGIN
  dkong_prepare_frame();
DKONG_END
#endif

#ifdef ENABLE_FROGGER
FROGGER_BEGIN
  frogger_prepare_frame();
FROGGER_END
#endif

#ifdef ENABLE_DIGDUG
DIGDUG_BEGIN
  digdug_prepare_frame();
DIGDUG_END
#endif

#ifdef ENABLE_1942
_1942_BEGIN
  _1942_prepare_frame();
_1942_END
#endif
}

void galapico_render_audio_video(void) 
{
  
    galapico_render_frame();
    audio_namco_waveregs_parse();
    snd_render_buffer();
    snd_render_buffer();
    snd_render_buffer();
    snd_render_buffer();
    snd_render_buffer();
    snd_render_buffer();

   
#ifdef ENABLE_GALAGA
  /* the screen is only updated every second frame, scroll speed is thus doubled */
  static const signed char speeds[8] = { -1, -2, -3, 0, 3, 2, 1, 0 };
  stars_scroll_y += 2*speeds[starcontrol & 7];
#endif
}



unsigned char buttons_get(void) 
{

  unsigned char input_states = 0;

  input_states |= (!gpio_get(B_COIN1)) ? BUTTON_EXTRA : 0;

        
#ifndef SINGLE_MACHINE
#if 0
  static unsigned long reset_timer = 0;

  // reset if coin (or start if no coin is configured) is held for
  // more than 1 second
  if(input_states & BUTTON_EXTRA) {
    if(machine != MCH_MENU) {

      if(!reset_timer)
        reset_timer = millis();

      if(millis() - reset_timer > 1000) {

         emulation_reset();
      }
    }    
  } else
    reset_timer = 0;
#endif
#endif
  
  unsigned char startAndCoinState =
      // there is a coin pin -> coin and start work normal
      (gpio_get(B_START1) ? 0 : BUTTON_START) |
      (gpio_get(B_COIN1) ? 0 : BUTTON_COIN);


      return startAndCoinState |
      (gpio_get(B_LEFT1) ? 0 : BUTTON_LEFT) |
      (gpio_get(B_RIGHT1) ? 0 : BUTTON_RIGHT) |
      (gpio_get(B_UP1) ? 0 : BUTTON_UP) |
      (gpio_get(B_DOWN1) ? 0 : BUTTON_DOWN) |
      (gpio_get(B_1P_1) ? 0 : BUTTON_FIRE);

}



// Sound HAL wants more samples
void galapico_audio_poll(unsigned char* buffer, unsigned short* numsamples)
{
    sound_buffer_pointer = buffer;
    *numsamples = sound_buffer_index;
    sound_buffer_index = 0;
    sound_init = 1;
}


/*
 Interface functions so I don't have to include emulation.c in the pico HAL
 or my PC simulator
 */
void galapico_prepare_emulation(void)
{
    audio_init();

    prepare_emulation();
}
void galapico_emulate_frame(void)
{
    emulate_frame();
}


/* RP 2350 Boot Functions*/
void core1_main()
{
    // initialize audio to default bitrate (24khz unless dkong is
    // the only game installed, then audio will directly be 
    // initialized to dkongs 11765hz)

    // Boot audio from this core (so interrups go to it)
    hal_pwm_audio_init();


    // Go to video loop
    Halvideo_core1();


    // you won't get here as it sits in a loop in the HAL 
    while (1)
    {
        // tight_loop_contents();

    }

}

// Core 0 main
int main()
{
 

    gpio_init(B_COIN1);
    gpio_set_input_enabled(B_COIN1, true);
    gpio_pull_up(B_COIN1);

    gpio_init(B_START1);
    gpio_set_input_enabled(B_START1, true);
    gpio_pull_up(B_START1);

    gpio_init(B_UP1);
    gpio_set_input_enabled(B_UP1, true);
    gpio_pull_up(B_UP1);

    gpio_init(B_DOWN1);
    gpio_set_input_enabled(B_DOWN1, true);
    gpio_pull_up(B_DOWN1);

    gpio_init(B_LEFT1);
    gpio_set_input_enabled(B_LEFT1, true);
    gpio_pull_up(B_LEFT1);

    gpio_init(B_RIGHT1);
    gpio_set_input_enabled(B_RIGHT1, true);
    gpio_pull_up(B_RIGHT1);

    gpio_init(B_1P_1);
    gpio_set_input_enabled(B_1P_1, true);
    gpio_pull_up(B_1P_1);

    gpio_init(B_1P_2);
    gpio_set_input_enabled(B_1P_2, true);
    gpio_pull_up(B_1P_2);


    prepare_emulation(); 

    // Emulator code first
    audio_init();


	// Start the second CPU core (see above function)
    multicore_launch_core1(core1_main);


    // Initialize the 240p screen  
    HAL_video_setFramebuffer();


    while (true)
    {
        // tight_loop_contents();
    }

}




