#pragma once
#ifndef __cplusplus
#error "C++ Required!"
#endif


#include <iostream>
#include <unordered_map>
#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>

#include <ptk/Frame.h>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

using namespace std;


namespace ptk {
    class Font {
    public:
        static FT_Library Library;
        static unordered_map<string, FT_Face> Cache;
        static unordered_map<string, string> Config;

        static int Initialize(void);
        static int Done(void);
        static string GetFontPath(string family);

        static cairo_font_face_t* Load(string family, int flags = 0);
    };
}