#ifndef BRICK_H
#define BRICK_H
#include "drawable.h"
namespace arkanoid {

    class Brick: public Drawable {

        public:
        static const unsigned height = 3;
        static const unsigned width = 6;
        
        Brick(int y_pos, int x_pos, int color);
    };
}
#endif