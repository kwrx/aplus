#ifndef _APLUS_KD_H
#define _APLUS_KD_H

#define KIOCSOUND           0x4B2F
#define KDMKTONE            0x4B30

#define KDGETLED            0x4B31
#define KDSETLED            0x4B32
#   define LED_SCR          0x01
#   define LED_NUM          0x02
#   define LED_CAP          0x04

#define KDGKBTYPE           0x4B33
#   define KB_84            0x01
#   define KB_101           0x02
#   define KB_OTHER         0x03

#define KDADDIO             0x4B34
#define KDDELIO             0x4B35
#define KDENABIO            0x4B36
#define KDDISABIO           0x4B37

#define KDSETMODE           0x4B3A
#   define KD_TEXT          0x00
#   define KD_GRAPHICS      0x01
#define KDGETMODE           0x4B3B

#define KDMAPDISP           0x4B3C
#define KDUNMAPDISP         0x4B3D

#define KDGKBMODE           0x4B44
#   define K_RAW            0x00
#   define K_XLATE          0x01
#   define K_MEDIUMRAW      0x02
#   define K_UNICODE        0x03
#   define K_OFF            0x04
#define KDSKBMODE           0x4B45

#define KDGKBLED            0x4B50
#define KDSKBLED            0x4B51

#endif