#ifndef _GNX_WINDOW_H
#define _GNX_WINDOW_H

#include <list>
#include <string>

#include <aplus/gshm.h>
#include <SDL2/SDL.h>


using namespace std;

#define WF_SHOW                 (0x01)
#define WF_NOPAINT              (0x02)


namespace GNX {
    class Window {
        public:
            string Title;
            uint16_t Width;
            uint16_t Height;
            uint16_t X;
            uint16_t Y;
            uint32_t ColorKey;
            int flags;
            
            SDL_Surface* Context;
            list<Window*>* Childs;
            Window* Parent;

            
            Window();
            Window(Window* parent, string title, uint16_t w, uint16_t h);
            ~Window();
      
            void Paint();
            void Paint(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
          
        protected:
            void resizeWindow(uint16_t w, uint16_t h);
    };
}

#endif