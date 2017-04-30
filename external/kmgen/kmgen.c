#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


#define KEYMAPS_PATH        "../../bin/usr/share/keymaps/"

#define DEF_KEY(x)  \
    { #x, x },


static struct {
    char* name;
    int value;
} vkeys[] = {

    DEF_KEY(VK_NULL)

    /* Mouse */
    DEF_KEY(VK_LBUTTON        )
    DEF_KEY(VK_RBUTTON        )
    DEF_KEY(VK_CANCEL         )
    DEF_KEY(VK_MBUTTON        )
    DEF_KEY(VK_XBUTTON1       )
    DEF_KEY(VK_XBUTTON2       )


    /* Keybaord */
    DEF_KEY(VK_BACK           )
    DEF_KEY(VK_TAB            )
    DEF_KEY(VK_CLEAR          )
    DEF_KEY(VK_RETURN         )
    DEF_KEY(VK_SHIFT          )
    DEF_KEY(VK_CONTROL        )
    DEF_KEY(VK_MENU           )
    DEF_KEY(VK_PAUSE          )
    DEF_KEY(VK_CAPITAL        )

    DEF_KEY(VK_KANA           )
    DEF_KEY(VK_HANGEUL        )
    DEF_KEY(VK_HANGUL         )
    DEF_KEY(VK_JUNJA          )
    DEF_KEY(VK_FINAL          )
    DEF_KEY(VK_HANJA          )
    DEF_KEY(VK_KANJI          )



    DEF_KEY(VK_ESCAPE         )
    DEF_KEY(VK_CONVERT        )
    DEF_KEY(VK_NONCONVERT     )
    DEF_KEY(VK_ACCEPT         )
    DEF_KEY(VK_MODECHANGE     )
    DEF_KEY(VK_SPACE          )
    DEF_KEY(VK_PRIOR          )
    DEF_KEY(VK_NEXT           )
    DEF_KEY(VK_END            )
    DEF_KEY(VK_HOME           )
    DEF_KEY(VK_LEFT           )
    DEF_KEY(VK_UP             )
    DEF_KEY(VK_RIGHT          )
    DEF_KEY(VK_DOWN           )
    DEF_KEY(VK_SELECT         )
    DEF_KEY(VK_PRINT          )
    DEF_KEY(VK_EXECUTE        )
    DEF_KEY(VK_SNAPSHOT       )
    DEF_KEY(VK_INSERT         )
    DEF_KEY(VK_DELETE         )
    DEF_KEY(VK_HELP           )


    DEF_KEY(VK_0              )
    DEF_KEY(VK_1              )
    DEF_KEY(VK_2              )
    DEF_KEY(VK_3              )
    DEF_KEY(VK_4              )
    DEF_KEY(VK_5              )
    DEF_KEY(VK_6              )
    DEF_KEY(VK_7              )
    DEF_KEY(VK_8              )
    DEF_KEY(VK_9              )

    DEF_KEY(VK_A              )
    DEF_KEY(VK_B              )
    DEF_KEY(VK_C              )
    DEF_KEY(VK_D              )
    DEF_KEY(VK_E              )
    DEF_KEY(VK_F              )
    DEF_KEY(VK_G              )
    DEF_KEY(VK_H              )
    DEF_KEY(VK_I              )
    DEF_KEY(VK_J              )
    DEF_KEY(VK_K              )
    DEF_KEY(VK_L              )
    DEF_KEY(VK_M              )
    DEF_KEY(VK_N              )
    DEF_KEY(VK_O              )
    DEF_KEY(VK_P              )
    DEF_KEY(VK_Q              )
    DEF_KEY(VK_R              )
    DEF_KEY(VK_S              )
    DEF_KEY(VK_T              )
    DEF_KEY(VK_U              )
    DEF_KEY(VK_V              )
    DEF_KEY(VK_W			  )
    DEF_KEY(VK_X              )
    DEF_KEY(VK_Y              )
    DEF_KEY(VK_Z              )


    DEF_KEY(VK_LWIN           )
    DEF_KEY(VK_RWIN           )
    DEF_KEY(VK_APPS           )
    DEF_KEY(VK_SLEEP          )

    DEF_KEY(VK_NUMPAD0        )
    DEF_KEY(VK_NUMPAD1        )
    DEF_KEY(VK_NUMPAD2        )
    DEF_KEY(VK_NUMPAD3        )
    DEF_KEY(VK_NUMPAD4        )
    DEF_KEY(VK_NUMPAD5        )
    DEF_KEY(VK_NUMPAD6        )
    DEF_KEY(VK_NUMPAD7        )
    DEF_KEY(VK_NUMPAD8        )
    DEF_KEY(VK_NUMPAD9        )
    DEF_KEY(VK_MULTIPLY       )
    DEF_KEY(VK_ADD            )
    DEF_KEY(VK_SEPARATOR      )
    DEF_KEY(VK_SUBTRACT       )
    DEF_KEY(VK_DECIMAL        )
    DEF_KEY(VK_DIVIDE         )
    DEF_KEY(VK_F1             )
    DEF_KEY(VK_F2             )
    DEF_KEY(VK_F3             )
    DEF_KEY(VK_F4             )
    DEF_KEY(VK_F5             )
    DEF_KEY(VK_F6             )
    DEF_KEY(VK_F7             )
    DEF_KEY(VK_F8             )
    DEF_KEY(VK_F9             )
    DEF_KEY(VK_F10            )
    DEF_KEY(VK_F11            )
    DEF_KEY(VK_F12            )
    DEF_KEY(VK_F13            )
    DEF_KEY(VK_F14            )
    DEF_KEY(VK_F15            )
    DEF_KEY(VK_F16            )
    DEF_KEY(VK_F17            )
    DEF_KEY(VK_F18            )
    DEF_KEY(VK_F19            )
    DEF_KEY(VK_F20            )
    DEF_KEY(VK_F21            )
    DEF_KEY(VK_F22            )
    DEF_KEY(VK_F23            )
    DEF_KEY(VK_F24            )



    DEF_KEY(VK_NUMLOCK        )
    DEF_KEY(VK_SCROLL         )


    DEF_KEY(VK_LSHIFT         )
    DEF_KEY(VK_RSHIFT         )
    DEF_KEY(VK_LCONTROL       )
    DEF_KEY(VK_RCONTROL       )
    DEF_KEY(VK_LMENU          )
    DEF_KEY(VK_RMENU          )



    DEF_KEY(VK_OEM_1          )
    DEF_KEY(VK_OEM_PLUS       )
    DEF_KEY(VK_OEM_COMMA      )
    DEF_KEY(VK_OEM_MINUS      )
    DEF_KEY(VK_OEM_PERIOD     )
    DEF_KEY(VK_OEM_2          )
    DEF_KEY(VK_OEM_3          )
    DEF_KEY(VK_OEM_4          )
    DEF_KEY(VK_OEM_5          )
    DEF_KEY(VK_OEM_6          )
    DEF_KEY(VK_OEM_7          )
    DEF_KEY(VK_OEM_8          )
    DEF_KEY(VK_OEM_AX         )
    DEF_KEY(VK_OEM_102        )


    DEF_KEY(VK_BROWSER_BACK        )
    DEF_KEY(VK_BROWSER_FORWARD     )
    DEF_KEY(VK_BROWSER_REFRESH     )
    DEF_KEY(VK_BROWSER_STOP        )
    DEF_KEY(VK_BROWSER_SEARCH      )
    DEF_KEY(VK_BROWSER_FAVORITES   )
    DEF_KEY(VK_BROWSER_HOME        )

    DEF_KEY(VK_VOLUME_MUTE         )
    DEF_KEY(VK_VOLUME_DOWN         )
    DEF_KEY(VK_VOLUME_UP           )
    DEF_KEY(VK_MEDIA_NEXT_TRACK    )
    DEF_KEY(VK_MEDIA_PREV_TRACK    )
    DEF_KEY(VK_MEDIA_STOP          )
    DEF_KEY(VK_MEDIA_PLAY_PAUSE    )
    DEF_KEY(VK_LAUNCH_MAIL         )
    DEF_KEY(VK_LAUNCH_MEDIA_SELECT )
    DEF_KEY(VK_LAUNCH_APP1         )
    DEF_KEY(VK_LAUNCH_APP2         )

    DEF_KEY(VK_PAGE_UP             )
    DEF_KEY(VK_PAGE_DOWN           )




    /* Gamepad */
    DEF_KEY(VK_GAMEPAD_A                         )
    DEF_KEY(VK_GAMEPAD_B                         )
    DEF_KEY(VK_GAMEPAD_X                         )
    DEF_KEY(VK_GAMEPAD_Y                         )
    DEF_KEY(VK_GAMEPAD_RIGHT_SHOULDER            )
    DEF_KEY(VK_GAMEPAD_LEFT_SHOULDER             )
    DEF_KEY(VK_GAMEPAD_LEFT_TRIGGER              )
    DEF_KEY(VK_GAMEPAD_RIGHT_TRIGGER             )
    DEF_KEY(VK_GAMEPAD_DPAD_UP                   )
    DEF_KEY(VK_GAMEPAD_DPAD_DOWN                 )
    DEF_KEY(VK_GAMEPAD_DPAD_LEFT                 )
    DEF_KEY(VK_GAMEPAD_DPAD_RIGHT                )
    DEF_KEY(VK_GAMEPAD_MENU                      )
    DEF_KEY(VK_GAMEPAD_VIEW                      )
    DEF_KEY(VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON    )
    DEF_KEY(VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON   )
    DEF_KEY(VK_GAMEPAD_LEFT_THUMBSTICK_UP        )
    DEF_KEY(VK_GAMEPAD_LEFT_THUMBSTICK_DOWN      )
    DEF_KEY(VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT     )
    DEF_KEY(VK_GAMEPAD_LEFT_THUMBSTICK_LEFT      )
    DEF_KEY(VK_GAMEPAD_RIGHT_THUMBSTICK_UP       )
    DEF_KEY(VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN     )
    DEF_KEY(VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT    )
    DEF_KEY(VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT     )
    { NULL, 0 }
};


int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "No output name given!");
        return -1;
    }



    char* keymap[1024] = { 0 };
    memset(keymap, 0, sizeof(keymap));


    char* output = argv[1];
    if(!output || strcmp(output, "") == 0) {
        fprintf(stderr, "Invalid output name!");
        return -1;
    }


    char buf[BUFSIZ] = { 0 };
    memset(buf, 0, BUFSIZ);
    sprintf(buf, "%s.c", output);

    FILE* fp = fopen(buf, "w");
    if(!fp) {
        perror(buf);
        return -1;
    }


    fprintf(fp, "#include <stdio.h>\n"
                "#include <string.h>\n"
                "#include <stdlib.h>\n"
                "\n\n"
                "static char keymap[1024] = {\n"
    );


    int i;
    for(i = 0; vkeys[i].name; i++)
        keymap[vkeys[i].value] = vkeys[i].name;

    int j;
    for(j = 0; j < 4; j++) {
        fprintf(fp, "\n/* %d-%d */\n", j * 256, j * 256 + 256);

        for(i = 0; i < 256; i++) {
            if(keymap[i])
                fprintf(fp, "\t%s,\n", keymap[i]);
            else
                fprintf(fp, "\t0,\n");
        }
    }

    fprintf(fp, "}\n"
                "\n\n"
                "int main(int argc, char** argv) {\n"
    );

    fprintf(fp, "\tFILE* fp = fopen(\"%s%s\", \"wb\");\n", KEYMAPS_PATH, output);

    fprintf(fp, "\tif(!fp)\n\t\tabort();\n"
                "\tfwrite(keymap, 1024, 1, fp);\n"
    );

    fprintf(fp, "\tfclose(fp);\n"
                "}\n"
    );

    fclose(fp);
    return 0;
}