#ifndef _GNX_H
#define _GNX_H

#include <list>
#include <aplus/gshm.h>

using namespace std;


namespace GNX {
    
    class Screen {
        public:
            static int Width;
            static int Height;
            static int Bpp;
            static int Stride;
            static void* FrameBuffer;
    };
    
    class Rectangle {
        public:
            int X;
            int Y;
            int Width;
            int Height;
            
            Rectangle(int x, int y, int w, int h)
                : X(x), Y(y), Width(w), Height(h) { }
            
            inline int Right() {
                return this->X + this->Width;
            }
            
            inline int Bottom() {
                return this->Y + this->Height;
            }
            
            inline Rectangle* Clone() {
                return new Rectangle(this->X, this->Y, this->Width, this->Height);
            }
            
            
            
            inline list<Rectangle*>* Split(Rectangle* cr) {
                list<Rectangle*>* ll = new list<Rectangle*>();
                Rectangle* tmp = this->Clone();
                
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
            
            inline void AddClip(list<Rectangle*>* cl) {
                for(int i = 0; i < cl->size();) {
                    auto it = cl->begin();
                    advance(it, i);
                    
                    
                    if(
                        (*it)->X <= this->Right()     &&
                        (*it)->Right() >= this->X     &&
                        (*it)->Y <= this->Bottom()    &&
                        (*it)->Bottom() >= this->Y
                    ) {
                        i++;
                        continue;
                    }
                    
                    list<Rectangle*>* ll = (*it)->Split(this);
                    cl->remove((*it));
                    cl->merge(*ll);
                    
                    delete ll;
                    i = 0;
                }
                
                cl->push_back(this);
            }
            
           inline void Draw(void (*plot) (int x, int y)) {
                for(int x = 0; x < this->Width; x++) {
                    plot(this->X + x, this->Y);
                    plot(this->X + x, this->Y + this->Height);
                }
                
                for(int y = 0; x < this->Height; y++) {
                    plot(this->X, this->Y + y);
                    plot(this->X + this->Width, this->Y + y);
                }
           }
    };
    
    class Surface {
        public:
            Surface(uint16_t width, uint16_t height)
                : X(0), Y(0), Width(width), Height(height) {
                    
                this->Stride = width * (Screen::Bpp >> 3);
                this->ClipRectangles = new list<Rectangle*>();
                this->Buffer = (void*) gnew char[this->Stride * height];
            }
            
            
            ~Surface() {
                delete this->ClipRectangles;
                gfree(this->Buffer);
            }
            
            uint16_t X;
            uint16_t Y;
            uint16_t Width;
            uint16_t Height;
            uint32_t Stride;
            void* Buffer;
            
            list<Rectangle*>* ClipRectangles;
    };
    
    class Window {
        public:
            Window(Window* parent, string title, int w, int h) {
                this->Title = title;
                this->Context = new Surface(w, h);
                this->Childs = new list<Window*> ();
                this->Parent = parent;
                
                if(this->Parent)
                    this->Parent->Childs->push_back(this);
            }
            
            ~Window() {
                delete this->Context;
                delete this->Childs;
                
                if(this->Parent)
                    this->Parent->Childs->remove(this);
            }
            
            string Title;
            Surface* Context;
            
            list<Window*>* Childs;
            Window* Parent;
    };
}

#endif