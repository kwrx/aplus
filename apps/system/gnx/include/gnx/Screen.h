#ifndef _GNX_SCREEN_H
#define _GNX_SCREEN_H

#include <iostream>
#include <gnx/Window.h>
#include <SDL2/SDL.h>

namespace GNX {
    class Screen {
        public:
            static int Width;
            static int Height;
            static int Bpp;
            static int Stride;
            static void* FrameBuffer;
            
            static Window* CreateWindow();
    };
}

#endif