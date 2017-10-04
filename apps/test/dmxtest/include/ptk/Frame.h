#pragma once
#ifndef __cplusplus
#error "C++ Required!"
#endif

#include <iostream>
#include <functional>
#include <cairo/cairo.h>

using namespace std;

namespace ptk {
    class Frame {
    public:
        cairo_t* Fx;
        cairo_surface_t* Surface;
        cairo_format_t Format;

        int Locked;
        double Width;
        double Height;

        uint8_t BytesPerPixel;
        uint8_t BitsPerPixel;
        uint32_t Stride;
        void* Buffer;

        
        Frame(cairo_t* fx, cairo_surface_t* surface) :
            Fx(fx), Surface(surface) {

                this->Width = (double) cairo_image_surface_get_width(surface);
                this->Height = (double) cairo_image_surface_get_height(surface);
                
                switch((this->Format = cairo_image_surface_get_format(surface))) {
                    case CAIRO_FORMAT_RGB24:
                        this->BytesPerPixel = 3;
                        break;
                    case CAIRO_FORMAT_RGB16_565:
                        this->BytesPerPixel = 2;
                        break;
                    case CAIRO_FORMAT_ARGB32:
                    default:
                        this->BytesPerPixel = 4;
                        break;  
                }

                this->BitsPerPixel = this->BytesPerPixel * 8;
                this->Stride = (uint32_t) cairo_image_surface_get_stride(surface);
                this->Buffer = (void*) cairo_image_surface_get_data(surface);
                this->Locked = 0;
        }

        Frame(void* buffer, uint16_t width, uint16_t height, uint8_t bitsperpixel) :
            Buffer(buffer), Width(width), Height(height), BitsPerPixel(bitsperpixel),
            BytesPerPixel(bitsperpixel / 8), Stride(width * (bitsperpixel / 8)) {

            switch(bitsperpixel) {
                case 16:
                    this->Format = CAIRO_FORMAT_RGB16_565;
                    break;
                case 24:
                    this->Format = CAIRO_FORMAT_RGB24;
                    break;
                case 32:
                default:
                    this->Format = CAIRO_FORMAT_ARGB32;
                    break;
            }

            this->Surface = cairo_image_surface_create_for_data (
                (unsigned char*) buffer,
                this->Format,
                width,
                height,
                width * (bitsperpixel / 8)
            );

            this->Fx = cairo_create(this->Surface);
            this->Locked = 0;            
        }

        Frame(uint16_t width, uint16_t height, uint8_t bitsperpixel) :
            Width(width), Height(height), BitsPerPixel(bitsperpixel),
            BytesPerPixel(bitsperpixel / 8), Stride(width * (bitsperpixel / 8)) {

            
            switch(bitsperpixel) {
                case 16:
                    this->Format = CAIRO_FORMAT_RGB16_565;
                    break;
                case 24:
                    this->Format = CAIRO_FORMAT_RGB24;
                    break;
                case 32:
                default:
                    this->Format = CAIRO_FORMAT_ARGB32;
                    break;
            }

            this->Surface = cairo_image_surface_create (
                this->Format,
                width,
                height
            );

            this->Fx = cairo_create(this->Surface);
            this->Buffer = (void*) cairo_image_surface_get_data(this->Surface);
            this->Locked = 0;           
        }

        ~Frame() {
            cairo_surface_destroy(this->Surface);
            cairo_destroy(this->Fx);
        }


        inline Frame* Clone() {
            if(this->Locked)
                return NULL;

            return new Frame (
                cairo_reference(this->Fx),
                cairo_surface_create_similar_image (
                    this->Surface, 
                    this->Format, 
                    (int) this->Width, 
                    (int) this->Height
                )
            );
        }

        inline cairo_t* LockSurface(double cx, double cy, double cw, double ch) {
            if(this->Locked)
                return NULL;

            this->Locked++;
            cairo_surface_t* s = cairo_surface_create_for_rectangle(this->Surface, cx, cy, cw, ch);
            if(!s)
                return NULL;

            return cairo_create(s);
        }

        inline void UnlockSurface(cairo_t* data) {
            cairo_surface_destroy(cairo_get_target(data));
            cairo_destroy(data);

            this->Locked = 0;
        }

        inline void Paint(function<void (cairo_t*)> OnPaintHandler = NULL) {
            if(OnPaintHandler)
                OnPaintHandler(this->Fx);

            /* TODO */
        }
    };
}
