#pragma once
/* Hardware definitions for stuff like pins and DMA */


/*  Pinouts (please check other documentation first):
    (Resistor values are ideal, can be something close-ish) 
    RGB arrangement allows standard 3:3:2 colour using just the lower 8-bits if you're short on RAM

   GPIO     Purpose 

    0       Red 5 2K
    1       Red 6 1k
    2       Red 7 500R
    3       Green 5 2K
    4       Green 6 1k
    5       Green 7 500
    6       Blue 6 1k
    7       Blue 7 500R

    8       Red 3 (LSB) 8K
    9       Red 4 4K
    10      Green 3 (LSB) 8K
    11      Green 4 4K
    12      Blue 3 (LSB) 8K
    13      Blue 4 4K
    14      Blue 5 2K
    15      CSYNC 470R


    (next side of board)
    16      COIN
    17      START
    18      UP
    19      DOWN
    20      LEFT
    21      RIGHT
    22      BUTTON1
    26      BUTTON2
    28      AUDIO - 500R -> 10nf->220uF 'T' filter (see below)


    Watch for ground pins when counting.
    Make sure you join video ground to the Pico ground.

    Resistors just need to be approx for colours (470R is ok for 250R).
    You can even make them using series and parallel 500R's:  500R || 500R (250R), 500R, 500R*2, 500R*4, 500R*8.
    For a colour, connect the 5 resistors to the pico, then join the other ends and take to the video.

    For audio:
    22 --500R-------(+220uF-)-----amplifier
             |
             10nf
             |
             GND
    500R and 10nf ('103') forms a lowpass filter to remove PWM  ( 1/(2*PI*RC) = 16kHz  )
    220uF is approx (100uF, 470uF are ok) - a chunky electrolytic around that size. Make sure -ve side goes to audio amp line in. 
    Takes out speaker-melting DC

            
*/
/* PINS */

typedef enum {RED_PIN = 0,  
              CSYNC_PIN=15,
              B_COIN1 = 16,
              B_START1 = 17,
              B_UP1 = 18,
              B_DOWN1 = 19,
              B_LEFT1 = 20, 
              B_RIGHT1 = 21,
              B_1P_1= 22,
              B_1P_2 = 26,
              TEST_PIN = 27,
              AUDIO_PIN = 28
              }inputs_t;



#define SPI_PORT spi0


/*  DMA   */

typedef enum
{
	audio_pwm_dma_chan =0,
	audio_trigger_dma_chan =1,
	audio_sample_dma_chan =2,

	rgb_chan_layer_top_data = 3, 
	rgb_chan_layer_top_cfg =4,

	rgb_chan_layer_bottom_data = 5, 
	rgb_chan_layer_bottom_cfg = 6,

	dma_gfx_0 = 7,
	dma_gfx_1 =8,
	dma_gfx_2 =9,
	dma_gfx_3 = 10,
	dma_gfx_4 = 11,
	dma_gfx_5 = 12,
	dma_gfx_6 = 13,
	dma_gfx_7 = 14,
	dma_gfx_8 = 15,
    
}lane_t;