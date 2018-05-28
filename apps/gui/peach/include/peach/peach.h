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


#ifndef _PEACH_H
#define _PEACH_H

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <errno.h>
#include <cairo/cairo.h>
using namespace std;


#define peach_die(x)                \
    __p_die(x, __FILE__, __LINE__)

#define __p_die(x, y, z)            \
    {                               \
        cerr << "peach: (" << y     \
             << ":" << z << ") "    \
             << x                   \
             << strerror(errno)     \
             << endl;               \
        exit(1);                    \
    }


#define WINDOW_SIZE_W_MIN           200
#define WINDOW_SIZE_H_MIN           50
#define WINDOW_MARGIN_BORDER        30


class Screen;
class Window;
class Renderer;
class Peach;


class Screen {
    public:
        uint16_t Width;
        uint16_t Height;
        uint16_t BitsPerPixel;
        Renderer* Framebuffer;

        Screen(Peach*);
        ~Screen();

        bool SetMode(uint16_t, uint16_t, uint16_t);
        bool GetMode(uint16_t*, uint16_t*, uint16_t*);
    private:
        Peach* context;
        uint16_t backupWidth;
        uint16_t backupHeight;
        uint16_t backupBitsPerPixel;
};

class Window {
    public:
        typedef enum : int {
            Hide = 0,
            Show = 1,
            Focus = 2,
            NoBorder = 4
        } WindowFlags;


        wstring Title;
        uint16_t X;
        uint16_t Y;
        uint16_t Width;
        uint16_t Height;
        WindowFlags Flags;

        bool MinimizeBox;
        bool MaximizeBox;

        Window(Peach*);
        ~Window();
        
        bool Resize(uint16_t, uint16_t);
    private:
        Peach* context;
        Renderer* surface;
};


class Renderer {
    public:
        uint16_t Width;
        uint16_t Height;

        Renderer(Peach*, uint16_t, uint16_t);
        Renderer(Peach*, uint16_t, uint16_t, void*);
        ~Renderer();

    private:
        cairo_surface_t* surface;
        cairo_t* cairo;
        Peach* context;
};


class Peach {
    public:
        Screen* DefaultScreen;

        vector<Window*>* Windows;
        Window* WindowTop;
        Window* WindowFocused;

        Peach();
        ~Peach();
};

#endif