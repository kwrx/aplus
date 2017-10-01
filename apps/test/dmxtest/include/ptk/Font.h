#pragma once
#ifndef __cplusplus
#error "C++ Required!"
#endif


#include <iostream>
#include <unordered_map>
#include <cairo/cairo.h>

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



        FT_Face Face;
        FT_F26Dot6 PtSize;
        FT_UInt Dpi;
        
        Font(string family, FT_F26Dot6 ptSize = 12, FT_UInt dpi = 72);

        bool IsLoaded();
        void DrawTo(ptk::Frame* frame, string text, double x = 0.0, double y = 0.0);
        void DrawTo(ptk::Frame* frame, string text, double x, double y, FT_F26Dot6 ptSize, FT_UInt dpi = 72);
    };
}