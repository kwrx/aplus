#include <iostream>
#include <list>
#include <aplus/gnx.h>

using namespace std;

GnxRect::GnxRect() {
    this->Top = 0;
    this->Left = 0;
    this->Bottom = 0;
    this->Right = 0;
}

GnxRect::GnxRect(int Top, int Left, int Bottom, int Right) {
    this->Top = Top;
    this->Left = Left;
    this->Bottom = Bottom;
    this->Right = Right;
}

list<GnxRect*>* GnxRect::Split(GnxRect* CuttingRect) {
    list<GnxRect*>* op = new list<GnxRect*>();
    GnxRect* tmp = new GnxRect();
    
    tmp->Top = this->Top;
    tmp->Bottom = this->Bottom;
    tmp->Left = this->Left;
    tmp->Right = this->Right;
    
    if(
        CuttingRect->Left >= tmp->Left &&
        CuttingRect->Left <= tmp->Right
    ) {
        op->push_back(new GnxRect(tmp->Top, tmp->Left, tmp->Bottom, CuttingRect->Left - 1));
        tmp->Left = CuttingRect->Left;
    }
    
    if(
        CuttingRect->Top >= tmp->Top &&
        CuttingRect->Top <= tmp->Bottom
    ) {
        op->push_back(new GnxRect(tmp->Top, tmp->Left, CuttingRect->Top - 1, tmp->Right));
        tmp->Top = CuttingRect->Top;
    }
    
    if(
        CuttingRect->Right >= tmp->Left &&
        CuttingRect->Right <= tmp->Right
    ) {
        op->push_back(new GnxRect(tmp->Top, CuttingRect->Right + 1, tmp->Bottom, tmp->Right));
        tmp->Right = CuttingRect->Right;
    }
    
    if(
        CuttingRect->Bottom >= tmp->Top &&
        CuttingRect->Bottom <= tmp->Bottom
    ) {
        op->push_back(new GnxRect(CuttingRect->Bottom + 1, tmp->Left, tmp->Bottom, tmp->Right));
        tmp->Bottom = CuttingRect->Bottom;
    }
    
    delete tmp;
    return op;
}