#pragma once
// TODO: Add some comments

#define TV_WIDTH 320
#define GAME_WIDTH 288
#define TV_HEIGHT 224
#define PIXEL_COUNT (TV_WIDTH * TV_HEIGHT)


extern void galapico_prepare_emulation(void);
extern void galapico_emulate_frame(void);
extern void galapico_update_screen(void);
extern void galapico_render_audio_video(void);

extern unsigned short* nextframeptr;

extern int digitalRead(short pin);

//TODO - probably shouldn't live here
extern void galapico_audio_poll(unsigned char* buffer, unsigned short* numsamples);
