#ifndef _SB16_H
#define _SB16_H

#define SB16_MIXER_PORT			0x04
#define SB16_MIXER_DATA			0x05
#define SB16_DSP_RESET			0x06

#define SB16_DSP_READ			0x0A
#define SB16_DSP_WRITE			0x0C
#define SB16_DSP_STATUS			0x0E
#define SB16_INTR_ACK			0x0F


#define SB16_DSP_READY			0xAA
#define SB16_DSP_VERSION		0xE1

#define SB16_CMD_SAMPLERATE		0x41
#define SB16_CMD_PLAY			0xA6
#define SB16_CMD_STOP			0xD9
#define SB16_CMD_GETDSP			0xE1


#endif
