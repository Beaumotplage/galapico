/* Pico2 hardware code to do PWM audio via DMA
   Based on an example from the SDK
*/

extern "C" 
{       

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"

}

#include "galapico.h"
#include "hw_defs.h"


// The fixed location the sample DMA channel writes to and the PWM DMA channel
// reads from
uint32_t single_sample = 0;
uint32_t* single_sample_ptr = &single_sample;

// You get a few PWM cycles per audio sample as PWM is like 100's of KHz
#define REPETITION_RATE 4

static unsigned char audio_buffer[20000];
static unsigned char  audio_buffer2[20000];

/* 
    DMA interrupt
    Fires when the hardware runs out of samples
    Will ask the game code how many more are available and then put them out on the next interation
    Double buffered (no other way)
*/
void dma_irh() {
    static int doublebuffer = 0;
    static unsigned short numsamples = 0;
    dma_hw->ints1 = (1u << audio_trigger_dma_chan);
    if (!doublebuffer)
    {
        galapico_audio_poll(&audio_buffer2[0], &numsamples);

        // TODO: Hacky clamps. Remove me please
        if (numsamples < 1)
            numsamples = 1;

        if (numsamples > 5000)
            numsamples = 5000;

        dma_hw->ch[audio_sample_dma_chan].al1_read_addr = (uint32_t)audio_buffer;
        dma_hw->ch[audio_trigger_dma_chan].transfer_count=  numsamples * REPETITION_RATE;
        dma_hw->ch[audio_trigger_dma_chan].al3_read_addr_trig = (uint32_t)&single_sample_ptr;

        doublebuffer = 1;    
    }
    else
    {
        galapico_audio_poll(&audio_buffer[0], &numsamples);

        
        // Hacky clamps. Remove me please
        if (numsamples < 1)
        numsamples = 1;

        if (numsamples > 5000)
            numsamples = 5000;   
        dma_hw->ch[audio_sample_dma_chan].al1_read_addr = (uint32_t)audio_buffer2;
        dma_hw->ch[audio_trigger_dma_chan].transfer_count= numsamples * REPETITION_RATE;
        dma_hw->ch[audio_trigger_dma_chan].al3_read_addr_trig = (uint32_t)&single_sample_ptr;

        doublebuffer = 0;
    }


}

void hal_pwm_audio_init(void) 
{


    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, (15.0/12.5f) *22.1f / REPETITION_RATE);
    pwm_config_set_wrap(&config, 255);
    pwm_init(audio_pin_slice, &config, true);
    pwm_set_enabled(audio_pin_slice, true);


    pwm_hw->slice[audio_pin_slice].cc = 100; 


    // Setup PWM DMA channel
    dma_channel_config pwm_dma_chan_config = dma_channel_get_default_config(audio_pwm_dma_chan);
    // Transfer 32-bits at a time
    channel_config_set_transfer_data_size(&pwm_dma_chan_config, DMA_SIZE_32);
    // Read from a fixed location, always writes to the same address
    channel_config_set_read_increment(&pwm_dma_chan_config, false);
    channel_config_set_write_increment(&pwm_dma_chan_config, false);
    // Chain to sample DMA channel when done
    channel_config_set_chain_to(&pwm_dma_chan_config, audio_sample_dma_chan);
    // Transfer on PWM cycle end
    channel_config_set_dreq(&pwm_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);

    dma_channel_configure(
        audio_pwm_dma_chan,
        &pwm_dma_chan_config,
        // Write to PWM slice CC register
        &pwm_hw->slice[audio_pin_slice].cc,
        // Read from single_sample
        &single_sample,
        // We might use the same sample a few times as PWM rate is different to audio sample rate
        REPETITION_RATE,
        // Don't start yet
        false
    );

    // Setup trigger DMA channel
    dma_channel_config trigger_dma_chan_config = dma_channel_get_default_config(audio_trigger_dma_chan);
    // Transfer 32-bits at a time
    channel_config_set_transfer_data_size(&trigger_dma_chan_config, DMA_SIZE_32);
    // Always read and write from and to the same address
    channel_config_set_read_increment(&trigger_dma_chan_config, false);
    channel_config_set_write_increment(&trigger_dma_chan_config, false);
    // Transfer on PWM cycle end
    channel_config_set_dreq(&trigger_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);

    dma_channel_configure(
        audio_trigger_dma_chan,
        &trigger_dma_chan_config,
        // Write to PWM DMA channel read address trigger
        &dma_hw->ch[audio_pwm_dma_chan].al3_read_addr_trig,
        // Read from location containing the address of single_sample
        &single_sample_ptr,
        // Need to trigger once for each audio sample but as the PWM DREQ is
        // used need to multiply by repetition rate
        REPETITION_RATE *  sizeof(audio_buffer),
        false
    );

    // Fire interrupt when trigger DMA channel is done
    dma_channel_set_irq1_enabled(audio_trigger_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_irh);
        irq_set_priority(DMA_IRQ_1,255);
    irq_set_enabled(DMA_IRQ_1, true);

    // Setup sample DMA channel
    dma_channel_config sample_dma_chan_config = dma_channel_get_default_config(audio_sample_dma_chan);
    // Transfer 16-bits at a time
    channel_config_set_transfer_data_size(&sample_dma_chan_config, DMA_SIZE_8);
    // Increment read address to go through audio buffer
    channel_config_set_read_increment(&sample_dma_chan_config, true);
    // Always write to the same address
    channel_config_set_write_increment(&sample_dma_chan_config, false);

    dma_channel_configure(
        audio_sample_dma_chan,
        &sample_dma_chan_config,
        // Write to single_sample
        &single_sample,
        // Read from audio buffer
        audio_buffer,
        // Only do one transfer (once per PWM DMA completion due to chaining)
        1,
        // Don't start yet
        false
    );

    // Kick things off with the trigger DMA channel
    dma_channel_start(audio_trigger_dma_chan);

}