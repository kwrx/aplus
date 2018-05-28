#include "../include/brick.h"

arkanoid::Brick::Brick(int y_pos, int x_pos, int color):Drawable('#', y_pos, x_pos, (y_pos + height) - 1, (x_pos + width) - 1, color){

}