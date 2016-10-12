#include <iostream>
#include <list>
#include <aplus/gnx.h>

using namespace std;


void GnxWindow::Paint(GnxRect* ARect) {
    for(auto i = this->clipRects->begin(); i != this->clipRects->end(); i++) {
        if(!(
            (*i)->Left <= ARect->Right &&
            (*i)->Right >= ARect->Left &&
            (*i)->Top <= ARect->Bottom &&
            (*i)->Bottom >= ARect->Top
        )) continue;
        
        list<GnxRect*>* splitRects = (*i)->Split(ARect);
        this->clipRects->remove(*i);
        
        for(auto j = splitRects->begin(); j != splitRects->end(); j++)
            this->clipRects->push_back(*j);
        
        splitRects->clear();
        delete splitRects;
    }
}

void GnxWindow::Paint() {
    GnxRect R;
    R.Top = 0;
    R.Left = 0;
    R.Bottom = this->Height;
    R.Right = this->Width;
    
    return this->Paint(&R);
}