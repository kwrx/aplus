#include <gnx/Window.h>
#include <gnx/Screen.h>

#include <aplus/base.h>

using namespace std;
using namespace GNX;


Window::Window() {
    this->Childs = new list<Window*> ();
    this->ColorKey = 0;
    this->flags = 0;
    this->X =
    this->Y =
    this->Width =
    this->Height = 0;
}

Window::Window(Window* parent, string title, uint16_t w, uint16_t h) 
    : Window() {
    this->Title = title;
    this->Parent = parent;
    this->flags = WF_SHOW;
    
    if(this->Parent)
        this->Parent->Childs->push_back(this);


    this->Context = NULL;                   
    this->resizeWindow(w, h);
}

Window::~Window() {
    delete this->Childs;
    
    if(this->Parent)
        this->Parent->Childs->remove(this);
}

void Window::resizeWindow(uint16_t w, uint16_t h) {
    if(likely(this->Context)) {
        fprintf(stderr, "%p\n", this->Context);
        gfree(this->Context->pixels);
        SDL_FreeSurface(this->Context);
    }
    
    
    this->Context = SDL_CreateRGBSurfaceFrom (
        (void*) galloc(w * h * (Screen::Bpp >> 3)),
        w, h, Screen::Bpp, 
        w * (Screen::Bpp >> 3),
        0x000000FF,
        0x0000FF00,
        0x00FF0000,
        0xFF000000
    );

    
    if(unlikely(!this->Context)) {
        fprintf(stderr, "%s::%s (%d): %s\n", __FILE__, __func__, __LINE__, SDL_GetError());
        exit(-1);
    }
    
    SDL_SetSurfaceBlendMode(this->Context, SDL_BLENDMODE_NONE);
    SDL_SetColorKey(this->Context, this->ColorKey ? SDL_TRUE : SDL_FALSE, this->ColorKey);
    
    this->Width = w;
    this->Height = h;
    
    /* DEBUG */
    SDL_FillRect(this->Context, NULL, SDL_MapRGB(this->Context->format, rand() % 255, rand() % 255, rand() % 255));
}


void Window::Paint() {
    this->Paint(0, 0, this->Width, this->Height);
}

void Window::Paint(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if(unlikely(!(this->flags & WF_SHOW)))
        return;

    
    SDL_Rect rs;
    rs.x = x;
    rs.y = y;
    rs.w = w;
    rs.h = h;
    
    SDL_Rect rd;
    rd.x = x + this->X;
    rd.y = y + this->Y;
    rd.w = w;
    rd.h = h;
    
    if(likely(this->Parent))
        SDL_BlitSurface(this->Context, &rs, this->Parent->Context, &rd);
    
    
    for(auto i = this->Childs->begin(); i != this->Childs->end(); i++) {
        SDL_Rect rc;
        rc.x = (*i)->X;
        rc.y = (*i)->Y;
        rc.w = (*i)->Width;
        rc.h = (*i)->Height;
        
        SDL_Rect ri;
        if(SDL_IntersectRect(&rs, &rc, &ri))
            (*i)->Paint(ri.x, ri.y, ri.w, ri.h);
    }
}