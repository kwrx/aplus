#pragma once
#ifndef __cplusplus
#error "C++ Required!"
#endif

#include <iostream>
#include <functional>
#include <ptk/Frame.h>
#include <ptk/Font.h>
#include <ptk/Color.h>

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>

using namespace std;

namespace ptk {

    class Window;

    typedef function<void (Window*)> OnCreateHandler;
    typedef function<void (Window*)> OnCloseHandler;
    typedef function<void (Window*, double, double)> OnResizeHandler;
    typedef function<void (Window*, double, double)> OnMoveHandler;
    typedef function<void (Window*, cairo_t*)> OnPaintHandler;
    //typedef function<void (Window*, KeyEventArgs*> OnKeyDownHandler;
    // ...


    class WindowStyle {
    public:
        static double MarginLeft;
        static double MarginTop;
        static double MarginRight;
        static double MarginBottom;
        static double BorderRadius;

        static void DrawBorders(Window*);
    };

    class Window {
    public:

        /* Attributes */
        string Title;
        double X;
        double Y;
        double Width;
        double Height;
        Color* BackColor;
        Color* ForeColor;

        bool MaximizeBox;
        bool MinimizeBox;

        /* View Container */
        Frame* View;
        double Opacity;

        /* Events */
        OnCreateHandler OnCreate;
        OnCloseHandler OnClose;
        OnResizeHandler OnResize;
        OnMoveHandler OnMove;
        OnPaintHandler OnPaint;

        inline Window() {
            this->BackColor = &Color::White;
            this->ForeColor = &Color::Black;
            this->Title = "";
            this->MaximizeBox = true;
            this->MinimizeBox = true;
            this->Opacity = 1.0;
        }

        /* Functions */
        inline void Invalidate(void) {
            if(this->View) {
                this->View->Paint([this] (auto cr) {
                    WindowStyle::DrawBorders(this);

                    cairo_t* cx = this->View->LockSurface(WindowStyle::MarginLeft, WindowStyle::MarginTop, this->Width - WindowStyle::MarginRight, this->Height - WindowStyle::MarginBottom);
                    if(!cx)
                        return;

                    if(this->OnPaint)
                        this->OnPaint(this, cx);

                    this->View->UnlockSurface(cx);
                });
            }
        }

        inline void Show() {
            if(this->OnCreate)
                this->OnCreate(this);

            this->Invalidate();
        }

    private:
        inline void init(void) {
            
        }
    };
}