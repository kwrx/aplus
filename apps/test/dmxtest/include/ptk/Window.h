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
                    /* TODO: Add WindowTheme::DrawBorders() */

                    double b = 8.0;
                    double w = 800.0;
                    double h = 600.0;
                    cairo_save(cr);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    cairo_set_line_width(cr, b);
                    cairo_rectangle(cr, b / 2, b / 2, w - b * 1.5, h - b * 1.5);
                    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
                    cairo_stroke(cr);
                    cairo_rectangle(cr, b / 2, b / 2, w- b * 1.5, 24.0);
                    cairo_fill(cr);
                    cairo_set_font_face(cr, Font::Load("Ubuntu Regular"));
                    cairo_set_font_size(cr, 16.0);
                    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
                    cairo_move_to(cr, b / 2 + 15, b / 2 + 15);
                    cairo_show_text(cr, "Hello World");
                    cairo_restore(cr);

                    /* TODO: Add WindowTheme */
                    cairo_t* cx = this->View->LockSurface(6.0, 6.0, this->Width - 12.0, this->Height - 12.0);
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