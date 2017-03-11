#include <iostream>
#include <list>

#include <aplus/base.h>
#include <aplus/input.h>

#include <gnx/Server.h>
#include <gnx/Window.h>
#include <gnx/Screen.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <unistd.h>
#include <sys/sched.h>


using namespace std;
using namespace GNX;


extern "C" int clone(void* (*fn)(void*), void*, int, void*);

static void* threadKeyboard(void* arg) {
    Server* gnx = (Server*) arg;

    for(;;)
        gnx->Input->PollKeyboard([&] (uint8_t e) { 
            gnx->HandleKeyboard(e);
        });
}

static void* threadMouse(void* arg) {
    Server* gnx = (Server*) arg;

    for(;;)
        gnx->Input->PollMouse([&] (mouse_t* e) {
            gnx->HandleMouse(e);
        });
}



Server::Server(uint16_t w, uint16_t h, uint16_t bpp, void* lfb) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();

    Screen::Width = w;
    Screen::Height = h;
    Screen::Bpp = bpp;
    Screen::Stride = w * (bpp >> 3);
    Screen::FrameBuffer = lfb;

    this->Desktop = Screen::CreateWindow();
    this->FocusWindow = NULL;
    this->CursorIndex = 0;
    this->Input = new InputController();
}

Server::~Server() {
    delete this->Desktop;
}


void Server::Initialize() {
    this->Cursor[GNX_CURSOR_ARROW] =        IMG_Load(PATH_CURSORS "/arrow.cur");
    this->Cursor[GNX_CURSOR_FORBIDDEN] =    IMG_Load(PATH_CURSORS "/forbidden.cur");
    this->Cursor[GNX_CURSOR_HELP] =         IMG_Load(PATH_CURSORS "/help.cur");
    this->Cursor[GNX_CURSOR_PENCIL] =       IMG_Load(PATH_CURSORS "/pencil.cur");
    this->Cursor[GNX_CURSOR_HAND] =         IMG_Load(PATH_CURSORS "/hand.cur");
    this->Cursor[GNX_CURSOR_SIZE_ALL] =     IMG_Load(PATH_CURSORS "/size_all.cur");
    this->Cursor[GNX_CURSOR_SIZE_BDIAG] =   IMG_Load(PATH_CURSORS "/size_bdiag.cur");
    this->Cursor[GNX_CURSOR_SIZE_FDIAG] =   IMG_Load(PATH_CURSORS "/size_fdiag.cur");
    this->Cursor[GNX_CURSOR_SIZE_HOR] =     IMG_Load(PATH_CURSORS "/size_hor.cur");
    this->Cursor[GNX_CURSOR_SIZE_VER] =     IMG_Load(PATH_CURSORS "/size_ver.cur");
    this->Cursor[GNX_CURSOR_TEXT] =         IMG_Load(PATH_CURSORS "/text.cur");
    this->Cursor[GNX_CURSOR_UPARROW] =      IMG_Load(PATH_CURSORS "/up_arrow.cur");

    this->CursorIndex = GNX_CURSOR_ARROW;


    clone(threadKeyboard, NULL, CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_SIGHAND, this);
    clone(threadMouse, NULL, CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_SIGHAND, this);
}

void Server::Run() {
    for(;;)
        sched_yield();
}

void Server::Close() {

}


void Server::HandleKeyboard(uint8_t vkey) {

}


void Server::HandleMouse(mouse_t* e) {
    static SDL_Rect r = { 0, 0, 32, 32 };
    this->Desktop->Paint(r.x, r.y, r.w, r.h);

    r.x = e->x;
    r.y = e->y;
    r.w = this->Cursor[this->CursorIndex]->w;
    r.h = this->Cursor[this->CursorIndex]->h;

    
    SDL_BlitSurface(this->Cursor[this->CursorIndex], NULL, this->Desktop->Context, &r);
}