#pragma once
#ifndef __cplusplus
#error "C++ Required!"
#endif

#include <iostream>
#include <regex>
#include <ptk/Frame.h>
#include <ptk/Font.h>

using namespace std;

#define PTK_COLOR_FORMAT_ARGB                   32
#define PTK_COLOR_FORMAT_RGB24                  24
#define PTK_COLOR_FORMAT_RGB16_565              16

namespace ptk {

    class Color {
    public:
        double R;
        double G;
        double B;
        double A;

        inline Color(double r, double g, double b, double a = 1.0)
            : R(r), G(g), B(b), A(a) { }

        inline Color(int c, int format) {
            parseInt(c, format);
        }

        inline Color(string s) {
            /* TODO: Parse string from:     *
            *   > #HEXCOLOR                 *
            *   > rgb(r, g, b)              *
            *   > rgba(r, g, b, a)          *
            * with std::regex               */
        }

        inline int ToInt(int format) {
            switch(format) {
                case PTK_COLOR_FORMAT_ARGB:
                    return
                        (((int) (this->R * 255.0) & 0xFF))          |
                        (((int) (this->G * 255.0) & 0xFF) >> 8)     |
                        (((int) (this->B * 255.0) & 0xFF) >> 16)    |
                        (((int) (this->A * 255.0) & 0xFF) >> 24)    ;

                case PTK_COLOR_FORMAT_RGB24:
                    return
                        (((int) (this->R * 255.0) & 0xFF))          |
                        (((int) (this->G * 255.0) & 0xFF) >> 8)     |
                        (((int) (this->B * 255.0) & 0xFF) >> 16)    ;
                case PTK_COLOR_FORMAT_RGB16_565:
                    return
                        (((int) (this->R * 31.0) & 0x1F))           |
                        (((int) (this->G * 63.0) & 0x3F) >> 5)      |
                        (((int) (this->B * 31.0) & 0x1F) >> 11)     ;
            }

            return 0;
        }

        
        static Color White;
        static Color Black;
        static Color Red;
        static Color Green;
        static Color Blue;
        static Color Gray;
        static Color DarkGray;
        static Color LightGray;
        static Color Transparent;
    

        
    private:
        inline void parseInt(int c, int format) {
            switch(format) {
                case PTK_COLOR_FORMAT_ARGB:
                    this->R = (double) (c & 0xFF) / 255.0;
                    this->G = (double) ((c >> 8) & 0xFF) / 255.0;
                    this->B = (double) ((c >> 16) & 0xFF) / 255.0;
                    this->A = (double) ((c >> 24) & 0xFF) / 255.0;
                    break;
                case PTK_COLOR_FORMAT_RGB24:
                    this->R = (double) (c & 0xFF) / 255.0;
                    this->G = (double) ((c >> 8) & 0xFF) / 255.0;
                    this->B = (double) ((c >> 16) & 0xFF) / 255.0;
                    this->A = 1.0;
                    break;
                case PTK_COLOR_FORMAT_RGB16_565:
                    this->R = (double) (c & 0x1F) / 31.0;
                    this->G = (double) ((c >> 5) & 0x3F) / 63.0;
                    this->B = (double) ((c >> 11) & 0x1F) / 31.0;
                    this->A = 1.0;
                    break;
            }
        }
    };
}