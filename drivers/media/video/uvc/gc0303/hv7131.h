
#ifndef __HV7131_H__
#define __HV7131_H__

int sensor_init(int left, int top, int width, int height, int FPReaderOpt);
void sensor_close(void);
int SetCMOSGain(int Gain);
int IncCMOSGain(void);
int DecCMOSGain(void);
void FilterRGB(char *PixelsBuffer, int Width, int Height);
int Read24WC02(int Address, unsigned char *data, int size);
int Write24WC02(int Address, unsigned  char *data, int size);

#endif
