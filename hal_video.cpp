/*
* Credit due to:
* https://vanhunteradams.com/Pico/VGA/VGA.html
* (for getting me started on all this)
*/

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "pico/multicore.h"

// Our assembled programs:
// Each gets the name <pio_filename.pio.h>
#include "rgb_notransparency.pio.h"
#include "csync.pio.h"

// Header file
#include "hal_video.h"
#include "hw_defs.h"
#include "galapico.h"

// Manually select a few state machines from pio instance pio0.
uint rgb_top_sm = 3;
//uint rgb_bottom_sm = 2; // no hardware layers used here
uint csync_sm = 0;


void vsync_handler(void); 
void hsync_handler(void);

static volatile bool ready = 0;

//TODO: Remove that 10000, only there because of a memory overflow bug :-/
unsigned short framebuffer[2][TV_HEIGHT*TV_WIDTH+10000];

unsigned short* outputframeptr = &framebuffer[0][0];
unsigned short* nextframeptr = &framebuffer[1][0];


/*
    15-bit Frame buffer mode.
*/
void HAL_video_setFramebuffer(void)
{
    uint rgb_offset = pio_add_program(pio0, &rgb_notransparency_program);
    uint csync_offset = pio_add_program(pio0, &csync_program);

  
    // Call the initialization functions that are defined within each PIO file.
    // Why not create these programs here? By putting the initialization function in
    // the pio file, then all information about how to use/setup that state machine
    // is consolidated in one place. Here in the C, we then just import and use it.
    
    rgb_notransparency_program_init(pio0, rgb_top_sm, rgb_offset, RED_PIN);
    csync_program_init(pio0, csync_sm, csync_offset, CSYNC_PIN); // Two pins used, starts with this one.

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // ============================== PIO DMA Channels =================================================
    /////////////////////////////////////////////////////////////////////////////////////////////////////


    // Channel Zero (sends color data to PIO RGB machine)
    dma_channel_config config0 = dma_channel_get_default_config(rgb_chan_layer_top_data);  // default configs
    channel_config_set_transfer_data_size(&config0, DMA_SIZE_16);             // 16-bit txfers
    channel_config_set_read_increment(&config0, true);                        // yes read incrementing
    channel_config_set_write_increment(&config0, false);                      // no write incrementing
    channel_config_set_dreq(&config0, DREQ_PIO0_TX3) ;                        // DREQ_PIO0_TX2 pacing (FIFO)
    channel_config_set_chain_to(&config0, rgb_chan_layer_top_cfg);            // chain to other channel

    dma_channel_configure(
        rgb_chan_layer_top_data,         // Channel to be configured
        &config0,                     // The configuration we just created
        &pio0->txf[rgb_top_sm],          // write address (RGB PIO TX FIFO)
        outputframeptr,         // The initial read address (pixel color array)
        TV_WIDTH*TV_HEIGHT,           // Number of transfers - e.g. 320x240 .
        false                         // Don't start immediately.
    );

    // Channel One (reconfigures the first channel when it's done streaming all the pixels)
    dma_channel_config config1 = dma_channel_get_default_config(rgb_chan_layer_top_cfg);   // default configs
    channel_config_set_transfer_data_size(&config1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&config1, false);                        // no read incrementing
    channel_config_set_write_increment(&config1, false);                       // no write incrementing
    channel_config_set_chain_to(&config1, rgb_chan_layer_top_data);               // chain to other channel

    dma_channel_configure(
        rgb_chan_layer_top_cfg,                         // Channel to be configured
        &config1,                                // The configuration we just created
        &dma_hw->ch[rgb_chan_layer_top_data].read_addr,  // Write address (channel 0 read address)
        &outputframeptr,                   // Read address (POINTER TO AN ADDRESS) - put it back to the start
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );

    // Prepare the PIOs
    pio_sm_put_blocking(pio0, rgb_top_sm, TV_WIDTH -1);
    pio_sm_put_blocking(pio0, csync_sm, TV_HEIGHT - 1);


    // Start the two pio machine IN SYNC
    // Note that the RGB state machine is running at full speed,
    // so synchronization doesn't matter for that one. But, we'll
    // start them all simultaneously anyway.

    pio_enable_sm_mask_in_sync(pio0, ( (1u << csync_sm) | (1u << rgb_top_sm)));


    // Start DMA channel . Once started, the contents of the pixel color array
    // will be continously DMA'd to the PIO machines that are driving the screen.
    // To change the contents of the screen, we need only change the contents
    // of that array.
  
    dma_start_channel_mask((1u << rgb_chan_layer_top_data)) ;

    
    //Hsync IRQ. NOT USED
#if 0   
    pio_set_irq0_source_enabled(pio0, pis_interrupt1,1) ;
    irq_set_exclusive_handler(PIO0_IRQ_0, hsync_handler);
    irq_set_priority(PIO0_IRQ_0,0);
    irq_set_enabled(PIO0_IRQ_0, false);
#endif


    //Nsync OFF
    pio_set_irq0_source_enabled(pio0, pis_interrupt1,0) ;
    irq_set_enabled(PIO0_IRQ_0, false);

    //Vsync IRQ
    pio_set_irq1_source_enabled(pio0, pis_interrupt0,1) ;
    irq_set_exclusive_handler(PIO0_IRQ_1, vsync_handler);
    irq_set_priority(PIO0_IRQ_1, 255);
    irq_set_enabled(PIO0_IRQ_1, true);

  // Sometimes a pin is handy for scoping interrupt execution durations
    gpio_init(TEST_PIN);
    gpio_set_dir(TEST_PIN, GPIO_OUT);
    gpio_put(TEST_PIN, 1);

}

    
void Halvideo_hsync_start()
{

    irq_set_exclusive_handler(PIO0_IRQ_0, hsync_handler);
    irq_set_priority(PIO0_IRQ_0,0);  
    irq_set_enabled(PIO0_IRQ_0, true);
     
}

extern void galapico_render_audio_video(void) ;

// CPU second core
// Put in an endless loop and told when to draw the video by the main CPU below (vsync handler)
void Halvideo_core1(void)
{
    while (1)
    {
        if (ready)
        {
//            gpio_put(TEST_PIN, 1);
              galapico_render_audio_video();
//            gpio_put(TEST_PIN, 0);

            ready = 0;
        }
    }
}




void hsync_handler(void)
{
    //Not used
}

// 60Hz VSYNC for main CPU
void __no_inline_not_in_flash_func  (vsync_handler)(void) 
{
       gpio_put(TEST_PIN, 1);
        /* See if we've missed a frame - DMA will have incremented*/
        //TODO: This is crap, shakes the screen 
  //  if (dma_hw->ch[rgb_chan_layer_top_data].transfer_count > TV_WIDTH*(TV_HEIGHT-2)  )
    {
        

        // Double frame buffers
        if (outputframeptr == &framebuffer[0][0])
        {
            dma_hw->ch[rgb_chan_layer_top_data].read_addr = (uint32_t) &framebuffer[1][0];

            outputframeptr = &framebuffer[1][0];
            nextframeptr = &framebuffer[0][0];
    
        }
        else
        {
            dma_hw->ch[rgb_chan_layer_top_data].read_addr = (uint32_t) &framebuffer[0][0];

            outputframeptr = &framebuffer[0][0];
            nextframeptr = &framebuffer[1][0];
        }
    

        galapico_update_screen();
        
        ready = 1;

        galapico_emulate_frame();
    }
    gpio_put(TEST_PIN, 0);
}


