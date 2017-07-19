#include "dmx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <aplus/base.h>
#include <aplus/sysconfig.h>
#include <fontconfig/fontconfig.h>


static void __load_from_path(dmx_t* dmx, char* path) {
    DIR* d = opendir(path);
    if(!d) {
        TRACE("opendir(%s) failed!\n", PATH_FONTS);
        return;
    }

    struct dirent* ent;
    while((ent = readdir(d))) {
        char buf[strlen(path) + strlen(ent->d_name) + 1];
        sprintf(buf, "%s/%s", path, ent->d_name);

        if(ent->d_type == DT_DIR) {
            __load_from_path(dmx, buf);
         
            continue;
        }


        FT_Face face;
        if(FT_New_Face(dmx->ft_library, buf, 0, &face) != 0)
            continue;


        
        FT_SfntName sn;
        if(FT_Get_Sfnt_Name(face, TT_NAME_ID_FONT_FAMILY, &sn) != 0)
            continue;

        char tmp[sn.string_len + 1];
        strncpy(tmp, sn.string, sn.string_len);

        if(FT_Get_Sfnt_Name(face, TT_NAME_ID_FONT_SUBFAMILY, &sn) != 0)
            continue;

        char tmp2[sn.string_len + 1];
        strncpy(tmp2, sn.string, sn.string_len);
        TRACE("Loaded %s : %s\n", tmp, tmp2);
    
    }

    closedir(d);
    return;
}


int init_fontengine(dmx_t* dmx) {
    TRACE("Initializing FontEngine\n");
    FT_Init_FreeType(&dmx->ft_library);


    TRACE("Loading System Fonts...\n");

    __load_from_path(dmx, PATH_FONTS);
    return -1;

    
    #define _(x, y, z)                                                                                      \
        if(dmx_font_obtain(&dmx->ft_cache[x], (char*) sysconfig(y, SYSCONFIG_FORMAT_STRING, 0), z) != 0) {  \
            TRACE("dmx_font_obtain() failed!\n");                                                           \
            return -1;                                                                                      \
        }


    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Regular");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_NORMAL, "ui.font.condensed", "Regular");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_NORMAL, "ui.font.monospace", "Regular");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Light");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_NORMAL, "ui.font.condensed", "Light");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_NORMAL, "ui.font.monospace", "Light");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Medium");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.condensed", "Medium");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.monospace", "Medium");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Bold");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.condensed", "Bold");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.monospace", "Bold");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Italic");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_ITALIC, "ui.font.condensed", "Italic");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_ITALIC, "ui.font.monospace", "Italic");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Light Italic");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_ITALIC, "ui.font.condensed", "Light Italic");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_ITALIC, "ui.font.monospace", "Light Italic");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Medium Italic");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.condensed", "Medium Italic");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.monospace", "Medium Italic");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Bold Italic");
    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.condensed", "Bold Italic");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.monospace", "Bold Italic");

    TRACE("Done!\n");
    return 0;
}


int dmx_font_obtain(FT_Face* face, char* family, char* style) {
    
}