#include <gnx/Window.h>
#include <gnx/Screen.h>

#include <aplus/base.h>

using namespace std;
using namespace GNX;

static list<SDL_Rect*>* splitRectangles(SDL_Rect* r1, SDL_Rect* r2) {
    list<Rectangle*>* ll = new list<Rectangle*>();
    Rectangle* tmp = r1;

    if(cr->X >= tmp->X && cr->X <= tmp->Right()) {
        ll->push_back(new Rectangle(tmp->X, tmp->Y, tmp->X - cr->X - 1, tmp->Height));
        tmp->X = cr->X;
    }

    if(cr->Y >= tmp->Y && cr->Y <= tmp->Bottom()) {
        ll->push_back(new Rectangle(tmp->X, tmp->Y, tmp->Width, tmp->Y - cr->Y - 1));
        tmp->Y = cr->Y;
    }

    if(cr->Right() >= tmp->X && cr->Right() <= tmp->Right()) {
        ll->push_back(new Rectangle(cr->Right() + 1, tmp->Y, tmp->Width, tmp->Height));
        tmp->Width = cr->Width;
    }
    if(cr->Bottom() >= tmp->Y && cr->Bottom() <= tmp->Bottom()) {
        ll->push_back(new Rectangle(tmp->X, cr->Bottom() + 1, tmp->Width, tmp->Height));
        tmp->Height = cr->Height;
    }

    delete tmp;
    return ll;
}


WindowContext::WindowContext(uint16_t w, uint16_t h, void* data) {
    this->ClipRects = new list<SDL_Rect*> ();
    if(unlikely(!this->ClipRects)) {
        fprintf(stderr, "%s::%s (%d): out of memory\n", __FILE__, __func__, __LINE__);
        exit(-1);
    }

    this->Surface = SDL_CreateRGBSurfaceFrom (
        data, w, h, Screen::Bpp, 
        w * (Screen::Bpp >> 3),
        0x000000FF,
        0x0000FF00,
        0x00FF0000,
        0xFF000000
    );

    if(unlikely(!this->Surface)) {
        fprintf(stderr, "%s::%s (%d): %s\n", __FILE__, __func__, __LINE__, SDL_GetError());
        exit(-1);
    }


    this->X =
    this->Y =
    this->ColorKey = 0;
    this->Width = w;
    this->Height = h; 

    SDL_SetSurfaceBlendMode(this->Surface, SDL_BLENDMODE_NONE);
    SDL_SetColorKey(this->Surface, this->ColorKey ? SDL_TRUE : SDL_FALSE, this->ColorKey);
    
    /* DEBUG */
    SDL_FillRect(this->Surface, NULL, SDL_MapRGB(this->Surface->format, rand() % 255, rand() % 255, rand() % 255));
    
}

WindowContext::~WindowContext() {
    delete this->ClipRects;

    gfree(this->Surface->pixels);
    SDL_FreeSurface(this->Surface);
}


void WindowContext::IntersectClipRect(SDL_Rect* r) {

    list<SDL_Rect*>* pp = new list<SDL_Rect*> ();
    for(auto i = this->ClipRects->begin(); i != this->ClipRects->end(); i++) {
        SDL_Rect j;
        if(SDL_IntersectRect((*i), r, &j)) {
            SDL_Rect* k = new SDL_Rect();
            memcpy(k, &j, sizeof(j));

            pp->push_back(k);
        }
    }

    delete this->ClipRects;
    this->ClipRects = pp;
}

void WindowContext::SubtractClipRect(SDL_Rect* r) {

    int n = 0;
    do {
        list<SDL_Rect*>* pp = new list<SDL_Rect*> ();
        for(auto i = this->ClipRects->begin(); i != this->ClipRects->end();) {
            n = 0;

            if(!SDL_HasIntersection((*i), r)) {
                pp->push_back((*i));
                i++;
                continue;
            }
            
            list<SDL_Rect*>* splitRects = splitRectangles((*i), r);
            for(auto j = splitRects.begin(); j != splitRects.end(); j++)
                pp->push_back((*j));

            delete splitRects;
            n++;
            break;
        }

        delete this->clipRects;
        this->clipRects = pp;
    } while(n);

}



Window::Window() {
    this->Childs = new list<Window*> ();
    this->flags = 0;
}

Window::Window(Window* parent, string title, uint16_t w, uint16_t h) 
    : Window() {
    this->Parent = parent;
    this->Title = title;
    this->flags = WF_SHOW;


    if(this->Parent)
        this->Parent->Childs->push_back(this);

    this->Context = NULL;                   
    this->resizeWindow(w, h);
}

Window::~Window() {
    delete this->Childs;
    delete this->Context;
    
    if(this->Parent)
        this->Parent->Childs->remove(this);
}

void Window::resizeWindow(uint16_t w, uint16_t h) {
    if(likely(this->Context))
        delete this->Context;
    
    this->Context = new WindowContext(w, h, (void*) galloc(w * h * (Screen::Bpp >> 3)));   
}


void Window::Paint() {
    this->Paint(0, 0, this->Context->Width, this->Context->Height);
}

void Window::Paint(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if(unlikely(!(this->flags & WF_SHOW)))
        return;

    #if 0
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
    #endif
}