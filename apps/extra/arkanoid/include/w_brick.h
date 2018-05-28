#ifndef W_BRICK_H
#define W_BRICK_H
#include "brick.h"
namespace arkanoid {

    class WinningBrick: public Brick {

        public:
        WinningBrick(int y_pos, int x_pos);
    };
}
#endif