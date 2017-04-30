#ifndef _APLUS_INPUT_H
#define _APLUS_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct mouse {
    uint8_t buttons[5];
    uint16_t speed;

    uint16_t x;
    uint16_t y;
    uint16_t z;

    int16_t dx;
    int16_t dy;
    int16_t dz;

    struct {
        uint16_t left;
        uint16_t top;
        uint16_t right;
        uint16_t bottom;
    } clip;

    uint8_t pack[4];
    uint8_t cycle;
} __attribute__((packed)) mouse_t;





#define VK_NULL			  0x00

/* Mouse */
#define VK_LBUTTON        0x01
#define VK_RBUTTON        0x02
#define VK_CANCEL         0x03
#define VK_MBUTTON        0x04
#define VK_XBUTTON1       0x05
#define VK_XBUTTON2       0x06


/* Keybaord */
#define VK_BACK           0x08
#define VK_TAB            0x09
#define VK_CLEAR          0x0C
#define VK_RETURN         0x0D
#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14

#define VK_KANA           0x15
#define VK_HANGEUL        0x15
#define VK_HANGUL         0x15
#define VK_JUNJA          0x17
#define VK_FINAL          0x18
#define VK_HANJA          0x19
#define VK_KANJI          0x19



#define VK_ESCAPE         0x1B
#define VK_CONVERT        0x1C
#define VK_NONCONVERT     0x1D
#define VK_ACCEPT         0x1E
#define VK_MODECHANGE     0x1F
#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SELECT         0x29
#define VK_PRINT          0x2A
#define VK_EXECUTE        0x2B
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_HELP           0x2F


#define VK_0              0x30
#define VK_1              0x31
#define VK_2              0x32
#define VK_3              0x33
#define VK_4              0x34
#define VK_5              0x35
#define VK_6              0x36
#define VK_7              0x37
#define VK_8              0x38
#define VK_9              0x39

#define VK_A              0x41
#define VK_B              0x42
#define VK_C              0x43
#define VK_D              0x44
#define VK_E              0x45
#define VK_F              0x46
#define VK_G              0x47
#define VK_H              0x48
#define VK_I              0x49
#define VK_J              0x4A
#define VK_K              0x4B
#define VK_L              0x4C
#define VK_M              0x4D
#define VK_N              0x4E
#define VK_O              0x4F
#define VK_P              0x50
#define VK_Q              0x51
#define VK_R              0x52
#define VK_S              0x53
#define VK_T              0x54
#define VK_U              0x55
#define VK_V              0x56
#define VK_W			  0x57
#define VK_X              0x58
#define VK_Y              0x59
#define VK_Z              0x5A


#define VK_LWIN           0x5B
#define VK_RWIN           0x5C
#define VK_APPS           0x5D
#define VK_SLEEP          0x5F

#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_F13            0x7C
#define VK_F14            0x7D
#define VK_F15            0x7E
#define VK_F16            0x7F
#define VK_F17            0x80
#define VK_F18            0x81
#define VK_F19            0x82
#define VK_F20            0x83
#define VK_F21            0x84
#define VK_F22            0x85
#define VK_F23            0x86
#define VK_F24            0x87



#define VK_NUMLOCK        0x90
#define VK_SCROLL         0x91


#define VK_LSHIFT         0xA0
#define VK_RSHIFT         0xA1
#define VK_LCONTROL       0xA2
#define VK_RCONTROL       0xA3
#define VK_LMENU          0xA4
#define VK_RMENU          0xA5



#define VK_OEM_1          0xBA   // ';:' for US
#define VK_OEM_PLUS       0xBB   // '+=' any country
#define VK_OEM_COMMA      0xBC   // ',' any country
#define VK_OEM_MINUS      0xBD   // '-' any country
#define VK_OEM_PERIOD     0xBE   // '.' any country
#define VK_OEM_2          0xBF   // '/?' for US
#define VK_OEM_3          0xC0   // '`~' for US
#define VK_OEM_4          0xDB   // '[{' for US
#define VK_OEM_5          0xDC   // '\|' for US
#define VK_OEM_6          0xDD   // ']}' for US
#define VK_OEM_7          0xDE   // ''"' for US
#define VK_OEM_8          0xDF   // '§!' for US
#define VK_OEM_AX         0xE1   // 'AX' key on Japanese AX kbd
#define VK_OEM_102        0xE2   // "<>" or "\|" on RT 102-key kbd.


#define VK_BROWSER_BACK        0xA6
#define VK_BROWSER_FORWARD     0xA7
#define VK_BROWSER_REFRESH     0xA8
#define VK_BROWSER_STOP        0xA9
#define VK_BROWSER_SEARCH      0xAA
#define VK_BROWSER_FAVORITES   0xAB
#define VK_BROWSER_HOME        0xAC

#define VK_VOLUME_MUTE         0xAD
#define VK_VOLUME_DOWN         0xAE
#define VK_VOLUME_UP           0xAF
#define VK_MEDIA_NEXT_TRACK    0xB0
#define VK_MEDIA_PREV_TRACK    0xB1
#define VK_MEDIA_STOP          0xB2
#define VK_MEDIA_PLAY_PAUSE    0xB3
#define VK_LAUNCH_MAIL         0xB4
#define VK_LAUNCH_MEDIA_SELECT 0xB5
#define VK_LAUNCH_APP1         0xB6
#define VK_LAUNCH_APP2         0xB7

#define VK_PAGE_UP             0xB8
#define VK_PAGE_DOWN           0xB9




/* Gamepad */
#define VK_GAMEPAD_A                         0xC3
#define VK_GAMEPAD_B                         0xC4
#define VK_GAMEPAD_X                         0xC5
#define VK_GAMEPAD_Y                         0xC6
#define VK_GAMEPAD_RIGHT_SHOULDER            0xC7
#define VK_GAMEPAD_LEFT_SHOULDER             0xC8
#define VK_GAMEPAD_LEFT_TRIGGER              0xC9
#define VK_GAMEPAD_RIGHT_TRIGGER             0xCA
#define VK_GAMEPAD_DPAD_UP                   0xCB
#define VK_GAMEPAD_DPAD_DOWN                 0xCC
#define VK_GAMEPAD_DPAD_LEFT                 0xCD
#define VK_GAMEPAD_DPAD_RIGHT                0xCE
#define VK_GAMEPAD_MENU                      0xCF
#define VK_GAMEPAD_VIEW                      0xD0
#define VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON    0xD1
#define VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON   0xD2
#define VK_GAMEPAD_LEFT_THUMBSTICK_UP        0xD3
#define VK_GAMEPAD_LEFT_THUMBSTICK_DOWN      0xD4
#define VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT     0xD5
#define VK_GAMEPAD_LEFT_THUMBSTICK_LEFT      0xD6
#define VK_GAMEPAD_RIGHT_THUMBSTICK_UP       0xD7
#define VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN     0xD8
#define VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT    0xD9
#define VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT     0xDA


#define VK_RELEASED                          0xFF



#ifdef __cplusplus
}
#endif

#endif
