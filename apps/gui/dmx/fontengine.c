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


        dmx_font_t* ft = (dmx_font_t*) calloc(1, sizeof(dmx_font_t));
        if(!ft) {
            TRACE("no memory left!");
            FT_Done_Face(face);
            return;
        }
        

        FT_SfntName sn_family;
        FT_SfntName sn_subfamily;

        if(FT_Get_Sfnt_Name(face, TT_NAME_ID_FONT_FAMILY, &sn_family) != 0) {
            TRACE("FT_Get_Sfnt_Name(FAMILY) failed!");
            continue;
        }
        strncpy(ft->family, sn_family.string, sn_family.string_len);


        if(FT_Get_Sfnt_Name(face, TT_NAME_ID_FONT_SUBFAMILY, &sn_subfamily) != 0) {
            TRACE("FT_Get_Sfnt_Name(SUBFAMILY) failed!");
            continue;
        }
        strncpy(ft->subfamily, sn_subfamily.string, sn_subfamily.string_len);


        FT_Done_Face(face);



        int fd = open(buf, O_RDONLY);
        if(fd < 0) {
            TRACE("%s: could not open\n", buf);
            continue;
        }


        


        lseek(fd, 0, SEEK_END);
        ft->bufsiz = lseek(fd, 0, SEEK_CUR);
        lseek(fd, 0, SEEK_SET);


        ft->buffer = (void*) malloc(ft->bufsiz);
        if(!ft->buffer) {
            TRACE("no memory left!");
            return;
        }

        if(read(fd, ft->buffer, ft->bufsiz) != ft->bufsiz) {
            TRACE("%s: I/O error\n", buf);

            free(ft->buffer);
            free(ft);
            close(fd);
            continue;
        }

        close(fd);
        list_push(dmx->ft_fonts, ft);
    }

    closedir(d);
    return;
}


int init_fontengine(dmx_t* dmx) {
    TRACE("Initializing FontEngine\n");
    FT_Init_FreeType(&dmx->ft_library);


    TRACE("Loading System Fonts...\n");
    __load_from_path(dmx, PATH_FONTS);


    
    #define _(x, y, z)                                                                                              \
        if(dmx_font_obtain(dmx, &dmx->ft_cache[x], (char*) sysconfig(y, SYSCONFIG_FORMAT_STRING, 0), z) != 0) {     \
            TRACE("dmx_font_obtain() failed!\n");                                                                   \
            return -1;                                                                                              \
        }



    TRACE("Loading Cache...\n");


    _(DMX_FONT_TYPE_CONDENSED | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_NORMAL, "ui.font.condensed", "Regular");

    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Regular");
    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Light");
    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Medium");
    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.regular", "Bold");
    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Italic");
    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_LIGHT | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Light Italic");
    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Medium Italic");
    _(DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.regular", "Bold Italic");

    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_NORMAL, "ui.font.monospace", "Regular");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_NORMAL, "ui.font.monospace", "Bold");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_ITALIC, "ui.font.monospace", "Italic");
    _(DMX_FONT_TYPE_MONOSPACE | DMX_FONT_WEIGHT_MEDIUM | DMX_FONT_STYLE_ITALIC, "ui.font.monospace", "Bold Italic");


    TRACE("Done!\n");
    return 0;
}


int dmx_font_obtain(dmx_t* dmx, FT_Face* face, char* family, char* subfamily) {
    dmx_font_t* ft = NULL;
    list_each(dmx->ft_fonts, v) {
        if(strcmp(v->family, family) != 0 || strcmp(v->subfamily, subfamily) != 0)
            continue;
        
        ft = v;
        break;
    }

    if(!ft) {
        TRACE("%s %s not found!\n", family, subfamily);
        return -1;
    }

    if(FT_New_Memory_Face(dmx->ft_library, ft->buffer, ft->bufsiz, 0, face) != 0) {
        TRACE("%s %s failed on FT_New_Face()\n");
        return -1;
    }

    return 0;
}