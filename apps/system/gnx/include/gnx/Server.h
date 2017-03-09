#ifndef _GNX_SERVER_H
#define _GNX_SERVER_H

#include <list>
#include <gnx/Window.h>


#define GNX_CURSOR_ARROW            0
#define GNX_CURSOR_CROSS            1
#define GNX_CURSOR_FORBIDDEN        2
#define GNX_CURSOR_HELP             3
#define GNX_CURSOR_PENCIL           4
#define GNX_CURSOR_HAND             5
#define GNX_CURSOR_SIZE_ALL         6
#define GNX_CURSOR_SIZE_BDIAG       7
#define GNX_CURSOR_SIZE_FDIAG       8
#define GNX_CURSOR_SIZE_HOR         9
#define GNX_CURSOR_SIZE_VER         10
#define GNX_CURSOR_TEXT             11
#define GNX_CURSOR_UPARROW          12
#define GNX_CURSORS_LENGTH          13



using namespace std;

namespace GNX {
    class Server {
        public:
            Window* Desktop;
            Window* FocusWindow;

            int CursorIndex;
            SDL_Surface* Cursor[GNX_CURSORS_LENGTH];
            
            Server(uint16_t w, uint16_t h, uint16_t bpp, void* lfb);
            ~Server();
            
            void Initialize();
            void Run();
            void Close();

            void HandleKeyboard(uint8_t vkey);
            void HandleMouse(mouse_t* e);
    };
}

#endif