#ifndef _GNX_H
#define _GNX_H

#include <list>

using namespace std;

namespace GNX {
    
    class Rectangle {
        public:
            int X;
            int Y;
            int Width;
            int Height;
    };
    
    class Surface {
        public:
            Surface(uint16_t width, uint16_t height);
            Surface(uint16_t width, uint16_t height, void* buffer);
            ~Surface();
            
            uint16_t X;
            uint16_t Y;
            uint16_t Width;
            uint16_t Height;
            uint32_t Stride;
            void* Buffer;
    };
    
    class Window {
        public:
            Window();
            ~Window();
            
            string Title;
            Surface* surface;
            
            list<Window*> Childs;
            Window* Parent;
    };
}

#endif