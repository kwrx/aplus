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



static int load_fontconfig(dmx_t* dmx) {
    FILE* fp = fopen("/etc/fonts/fonts.conf", "r");
    if(!fp) {
        TRACE("/etc/fonts/fonts.conf not found!\n");
        return -1;
    }



    char buf[BUFSIZ];
    while(fgets(buf, BUFSIZ, fp)) {
        if(!strchr(buf, ':')) {
            TRACE("syntax-error: expected \':\' character!");
            break;
        }


        dmx_font_t* d = (dmx_font_t*) calloc(1, sizeof(dmx_font_t));
        if(!d) {
            TRACE("%s: no memory left for new dmx_font\n");
            break;
        }

        strcpy(d->path, strtok(buf, ":"));
        strcpy(d->family, strtok(NULL, ":"));
        strcpy(d->subfamily, strtok(NULL, "\n"));


        list_push(dmx->ft_fonts, d);
    }
    
    fclose(fp);
    return 0;
}


int init_fontengine(dmx_t* dmx) {
    TRACE("Initializing FontEngine\n");
    FT_Init_FreeType(&dmx->ft_library);


    TRACE("Loading System Fonts...\n");
    if(load_fontconfig(dmx) != 0) {
        TRACE("load_fontconfig() failed!\n");
        return -1;
    }


    
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

    if(!ft->cache) {
        FILE* fp = fopen(ft->path, "rb");
        if(!fp) {
            TRACE("%s: could not open for %s %s\n", ft->path, family, subfamily);
            return -1;
        }

        fseek(fp, 0, SEEK_END);
        ft->cachesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        ft->cache = (void*) malloc(ft->cachesize);
        if(!ft->cache) {
            TRACE("no memory left for %s %s\n", family, subfamily);
            fclose(fp);
            return -1;
        }

        fread(ft->cache, 1, ft->cachesize, fp);
        fclose(fp);
    }

    if(FT_New_Memory_Face(dmx->ft_library, ft->cache, ft->cachesize, 0, face) != 0) {
        TRACE("%s %s failed on FT_New_Face()\n");
        return -1;
    }

    return 0;
}