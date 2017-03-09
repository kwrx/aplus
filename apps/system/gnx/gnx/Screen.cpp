#include <gnx/Screen.h>

using namespace GNX;

int Screen::Width = 800;
int Screen::Height = 600;
int Screen::Bpp = 32;
int Screen::Stride = 3200;
void* Screen::FrameBuffer = NULL;


Window* Screen::CreateWindow() {
    Window* W = new Window();
    W->Title = "Desktop";
    W->Parent = NULL;
    W->flags = WF_SHOW;
    
    W->Width = Screen::Width;
    W->Height = Screen::Height;
    W->X =
    W->Y = 0;
    
    W->Context = SDL_CreateRGBSurfaceFrom (
        Screen::FrameBuffer,
        Screen::Width, Screen::Height, Screen::Bpp, 
        Screen::Stride,
        0x000000FF,
        0x0000FF00,
        0x00FF0000,
        0xFF000000
    );

    
    if(unlikely(!W->Context)) {
        fprintf(stderr, "%s::%s (%d): %s\n", __FILE__, __func__, __LINE__, SDL_GetError());
        exit(-1);
    }
    
    SDL_SetSurfaceBlendMode(W->Context, SDL_BLENDMODE_NONE);
    SDL_SetColorKey(W->Context, SDL_FALSE, W->ColorKey);
    
    return W;
}