#ifndef _GNXSERVER_H
#define _GNXSERVER_H

#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include <aplus/base.h>
#include <aplus/fbdev.h>
#include <aplus/gnx.h>
#include <aplus/sysconfig.h>

using namespace std;

class WindowServer {
    public:
        struct {
            uint16_t Width;
            uint16_t Height;
        } Context;

        pid_t Hwnd;
        WindowServer* Parent;
        list<WindowServer*>* Childs;
    
        WindowServer(WindowServer* p, uint16_t w, uint16_t h);
}

class Server {
    public:
        struct {
            uint16_t Width;
            uint16_t Height;
            uint16_t Bpp;
            uint32_t Pitch;
            uintptr_t Framebuffer; 
        } Screen;


        WindowServer* Desktop;
        EventDispacher* EvController;

        Server(uint16_t w, uint16_t h, uint16_t bpp, uintptr_t lfbptr);
        
        void Run();
};

#endif
