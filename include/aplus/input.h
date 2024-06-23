/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2018 Antonino Natale
 *
 *
 * This file is part of aPlus.
 *
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_INPUT_H
#define _APLUS_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define NR_KEYS        256
#define KEY_RESERVED   0
#define KEY_ESC        1
#define KEY_1          2
#define KEY_2          3
#define KEY_3          4
#define KEY_4          5
#define KEY_5          6
#define KEY_6          7
#define KEY_7          8
#define KEY_8          9
#define KEY_9          10
#define KEY_0          11
#define KEY_MINUS      12
#define KEY_EQUAL      13
#define KEY_BACKSPACE  14
#define KEY_TAB        15
#define KEY_Q          16
#define KEY_W          17
#define KEY_E          18
#define KEY_R          19
#define KEY_T          20
#define KEY_Y          21
#define KEY_U          22
#define KEY_I          23
#define KEY_O          24
#define KEY_P          25
#define KEY_LEFTBRACE  26
#define KEY_RIGHTBRACE 27
#define KEY_ENTER      28
#define KEY_LEFTCTRL   29
#define KEY_A          30
#define KEY_S          31
#define KEY_D          32
#define KEY_F          33
#define KEY_G          34
#define KEY_H          35
#define KEY_J          36
#define KEY_K          37
#define KEY_L          38
#define KEY_SEMICOLON  39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE      41
#define KEY_LEFTSHIFT  42
#define KEY_BACKSLASH  43
#define KEY_Z          44
#define KEY_X          45
#define KEY_C          46
#define KEY_V          47
#define KEY_B          48
#define KEY_N          49
#define KEY_M          50
#define KEY_COMMA      51
#define KEY_DOT        52
#define KEY_SLASH      53
#define KEY_RIGHTSHIFT 54
#define KEY_KPASTERISK 55
#define KEY_LEFTALT    56
#define KEY_SPACE      57
#define KEY_CAPSLOCK   58
#define KEY_F1         59
#define KEY_F2         60
#define KEY_F3         61
#define KEY_F4         62
#define KEY_F5         63
#define KEY_F6         64
#define KEY_F7         65
#define KEY_F8         66
#define KEY_F9         67
#define KEY_F10        68
#define KEY_NUMLOCK    69
#define KEY_SCROLLLOCK 70
#define KEY_KP7        71
#define KEY_KP8        72
#define KEY_KP9        73
#define KEY_KPMINUS    74
#define KEY_KP4        75
#define KEY_KP5        76
#define KEY_KP6        77
#define KEY_KPPLUS     78
#define KEY_KP1        79
#define KEY_KP2        80
#define KEY_KP3        81
#define KEY_KP0        82
#define KEY_KPDOT      83

#define KEY_ZENKAKUHANKAKU   85
#define KEY_102ND            86
#define KEY_F11              87
#define KEY_F12              88
#define KEY_RO               89
#define KEY_KATAKANA         90
#define KEY_HIRAGANA         91
#define KEY_HENKAN           92
#define KEY_KATAKANAHIRAGANA 93
#define KEY_MUHENKAN         94
#define KEY_KPJPCOMMA        95
#define KEY_KPENTER          96
#define KEY_RIGHTCTRL        97
#define KEY_KPSLASH          98
#define KEY_SYSRQ            99
#define KEY_RIGHTALT         100
#define KEY_LINEFEED         101
#define KEY_HOME             102
#define KEY_UP               103
#define KEY_PAGEUP           104
#define KEY_LEFT             105
#define KEY_RIGHT            106
#define KEY_END              107
#define KEY_DOWN             108
#define KEY_PAGEDOWN         109
#define KEY_INSERT           110
#define KEY_DELETE           111
#define KEY_MACRO            112
#define KEY_MUTE             113
#define KEY_VOLUMEDOWN       114
#define KEY_VOLUMEUP         115
#define KEY_POWER            116 /* SC System Power Down */
#define KEY_KPEQUAL          117
#define KEY_KPPLUSMINUS      118
#define KEY_PAUSE            119
#define KEY_SCALE            120 /* AL Compiz Scale (Expose) */

#define KEY_KPCOMMA   121
#define KEY_HANGEUL   122
#define KEY_HANGUEL   KEY_HANGEUL
#define KEY_HANJA     123
#define KEY_YEN       124
#define KEY_LEFTMETA  125
#define KEY_RIGHTMETA 126
#define KEY_COMPOSE   127

#define KEY_STOP           128 /* AC Stop */
#define KEY_AGAIN          129
#define KEY_PROPS          130 /* AC Properties */
#define KEY_UNDO           131 /* AC Undo */
#define KEY_FRONT          132
#define KEY_COPY           133 /* AC Copy */
#define KEY_OPEN           134 /* AC Open */
#define KEY_PASTE          135 /* AC Paste */
#define KEY_FIND           136 /* AC Search */
#define KEY_CUT            137 /* AC Cut */
#define KEY_HELP           138 /* AL Integrated Help Center */
#define KEY_MENU           139 /* Menu (show menu) */
#define KEY_CALC           140 /* AL Calculator */
#define KEY_SETUP          141
#define KEY_SLEEP          142 /* SC System Sleep */
#define KEY_WAKEUP         143 /* System Wake Up */
#define KEY_FILE           144 /* AL Local Machine Browser */
#define KEY_SENDFILE       145
#define KEY_DELETEFILE     146
#define KEY_XFER           147
#define KEY_PROG1          148
#define KEY_PROG2          149
#define KEY_WWW            150 /* AL Internet Browser */
#define KEY_MSDOS          151
#define KEY_COFFEE         152 /* AL Terminal Lock/Screensaver */
#define KEY_SCREENLOCK     KEY_COFFEE
#define KEY_ROTATE_DISPLAY 153 /* Display orientation for e.g. tablets */
#define KEY_DIRECTION      KEY_ROTATE_DISPLAY
#define KEY_CYCLEWINDOWS   154
#define KEY_MAIL           155
#define KEY_BOOKMARKS      156 /* AC Bookmarks */
#define KEY_COMPUTER       157
#define KEY_BACK           158 /* AC Back */
#define KEY_FORWARD        159 /* AC Forward */
#define KEY_CLOSECD        160
#define KEY_EJECTCD        161
#define KEY_EJECTCLOSECD   162
#define KEY_NEXTSONG       163
#define KEY_PLAYPAUSE      164
#define KEY_PREVIOUSSONG   165
#define KEY_STOPCD         166
#define KEY_RECORD         167
#define KEY_REWIND         168
#define KEY_PHONE          169 /* Media Select Telephone */
#define KEY_ISO            170
#define KEY_CONFIG         171 /* AL Consumer Control Configuration */
#define KEY_HOMEPAGE       172 /* AC Home */
#define KEY_REFRESH        173 /* AC Refresh */
#define KEY_EXIT           174 /* AC Exit */
#define KEY_MOVE           175
#define KEY_EDIT           176
#define KEY_SCROLLUP       177
#define KEY_SCROLLDOWN     178
#define KEY_KPLEFTPAREN    179
#define KEY_KPRIGHTPAREN   180
#define KEY_NEW            181 /* AC New */
#define KEY_REDO           182 /* AC Redo/Repeat */

#define KEY_F13 183
#define KEY_F14 184
#define KEY_F15 185
#define KEY_F16 186
#define KEY_F17 187
#define KEY_F18 188
#define KEY_F19 189
#define KEY_F20 190
#define KEY_F21 191
#define KEY_F22 192
#define KEY_F23 193
#define KEY_F24 194

#define KEY_PLAYCD         200
#define KEY_PAUSECD        201
#define KEY_PROG3          202
#define KEY_PROG4          203
#define KEY_DASHBOARD      204 /* AL Dashboard */
#define KEY_SUSPEND        205
#define KEY_CLOSE          206 /* AC Close */
#define KEY_PLAY           207
#define KEY_FASTFORWARD    208
#define KEY_BASSBOOST      209
#define KEY_PRINT          210 /* AC Print */
#define KEY_HP             211
#define KEY_CAMERA         212
#define KEY_SOUND          213
#define KEY_QUESTION       214
#define KEY_EMAIL          215
#define KEY_CHAT           216
#define KEY_SEARCH         217
#define KEY_CONNECT        218
#define KEY_FINANCE        219 /* AL Checkbook/Finance */
#define KEY_SPORT          220
#define KEY_SHOP           221
#define KEY_ALTERASE       222
#define KEY_CANCEL         223 /* AC Cancel */
#define KEY_BRIGHTNESSDOWN 224
#define KEY_BRIGHTNESSUP   225
#define KEY_MEDIA          226

#define KEY_SWITCHVIDEOMODE              \
    227 /* Cycle between available video \
           outputs (Monitor/LCD/TV-out/etc) */
#define KEY_KBDILLUMTOGGLE 228
#define KEY_KBDILLUMDOWN   229
#define KEY_KBDILLUMUP     230

#define KEY_SEND        231 /* AC Send */
#define KEY_REPLY       232 /* AC Reply */
#define KEY_FORWARDMAIL 233 /* AC Forward Msg */
#define KEY_SAVE        234 /* AC Save */
#define KEY_DOCUMENTS   235

#define KEY_BATTERY 236

#define KEY_BLUETOOTH 237
#define KEY_WLAN      238
#define KEY_UWB       239

#define KEY_UNKNOWN 240

#define KEY_VIDEO_NEXT       241 /* drive next video source */
#define KEY_VIDEO_PREV       242 /* drive previous video source */
#define KEY_BRIGHTNESS_CYCLE 243 /* brightness up, after max is min */
#define KEY_BRIGHTNESS_AUTO            \
    244 /* Set Auto Brightness: manual \
          brightness control is off,   \
          rely on ambient */
#define KEY_BRIGHTNESS_ZERO KEY_BRIGHTNESS_AUTO
#define KEY_DISPLAY_OFF     245 /* display device to off state */

#define KEY_WWAN   246 /* Wireless WAN (LTE, UMTS, GSM, etc.) */
#define KEY_WIMAX  KEY_WWAN
#define KEY_RFKILL 247 /* Key that controls all radios */

#define KEY_MICMUTE 248 /* Mute / unmute the microphone */

/* Code 255 is reserved for special needs of AT keyboard driver */

#define BTN_MISC 0x100
#define BTN_0    0x100
#define BTN_1    0x101
#define BTN_2    0x102
#define BTN_3    0x103
#define BTN_4    0x104
#define BTN_5    0x105
#define BTN_6    0x106
#define BTN_7    0x107
#define BTN_8    0x108
#define BTN_9    0x109

#define BTN_MOUSE   0x110
#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_MIDDLE  0x112
#define BTN_SIDE    0x113
#define BTN_EXTRA   0x114
#define BTN_FORWARD 0x115
#define BTN_BACK    0x116
#define BTN_TASK    0x117

#define BTN_JOYSTICK 0x120
#define BTN_TRIGGER  0x120
#define BTN_THUMB    0x121
#define BTN_THUMB2   0x122
#define BTN_TOP      0x123
#define BTN_TOP2     0x124
#define BTN_PINKIE   0x125
#define BTN_BASE     0x126
#define BTN_BASE2    0x127
#define BTN_BASE3    0x128
#define BTN_BASE4    0x129
#define BTN_BASE5    0x12a
#define BTN_BASE6    0x12b
#define BTN_DEAD     0x12f

#define BTN_GAMEPAD 0x130
#define BTN_SOUTH   0x130
#define BTN_A       BTN_SOUTH
#define BTN_EAST    0x131
#define BTN_B       BTN_EAST
#define BTN_C       0x132
#define BTN_NORTH   0x133
#define BTN_X       BTN_NORTH
#define BTN_WEST    0x134
#define BTN_Y       BTN_WEST
#define BTN_Z       0x135
#define BTN_TL      0x136
#define BTN_TR      0x137
#define BTN_TL2     0x138
#define BTN_TR2     0x139
#define BTN_SELECT  0x13a
#define BTN_START   0x13b
#define BTN_MODE    0x13c
#define BTN_THUMBL  0x13d
#define BTN_THUMBR  0x13e

#define BTN_DIGI           0x140
#define BTN_TOOL_PEN       0x140
#define BTN_TOOL_RUBBER    0x141
#define BTN_TOOL_BRUSH     0x142
#define BTN_TOOL_PENCIL    0x143
#define BTN_TOOL_AIRBRUSH  0x144
#define BTN_TOOL_FINGER    0x145
#define BTN_TOOL_MOUSE     0x146
#define BTN_TOOL_LENS      0x147
#define BTN_TOOL_QUINTTAP  0x148 /* Five fingers on trackpad */
#define BTN_TOUCH          0x14a
#define BTN_STYLUS         0x14b
#define BTN_STYLUS2        0x14c
#define BTN_TOOL_DOUBLETAP 0x14d
#define BTN_TOOL_TRIPLETAP 0x14e
#define BTN_TOOL_QUADTAP   0x14f /* Four fingers on trackpad */

#define BTN_WHEEL     0x150
#define BTN_GEAR_DOWN 0x150
#define BTN_GEAR_UP   0x151

#define KEY_OK                0x160
#define KEY_SELECT            0x161
#define KEY_GOTO              0x162
#define KEY_CLEAR             0x163
#define KEY_POWER2            0x164
#define KEY_OPTION            0x165
#define KEY_INFO              0x166 /* AL OEM Features/Tips/Tutorial */
#define KEY_TIME              0x167
#define KEY_VENDOR            0x168
#define KEY_ARCHIVE           0x169
#define KEY_PROGRAM           0x16a /* Media Select Program Guide */
#define KEY_CHANNEL           0x16b
#define KEY_FAVORITES         0x16c
#define KEY_EPG               0x16d
#define KEY_PVR               0x16e /* Media Select Home */
#define KEY_MHP               0x16f
#define KEY_LANGUAGE          0x170
#define KEY_TITLE             0x171
#define KEY_SUBTITLE          0x172
#define KEY_ANGLE             0x173
#define KEY_ZOOM              0x174
#define KEY_MODE              0x175
#define KEY_KEYBOARD          0x176
#define KEY_SCREEN            0x177
#define KEY_PC                0x178 /* Media Select Computer */
#define KEY_TV                0x179 /* Media Select TV */
#define KEY_TV2               0x17a /* Media Select Cable */
#define KEY_VCR               0x17b /* Media Select VCR */
#define KEY_VCR2              0x17c /* VCR Plus */
#define KEY_SAT               0x17d /* Media Select Satellite */
#define KEY_SAT2              0x17e
#define KEY_CD                0x17f /* Media Select CD */
#define KEY_TAPE              0x180 /* Media Select Tape */
#define KEY_RADIO             0x181
#define KEY_TUNER             0x182 /* Media Select Tuner */
#define KEY_PLAYER            0x183
#define KEY_TEXT              0x184
#define KEY_DVD               0x185 /* Media Select DVD */
#define KEY_AUX               0x186
#define KEY_MP3               0x187
#define KEY_AUDIO             0x188 /* AL Audio Browser */
#define KEY_VIDEO             0x189 /* AL Movie Browser */
#define KEY_DIRECTORY         0x18a
#define KEY_LIST              0x18b
#define KEY_MEMO              0x18c /* Media Select Messages */
#define KEY_CALENDAR          0x18d
#define KEY_RED               0x18e
#define KEY_GREEN             0x18f
#define KEY_YELLOW            0x190
#define KEY_BLUE              0x191
#define KEY_CHANNELUP         0x192 /* Channel Increment */
#define KEY_CHANNELDOWN       0x193 /* Channel Decrement */
#define KEY_FIRST             0x194
#define KEY_LAST              0x195 /* Recall Last */
#define KEY_AB                0x196
#define KEY_NEXT              0x197
#define KEY_RESTART           0x198
#define KEY_SLOW              0x199
#define KEY_SHUFFLE           0x19a
#define KEY_BREAK             0x19b
#define KEY_PREVIOUS          0x19c
#define KEY_DIGITS            0x19d
#define KEY_TEEN              0x19e
#define KEY_TWEN              0x19f
#define KEY_VIDEOPHONE        0x1a0 /* Media Select Video Phone */
#define KEY_GAMES             0x1a1 /* Media Select Games */
#define KEY_ZOOMIN            0x1a2 /* AC Zoom In */
#define KEY_ZOOMOUT           0x1a3 /* AC Zoom Out */
#define KEY_ZOOMRESET         0x1a4 /* AC Zoom */
#define KEY_WORDPROCESSOR     0x1a5 /* AL Word Processor */
#define KEY_EDITOR            0x1a6 /* AL Text Editor */
#define KEY_SPREADSHEET       0x1a7 /* AL Spreadsheet */
#define KEY_GRAPHICSEDITOR    0x1a8 /* AL Graphics Editor */
#define KEY_PRESENTATION      0x1a9 /* AL Presentation App */
#define KEY_DATABASE          0x1aa /* AL Database App */
#define KEY_NEWS              0x1ab /* AL Newsreader */
#define KEY_VOICEMAIL         0x1ac /* AL Voicemail */
#define KEY_ADDRESSBOOK       0x1ad /* AL Contacts/Address Book */
#define KEY_MESSENGER         0x1ae /* AL Instant Messaging */
#define KEY_DISPLAYTOGGLE     0x1af /* Turn display (LCD) on and off */
#define KEY_BRIGHTNESS_TOGGLE KEY_DISPLAYTOGGLE
#define KEY_SPELLCHECK        0x1b0 /* AL Spell Check */
#define KEY_LOGOFF            0x1b1 /* AL Logoff */

#define KEY_DOLLAR 0x1b2
#define KEY_EURO   0x1b3

#define KEY_FRAMEBACK      0x1b4 /* Consumer - transport controls */
#define KEY_FRAMEFORWARD   0x1b5
#define KEY_CONTEXT_MENU   0x1b6 /* GenDesc - system context menu */
#define KEY_MEDIA_REPEAT   0x1b7 /* Consumer - transport control */
#define KEY_10CHANNELSUP   0x1b8 /* 10 channels up (10+) */
#define KEY_10CHANNELSDOWN 0x1b9 /* 10 channels down (10-) */
#define KEY_IMAGES         0x1ba /* AL Image Browser */

#define KEY_DEL_EOL  0x1c0
#define KEY_DEL_EOS  0x1c1
#define KEY_INS_LINE 0x1c2
#define KEY_DEL_LINE 0x1c3

#define KEY_FN     0x1d0
#define KEY_FN_ESC 0x1d1
#define KEY_FN_F1  0x1d2
#define KEY_FN_F2  0x1d3
#define KEY_FN_F3  0x1d4
#define KEY_FN_F4  0x1d5
#define KEY_FN_F5  0x1d6
#define KEY_FN_F6  0x1d7
#define KEY_FN_F7  0x1d8
#define KEY_FN_F8  0x1d9
#define KEY_FN_F9  0x1da
#define KEY_FN_F10 0x1db
#define KEY_FN_F11 0x1dc
#define KEY_FN_F12 0x1dd
#define KEY_FN_1   0x1de
#define KEY_FN_2   0x1df
#define KEY_FN_D   0x1e0
#define KEY_FN_E   0x1e1
#define KEY_FN_F   0x1e2
#define KEY_FN_S   0x1e3
#define KEY_FN_B   0x1e4

#define KEY_BRL_DOT1  0x1f1
#define KEY_BRL_DOT2  0x1f2
#define KEY_BRL_DOT3  0x1f3
#define KEY_BRL_DOT4  0x1f4
#define KEY_BRL_DOT5  0x1f5
#define KEY_BRL_DOT6  0x1f6
#define KEY_BRL_DOT7  0x1f7
#define KEY_BRL_DOT8  0x1f8
#define KEY_BRL_DOT9  0x1f9
#define KEY_BRL_DOT10 0x1fa

#define KEY_NUMERIC_0     0x200 /* used by phones, remote controls, */
#define KEY_NUMERIC_1     0x201 /* and other keypads */
#define KEY_NUMERIC_2     0x202
#define KEY_NUMERIC_3     0x203
#define KEY_NUMERIC_4     0x204
#define KEY_NUMERIC_5     0x205
#define KEY_NUMERIC_6     0x206
#define KEY_NUMERIC_7     0x207
#define KEY_NUMERIC_8     0x208
#define KEY_NUMERIC_9     0x209
#define KEY_NUMERIC_STAR  0x20a
#define KEY_NUMERIC_POUND 0x20b
#define KEY_NUMERIC_A     0x20c /* Phone key A - HUT Telephony 0xb9 */
#define KEY_NUMERIC_B     0x20d
#define KEY_NUMERIC_C     0x20e
#define KEY_NUMERIC_D     0x20f

#define KEY_CAMERA_FOCUS 0x210
#define KEY_WPS_BUTTON   0x211 /* WiFi Protected Setup key */

#define KEY_TOUCHPAD_TOGGLE 0x212 /* Request switch touchpad on or off */
#define KEY_TOUCHPAD_ON     0x213
#define KEY_TOUCHPAD_OFF    0x214

#define KEY_CAMERA_ZOOMIN  0x215
#define KEY_CAMERA_ZOOMOUT 0x216
#define KEY_CAMERA_UP      0x217
#define KEY_CAMERA_DOWN    0x218
#define KEY_CAMERA_LEFT    0x219
#define KEY_CAMERA_RIGHT   0x21a

#define KEY_ATTENDANT_ON     0x21b
#define KEY_ATTENDANT_OFF    0x21c
#define KEY_ATTENDANT_TOGGLE 0x21d /* Attendant call on or off */
#define KEY_LIGHTS_TOGGLE    0x21e /* Reading light on or off */

#define BTN_DPAD_UP    0x220
#define BTN_DPAD_DOWN  0x221
#define BTN_DPAD_LEFT  0x222
#define BTN_DPAD_RIGHT 0x223

#define KEY_ALS_TOGGLE 0x230 /* Ambient light sensor */

#define KEY_BUTTONCONFIG 0x240 /* AL Button Configuration */
#define KEY_TASKMANAGER  0x241 /* AL Task/Project Manager */
#define KEY_JOURNAL      0x242 /* AL Log/Journal/Timecard */
#define KEY_CONTROLPANEL 0x243 /* AL Control Panel */
#define KEY_APPSELECT    0x244 /* AL Select Task/Application */
#define KEY_SCREENSAVER  0x245 /* AL Screen Saver */
#define KEY_VOICECOMMAND 0x246 /* Listening Voice Command */
#define KEY_ASSISTANT    0x247 /* AL Context-aware desktop assistant */

#define KEY_BRIGHTNESS_MIN 0x250 /* Set Brightness to Minimum */
#define KEY_BRIGHTNESS_MAX 0x251 /* Set Brightness to Maximum */

#define KEY_KBDINPUTASSIST_PREV      0x260
#define KEY_KBDINPUTASSIST_NEXT      0x261
#define KEY_KBDINPUTASSIST_PREVGROUP 0x262
#define KEY_KBDINPUTASSIST_NEXTGROUP 0x263
#define KEY_KBDINPUTASSIST_ACCEPT    0x264
#define KEY_KBDINPUTASSIST_CANCEL    0x265

/* Diagonal movement keys */
#define KEY_RIGHT_UP   0x266
#define KEY_RIGHT_DOWN 0x267
#define KEY_LEFT_UP    0x268
#define KEY_LEFT_DOWN  0x269

#define KEY_ROOT_MENU 0x26a /* Show Device's Root Menu */
/* Show Top Menu of the Media (e.g. DVD) */
#define KEY_MEDIA_TOP_MENU 0x26b
#define KEY_NUMERIC_11     0x26c
#define KEY_NUMERIC_12     0x26d
/*
 * Toggle Audio Description                                            : refers to an audio service that helps blind and
 * visually impaired consumers understand the action in a program. Note: in
 * some countries this is referred to as "Video Description".
 */
#define KEY_AUDIO_DESC    0x26e
#define KEY_3D_MODE       0x26f
#define KEY_NEXT_FAVORITE 0x270
#define KEY_STOP_RECORD   0x271
#define KEY_PAUSE_RECORD  0x272
#define KEY_VOD           0x273 /* Video on Demand */
#define KEY_UNMUTE        0x274
#define KEY_FASTREVERSE   0x275
#define KEY_SLOWREVERSE   0x276
/*
 * Control a data application associated with the currently viewed channel,
 * e.g. teletext or data broadcast application (MHEG, MHP, HbbTV, etc.)
 */
#define KEY_DATA              0x277
#define KEY_ONSCREEN_KEYBOARD 0x278

#define BTN_TRIGGER_HAPPY   0x2c0
#define BTN_TRIGGER_HAPPY1  0x2c0
#define BTN_TRIGGER_HAPPY2  0x2c1
#define BTN_TRIGGER_HAPPY3  0x2c2
#define BTN_TRIGGER_HAPPY4  0x2c3
#define BTN_TRIGGER_HAPPY5  0x2c4
#define BTN_TRIGGER_HAPPY6  0x2c5
#define BTN_TRIGGER_HAPPY7  0x2c6
#define BTN_TRIGGER_HAPPY8  0x2c7
#define BTN_TRIGGER_HAPPY9  0x2c8
#define BTN_TRIGGER_HAPPY10 0x2c9
#define BTN_TRIGGER_HAPPY11 0x2ca
#define BTN_TRIGGER_HAPPY12 0x2cb
#define BTN_TRIGGER_HAPPY13 0x2cc
#define BTN_TRIGGER_HAPPY14 0x2cd
#define BTN_TRIGGER_HAPPY15 0x2ce
#define BTN_TRIGGER_HAPPY16 0x2cf
#define BTN_TRIGGER_HAPPY17 0x2d0
#define BTN_TRIGGER_HAPPY18 0x2d1
#define BTN_TRIGGER_HAPPY19 0x2d2
#define BTN_TRIGGER_HAPPY20 0x2d3
#define BTN_TRIGGER_HAPPY21 0x2d4
#define BTN_TRIGGER_HAPPY22 0x2d5
#define BTN_TRIGGER_HAPPY23 0x2d6
#define BTN_TRIGGER_HAPPY24 0x2d7
#define BTN_TRIGGER_HAPPY25 0x2d8
#define BTN_TRIGGER_HAPPY26 0x2d9
#define BTN_TRIGGER_HAPPY27 0x2da
#define BTN_TRIGGER_HAPPY28 0x2db
#define BTN_TRIGGER_HAPPY29 0x2dc
#define BTN_TRIGGER_HAPPY30 0x2dd
#define BTN_TRIGGER_HAPPY31 0x2de
#define BTN_TRIGGER_HAPPY32 0x2df
#define BTN_TRIGGER_HAPPY33 0x2e0
#define BTN_TRIGGER_HAPPY34 0x2e1
#define BTN_TRIGGER_HAPPY35 0x2e2
#define BTN_TRIGGER_HAPPY36 0x2e3
#define BTN_TRIGGER_HAPPY37 0x2e4
#define BTN_TRIGGER_HAPPY38 0x2e5
#define BTN_TRIGGER_HAPPY39 0x2e6
#define BTN_TRIGGER_HAPPY40 0x2e7

/* We avoid low common keys in module aliases so they don't get huge. */
#define KEY_MIN_INTERESTING KEY_MUTE
#define KEY_MAX             0x2FF
#define KEY_CNT             (KEY_MAX + 1)



#define KT_LATIN  0 /* we depend on this being zero */
#define KT_FN     1
#define KT_SPEC   2
#define KT_PAD    3
#define KT_DEAD   4
#define KT_CONS   5
#define KT_CUR    6
#define KT_SHIFT  7
#define KT_META   8
#define KT_ASCII  9
#define KT_LOCK   10
#define KT_LETTER 11 /* symbol that can be acted upon by CapsLock */
#define KT_SLOCK  12
#define KT_DEAD2  13
#define KT_BRL    14


#define K(t, v) (((t) << 8) | (v))
#define KTYP(x) ((x) >> 8)
#define KVAL(x) ((x) & 0xFF)



#define KG_SHIFT     0
#define KG_CTRL      2
#define KG_ALT       3
#define KG_ALTGR     1
#define KG_SHIFTL    4
#define KG_KANASHIFT 4
#define KG_SHIFTR    5
#define KG_CTRLL     6
#define KG_CTRLR     7
#define KG_CAPSSHIFT 8

#define NR_SHIFT 9



#define K_F1     K(KT_FN, 0)
#define K_F2     K(KT_FN, 1)
#define K_F3     K(KT_FN, 2)
#define K_F4     K(KT_FN, 3)
#define K_F5     K(KT_FN, 4)
#define K_F6     K(KT_FN, 5)
#define K_F7     K(KT_FN, 6)
#define K_F8     K(KT_FN, 7)
#define K_F9     K(KT_FN, 8)
#define K_F10    K(KT_FN, 9)
#define K_F11    K(KT_FN, 10)
#define K_F12    K(KT_FN, 11)
#define K_F13    K(KT_FN, 12)
#define K_F14    K(KT_FN, 13)
#define K_F15    K(KT_FN, 14)
#define K_F16    K(KT_FN, 15)
#define K_F17    K(KT_FN, 16)
#define K_F18    K(KT_FN, 17)
#define K_F19    K(KT_FN, 18)
#define K_F20    K(KT_FN, 19)
#define K_FIND   K(KT_FN, 20)
#define K_INSERT K(KT_FN, 21)
#define K_REMOVE K(KT_FN, 22)
#define K_SELECT K(KT_FN, 23)
#define K_PGUP   K(KT_FN, 24) /* PGUP is a synonym for PRIOR */
#define K_PGDN   K(KT_FN, 25) /* PGDN is a synonym for NEXT */
#define K_MACRO  K(KT_FN, 26)
#define K_HELP   K(KT_FN, 27)
#define K_DO     K(KT_FN, 28)
#define K_PAUSE  K(KT_FN, 29)
#define K_F21    K(KT_FN, 30)
#define K_F22    K(KT_FN, 31)
#define K_F23    K(KT_FN, 32)
#define K_F24    K(KT_FN, 33)
#define K_F25    K(KT_FN, 34)
#define K_F26    K(KT_FN, 35)
#define K_F27    K(KT_FN, 36)
#define K_F28    K(KT_FN, 37)
#define K_F29    K(KT_FN, 38)
#define K_F30    K(KT_FN, 39)
#define K_F31    K(KT_FN, 40)
#define K_F32    K(KT_FN, 41)
#define K_F33    K(KT_FN, 42)
#define K_F34    K(KT_FN, 43)
#define K_F35    K(KT_FN, 44)
#define K_F36    K(KT_FN, 45)
#define K_F37    K(KT_FN, 46)
#define K_F38    K(KT_FN, 47)
#define K_F39    K(KT_FN, 48)
#define K_F40    K(KT_FN, 49)
#define K_F41    K(KT_FN, 50)
#define K_F42    K(KT_FN, 51)
#define K_F43    K(KT_FN, 52)
#define K_F44    K(KT_FN, 53)
#define K_F45    K(KT_FN, 54)
#define K_F46    K(KT_FN, 55)
#define K_F47    K(KT_FN, 56)
#define K_F48    K(KT_FN, 57)
#define K_F49    K(KT_FN, 58)
#define K_F50    K(KT_FN, 59)
#define K_F51    K(KT_FN, 60)
#define K_F52    K(KT_FN, 61)
#define K_F53    K(KT_FN, 62)
#define K_F54    K(KT_FN, 63)
#define K_F55    K(KT_FN, 64)
#define K_F56    K(KT_FN, 65)
#define K_F57    K(KT_FN, 66)
#define K_F58    K(KT_FN, 67)
#define K_F59    K(KT_FN, 68)
#define K_F60    K(KT_FN, 69)
#define K_F61    K(KT_FN, 70)
#define K_F62    K(KT_FN, 71)
#define K_F63    K(KT_FN, 72)
#define K_F64    K(KT_FN, 73)
#define K_F65    K(KT_FN, 74)
#define K_F66    K(KT_FN, 75)
#define K_F67    K(KT_FN, 76)
#define K_F68    K(KT_FN, 77)
#define K_F69    K(KT_FN, 78)
#define K_F70    K(KT_FN, 79)
#define K_F71    K(KT_FN, 80)
#define K_F72    K(KT_FN, 81)
#define K_F73    K(KT_FN, 82)
#define K_F74    K(KT_FN, 83)
#define K_F75    K(KT_FN, 84)
#define K_F76    K(KT_FN, 85)
#define K_F77    K(KT_FN, 86)
#define K_F78    K(KT_FN, 87)
#define K_F79    K(KT_FN, 88)
#define K_F80    K(KT_FN, 89)
#define K_F81    K(KT_FN, 90)
#define K_F82    K(KT_FN, 91)
#define K_F83    K(KT_FN, 92)
#define K_F84    K(KT_FN, 93)
#define K_F85    K(KT_FN, 94)
#define K_F86    K(KT_FN, 95)
#define K_F87    K(KT_FN, 96)
#define K_F88    K(KT_FN, 97)
#define K_F89    K(KT_FN, 98)
#define K_F90    K(KT_FN, 99)
#define K_F91    K(KT_FN, 100)
#define K_F92    K(KT_FN, 101)
#define K_F93    K(KT_FN, 102)
#define K_F94    K(KT_FN, 103)
#define K_F95    K(KT_FN, 104)
#define K_F96    K(KT_FN, 105)
#define K_F97    K(KT_FN, 106)
#define K_F98    K(KT_FN, 107)
#define K_F99    K(KT_FN, 108)
#define K_F100   K(KT_FN, 109)
#define K_F101   K(KT_FN, 110)
#define K_F102   K(KT_FN, 111)
#define K_F103   K(KT_FN, 112)
#define K_F104   K(KT_FN, 113)
#define K_F105   K(KT_FN, 114)
#define K_F106   K(KT_FN, 115)
#define K_F107   K(KT_FN, 116)
#define K_F108   K(KT_FN, 117)
#define K_F109   K(KT_FN, 118)
#define K_F110   K(KT_FN, 119)
#define K_F111   K(KT_FN, 120)
#define K_F112   K(KT_FN, 121)
#define K_F113   K(KT_FN, 122)
#define K_F114   K(KT_FN, 123)
#define K_F115   K(KT_FN, 124)
#define K_F116   K(KT_FN, 125)
#define K_F117   K(KT_FN, 126)
#define K_F118   K(KT_FN, 127)
#define K_F119   K(KT_FN, 128)
#define K_F120   K(KT_FN, 129)
#define K_F121   K(KT_FN, 130)
#define K_F122   K(KT_FN, 131)
#define K_F123   K(KT_FN, 132)
#define K_F124   K(KT_FN, 133)
#define K_F125   K(KT_FN, 134)
#define K_F126   K(KT_FN, 135)
#define K_F127   K(KT_FN, 136)
#define K_F128   K(KT_FN, 137)
#define K_F129   K(KT_FN, 138)
#define K_F130   K(KT_FN, 139)
#define K_F131   K(KT_FN, 140)
#define K_F132   K(KT_FN, 141)
#define K_F133   K(KT_FN, 142)
#define K_F134   K(KT_FN, 143)
#define K_F135   K(KT_FN, 144)
#define K_F136   K(KT_FN, 145)
#define K_F137   K(KT_FN, 146)
#define K_F138   K(KT_FN, 147)
#define K_F139   K(KT_FN, 148)
#define K_F140   K(KT_FN, 149)
#define K_F141   K(KT_FN, 150)
#define K_F142   K(KT_FN, 151)
#define K_F143   K(KT_FN, 152)
#define K_F144   K(KT_FN, 153)
#define K_F145   K(KT_FN, 154)
#define K_F146   K(KT_FN, 155)
#define K_F147   K(KT_FN, 156)
#define K_F148   K(KT_FN, 157)
#define K_F149   K(KT_FN, 158)
#define K_F150   K(KT_FN, 159)
#define K_F151   K(KT_FN, 160)
#define K_F152   K(KT_FN, 161)
#define K_F153   K(KT_FN, 162)
#define K_F154   K(KT_FN, 163)
#define K_F155   K(KT_FN, 164)
#define K_F156   K(KT_FN, 165)
#define K_F157   K(KT_FN, 166)
#define K_F158   K(KT_FN, 167)
#define K_F159   K(KT_FN, 168)
#define K_F160   K(KT_FN, 169)
#define K_F161   K(KT_FN, 170)
#define K_F162   K(KT_FN, 171)
#define K_F163   K(KT_FN, 172)
#define K_F164   K(KT_FN, 173)
#define K_F165   K(KT_FN, 174)
#define K_F166   K(KT_FN, 175)
#define K_F167   K(KT_FN, 176)
#define K_F168   K(KT_FN, 177)
#define K_F169   K(KT_FN, 178)
#define K_F170   K(KT_FN, 179)
#define K_F171   K(KT_FN, 180)
#define K_F172   K(KT_FN, 181)
#define K_F173   K(KT_FN, 182)
#define K_F174   K(KT_FN, 183)
#define K_F175   K(KT_FN, 184)
#define K_F176   K(KT_FN, 185)
#define K_F177   K(KT_FN, 186)
#define K_F178   K(KT_FN, 187)
#define K_F179   K(KT_FN, 188)
#define K_F180   K(KT_FN, 189)
#define K_F181   K(KT_FN, 190)
#define K_F182   K(KT_FN, 191)
#define K_F183   K(KT_FN, 192)
#define K_F184   K(KT_FN, 193)
#define K_F185   K(KT_FN, 194)
#define K_F186   K(KT_FN, 195)
#define K_F187   K(KT_FN, 196)
#define K_F188   K(KT_FN, 197)
#define K_F189   K(KT_FN, 198)
#define K_F190   K(KT_FN, 199)
#define K_F191   K(KT_FN, 200)
#define K_F192   K(KT_FN, 201)
#define K_F193   K(KT_FN, 202)
#define K_F194   K(KT_FN, 203)
#define K_F195   K(KT_FN, 204)
#define K_F196   K(KT_FN, 205)
#define K_F197   K(KT_FN, 206)
#define K_F198   K(KT_FN, 207)
#define K_F199   K(KT_FN, 208)
#define K_F200   K(KT_FN, 209)
#define K_F201   K(KT_FN, 210)
#define K_F202   K(KT_FN, 211)
#define K_F203   K(KT_FN, 212)
#define K_F204   K(KT_FN, 213)
#define K_F205   K(KT_FN, 214)
#define K_F206   K(KT_FN, 215)
#define K_F207   K(KT_FN, 216)
#define K_F208   K(KT_FN, 217)
#define K_F209   K(KT_FN, 218)
#define K_F210   K(KT_FN, 219)
#define K_F211   K(KT_FN, 220)
#define K_F212   K(KT_FN, 221)
#define K_F213   K(KT_FN, 222)
#define K_F214   K(KT_FN, 223)
#define K_F215   K(KT_FN, 224)
#define K_F216   K(KT_FN, 225)
#define K_F217   K(KT_FN, 226)
#define K_F218   K(KT_FN, 227)
#define K_F219   K(KT_FN, 228)
#define K_F220   K(KT_FN, 229)
#define K_F221   K(KT_FN, 230)
#define K_F222   K(KT_FN, 231)
#define K_F223   K(KT_FN, 232)
#define K_F224   K(KT_FN, 233)
#define K_F225   K(KT_FN, 234)
#define K_F226   K(KT_FN, 235)
#define K_F227   K(KT_FN, 236)
#define K_F228   K(KT_FN, 237)
#define K_F229   K(KT_FN, 238)
#define K_F230   K(KT_FN, 239)
#define K_F231   K(KT_FN, 240)
#define K_F232   K(KT_FN, 241)
#define K_F233   K(KT_FN, 242)
#define K_F234   K(KT_FN, 243)
#define K_F235   K(KT_FN, 244)
#define K_F236   K(KT_FN, 245)
#define K_F237   K(KT_FN, 246)
#define K_F238   K(KT_FN, 247)
#define K_F239   K(KT_FN, 248)
#define K_F240   K(KT_FN, 249)
#define K_F241   K(KT_FN, 250)
#define K_F242   K(KT_FN, 251)
#define K_F243   K(KT_FN, 252)
#define K_F244   K(KT_FN, 253)
#define K_F245   K(KT_FN, 254)
#define K_UNDO   K(KT_FN, 255)


#define K_HOLE         K(KT_SPEC, 0)
#define K_ENTER        K(KT_SPEC, 1)
#define K_SH_REGS      K(KT_SPEC, 2)
#define K_SH_MEM       K(KT_SPEC, 3)
#define K_SH_STAT      K(KT_SPEC, 4)
#define K_BREAK        K(KT_SPEC, 5)
#define K_CONS         K(KT_SPEC, 6)
#define K_CAPS         K(KT_SPEC, 7)
#define K_NUM          K(KT_SPEC, 8)
#define K_HOLD         K(KT_SPEC, 9)
#define K_SCROLLFORW   K(KT_SPEC, 10)
#define K_SCROLLBACK   K(KT_SPEC, 11)
#define K_BOOT         K(KT_SPEC, 12)
#define K_CAPSON       K(KT_SPEC, 13)
#define K_COMPOSE      K(KT_SPEC, 14)
#define K_SAK          K(KT_SPEC, 15)
#define K_DECRCONSOLE  K(KT_SPEC, 16)
#define K_INCRCONSOLE  K(KT_SPEC, 17)
#define K_SPAWNCONSOLE K(KT_SPEC, 18)
#define K_BARENUMLOCK  K(KT_SPEC, 19)

#define K_ALLOCATED K(KT_SPEC, 126) /* dynamically allocated keymap */
#define K_NOSUCHMAP K(KT_SPEC, 127) /* returned by KDGKBENT */

#define K_P0         K(KT_PAD, 0)
#define K_P1         K(KT_PAD, 1)
#define K_P2         K(KT_PAD, 2)
#define K_P3         K(KT_PAD, 3)
#define K_P4         K(KT_PAD, 4)
#define K_P5         K(KT_PAD, 5)
#define K_P6         K(KT_PAD, 6)
#define K_P7         K(KT_PAD, 7)
#define K_P8         K(KT_PAD, 8)
#define K_P9         K(KT_PAD, 9)
#define K_PPLUS      K(KT_PAD, 10) /* key-pad plus */
#define K_PMINUS     K(KT_PAD, 11) /* key-pad minus */
#define K_PSTAR      K(KT_PAD, 12) /* key-pad asterisk (star) */
#define K_PSLASH     K(KT_PAD, 13) /* key-pad slash */
#define K_PENTER     K(KT_PAD, 14) /* key-pad enter */
#define K_PCOMMA     K(KT_PAD, 15) /* key-pad comma: kludge... */
#define K_PDOT       K(KT_PAD, 16) /* key-pad dot (period): kludge... */
#define K_PPLUSMINUS K(KT_PAD, 17) /* key-pad plus/minus */
#define K_PPARENL    K(KT_PAD, 18) /* key-pad left parenthesis */
#define K_PPARENR    K(KT_PAD, 19) /* key-pad right parenthesis */

#define NR_PAD 20


#define K_DGRAVE      K(KT_DEAD, 0)
#define K_DACUTE      K(KT_DEAD, 1)
#define K_DCIRCM      K(KT_DEAD, 2)
#define K_DTILDE      K(KT_DEAD, 3)
#define K_DDIERE      K(KT_DEAD, 4)
#define K_DCEDIL      K(KT_DEAD, 5)
#define K_DMACRON     K(KT_DEAD, 6)
#define K_DBREVE      K(KT_DEAD, 7)
#define K_DABDOT      K(KT_DEAD, 8)
#define K_DABRING     K(KT_DEAD, 9)
#define K_DDBACUTE    K(KT_DEAD, 10)
#define K_DCARON      K(KT_DEAD, 11)
#define K_DOGONEK     K(KT_DEAD, 12)
#define K_DIOTA       K(KT_DEAD, 13)
#define K_DVOICED     K(KT_DEAD, 14)
#define K_DSEMVOICED  K(KT_DEAD, 15)
#define K_DBEDOT      K(KT_DEAD, 16)
#define K_DHOOK       K(KT_DEAD, 17)
#define K_DHORN       K(KT_DEAD, 18)
#define K_DSTROKE     K(KT_DEAD, 19)
#define K_DABCOMMA    K(KT_DEAD, 20)
#define K_DABREVCOMMA K(KT_DEAD, 21)
#define K_DDBGRAVE    K(KT_DEAD, 22)
#define K_DINVBREVE   K(KT_DEAD, 23)
#define K_DBECOMMA    K(KT_DEAD, 24)
#define K_DCURRENCY   K(KT_DEAD, 25)
#define K_DGREEK      K(KT_DEAD, 26)

#define NR_DEAD 27


#define K_DOWN  K(KT_CUR, 0)
#define K_LEFT  K(KT_CUR, 1)
#define K_RIGHT K(KT_CUR, 2)
#define K_UP    K(KT_CUR, 3)

#define K_SHIFT     K(KT_SHIFT, KG_SHIFT)
#define K_CTRL      K(KT_SHIFT, KG_CTRL)
#define K_ALT       K(KT_SHIFT, KG_ALT)
#define K_ALTGR     K(KT_SHIFT, KG_ALTGR)
#define K_SHIFTL    K(KT_SHIFT, KG_SHIFTL)
#define K_SHIFTR    K(KT_SHIFT, KG_SHIFTR)
#define K_CTRLL     K(KT_SHIFT, KG_CTRLL)
#define K_CTRLR     K(KT_SHIFT, KG_CTRLR)
#define K_CAPSSHIFT K(KT_SHIFT, KG_CAPSSHIFT)

#define K_ASC0 K(KT_ASCII, 0)
#define K_ASC1 K(KT_ASCII, 1)
#define K_ASC2 K(KT_ASCII, 2)
#define K_ASC3 K(KT_ASCII, 3)
#define K_ASC4 K(KT_ASCII, 4)
#define K_ASC5 K(KT_ASCII, 5)
#define K_ASC6 K(KT_ASCII, 6)
#define K_ASC7 K(KT_ASCII, 7)
#define K_ASC8 K(KT_ASCII, 8)
#define K_ASC9 K(KT_ASCII, 9)
#define K_HEX0 K(KT_ASCII, 10)
#define K_HEX1 K(KT_ASCII, 11)
#define K_HEX2 K(KT_ASCII, 12)
#define K_HEX3 K(KT_ASCII, 13)
#define K_HEX4 K(KT_ASCII, 14)
#define K_HEX5 K(KT_ASCII, 15)
#define K_HEX6 K(KT_ASCII, 16)
#define K_HEX7 K(KT_ASCII, 17)
#define K_HEX8 K(KT_ASCII, 18)
#define K_HEX9 K(KT_ASCII, 19)
#define K_HEXa K(KT_ASCII, 20)
#define K_HEXb K(KT_ASCII, 21)
#define K_HEXc K(KT_ASCII, 22)
#define K_HEXd K(KT_ASCII, 23)
#define K_HEXe K(KT_ASCII, 24)
#define K_HEXf K(KT_ASCII, 25)

#define NR_ASCII 26


#define K_SHIFTLOCK     K(KT_LOCK, KG_SHIFT)
#define K_CTRLLOCK      K(KT_LOCK, KG_CTRL)
#define K_ALTLOCK       K(KT_LOCK, KG_ALT)
#define K_ALTGRLOCK     K(KT_LOCK, KG_ALTGR)
#define K_SHIFTLLOCK    K(KT_LOCK, KG_SHIFTL)
#define K_SHIFTRLOCK    K(KT_LOCK, KG_SHIFTR)
#define K_CTRLLLOCK     K(KT_LOCK, KG_CTRLL)
#define K_CTRLRLOCK     K(KT_LOCK, KG_CTRLR)
#define K_CAPSSHIFTLOCK K(KT_LOCK, KG_CAPSSHIFT)

#define K_SHIFT_SLOCK     K(KT_SLOCK, KG_SHIFT)
#define K_CTRL_SLOCK      K(KT_SLOCK, KG_CTRL)
#define K_ALT_SLOCK       K(KT_SLOCK, KG_ALT)
#define K_ALTGR_SLOCK     K(KT_SLOCK, KG_ALTGR)
#define K_SHIFTL_SLOCK    K(KT_SLOCK, KG_SHIFTL)
#define K_SHIFTR_SLOCK    K(KT_SLOCK, KG_SHIFTR)
#define K_CTRLL_SLOCK     K(KT_SLOCK, KG_CTRLL)
#define K_CTRLR_SLOCK     K(KT_SLOCK, KG_CTRLR)
#define K_CAPSSHIFT_SLOCK K(KT_SLOCK, KG_CAPSSHIFT)

#define NR_LOCK 9


#define K_BRL_BLANK K(KT_BRL, 0)
#define K_BRL_DOT1  K(KT_BRL, 1)
#define K_BRL_DOT2  K(KT_BRL, 2)
#define K_BRL_DOT3  K(KT_BRL, 3)
#define K_BRL_DOT4  K(KT_BRL, 4)
#define K_BRL_DOT5  K(KT_BRL, 5)
#define K_BRL_DOT6  K(KT_BRL, 6)
#define K_BRL_DOT7  K(KT_BRL, 7)
#define K_BRL_DOT8  K(KT_BRL, 8)
#define K_BRL_DOT9  K(KT_BRL, 9)
#define K_BRL_DOT10 K(KT_BRL, 10)

#define NR_BRL 11


#ifdef __cplusplus
}
#endif
#endif
