#ifndef N_BRICK_H
#define N_BRICK_H
#include "brick.h"
namespace arkanoid {

    class NormalBrick: public Brick {

        public:
        NormalBrick(int y_pos, int x_pos);
    };
}
#endif