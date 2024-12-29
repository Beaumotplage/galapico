#pragma once
//TODO: Remove this file, it's here as a reminder of things to fix or bin.

#include <stdlib.h>
//#define uint8_t unsigned char
//#define uint16_t unsigned short
#define byte unsigned char
#define word unsigned short


inline int esp_random()
{
	return 77;
}

inline void vTaskDelay(int x)
{

}

inline void xTaskNotifyGive(int x)
{


}

inline int micros(void)
{
	return 0;

}

inline int millis(void)
{
	return 0;
}
//    i2s_write(I2S_NUM_0, snd_buffer, sizeof(snd_buffer), &bytesOut, 0);

//  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

inline void ulTaskNotifyTake(int x, int y)
{

}
