#ifndef _GNX_H
#define _GNX_H


#ifndef __cplusplus
#error "Only C++ supported for GNX"
#endif


#include <iostream>
#include <vector>

using namespace std;



struct GnxWindow {
    public:
        string Title;
        uint16_t X;
        uint16_t Y;
        uint16_t Width;
        uint16_t Height;

        GnxWindow* Parent;
        vector<GnxWindow*>* Childrens;
}

struct GnxDisplay {
    public:
        uint16_t Width;
        uint16_t Height;
        uint16_t Bpp;
        uint32_t Pitch;
        void* Pixels;
        void* BackBuffer;

        GnxWindow* RootWindow;
};


class Gnx {
    public:
        vector<GnxDisplay*>* Displays;
        GnxDisplay* CurrentDisplay;

        Gnx();
        ~Gnx();

        GnxDisplay* OpenDisplay(const char* name);
        GnxDisplay* OpenDisplay(uint16_t w, uint16_t h, uint16_t b, void* data);
};



